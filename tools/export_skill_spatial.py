#!/usr/bin/env python3
"""Launch one real SkillLab world session and publish its spatial export."""
from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import secrets
import shutil
import signal
import struct
import subprocess
import sys
import time
import zlib
from datetime import datetime, timezone


REPO = Path(__file__).resolve().parents[1]
DEFAULT_PROJECT = REPO / "ue/SkillLab/SkillLab.uproject"
DEFAULT_ENGINE = Path("D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe")
SETTINGS_RELATIVE = Path("Saved/SpatialExportTool/settings.json")
PROTOCOL_VERSION = 1


class ExportError(ValueError):
    pass


class JsonArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        print(json.dumps({"status": "failed", "errors": [message]}, ensure_ascii=False))
        raise SystemExit(1)


def read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ExportError(f"无法读取 JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ExportError("请求 JSON 顶层必须是对象")
    return value


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + f".{secrets.token_hex(4)}.tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def absolute_path(value, field: str, *, must_exist: bool = False) -> Path:
    if not isinstance(value, str) or not value:
        raise ExportError(f"{field} 必须是绝对路径字符串")
    path = Path(value).expanduser()
    if not path.is_absolute():
        raise ExportError(f"{field} 必须是绝对路径")
    path = path.resolve()
    if must_exist and not path.exists():
        raise ExportError(f"{field} 不存在: {path}")
    return path


def finite_number(value, field: str, *, minimum=None, strict=False) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ExportError(f"{field} 必须是有限数字")
    if minimum is not None and (value <= minimum if strict else value < minimum):
        qualifier = "大于" if strict else "不小于"
        raise ExportError(f"{field} 必须{qualifier} {minimum}")
    return float(value)


def check_actor_locator(value, field: str) -> None:
    if not isinstance(value, dict) or len(set(value) & {"player_index", "level_actor_path"}) != 1:
        raise ExportError(f"{field} 必须且只能包含 player_index 或 level_actor_path")
    if "player_index" in value:
        index = value["player_index"]
        if isinstance(index, bool) or not isinstance(index, int) or index < 0:
            raise ExportError(f"{field}.player_index 必须是非负整数")
    else:
        if not isinstance(value["level_actor_path"], str) or not value["level_actor_path"].startswith("/"):
            raise ExportError(f"{field}.level_actor_path 必须是以 / 开头的 UE 对象路径")


def check_vector(value, field: str, length=3) -> list[float]:
    if not isinstance(value, list) or len(value) != length:
        raise ExportError(f"{field} 必须是包含 {length} 个数字的数组")
    return [finite_number(item, f"{field}[{index}]") for index, item in enumerate(value)]


def normalize_request(raw: dict, mode: str, project: Path) -> dict:
    request = dict(raw)
    if "project_path" in request:
        project = absolute_path(request["project_path"], "project_path", must_exist=True)
    if project.suffix.lower() != ".uproject" or not project.is_file():
        raise ExportError(f"project_path 必须指向现存 .uproject: {project}")
    if not isinstance(request.get("map"), str) or not request["map"].startswith("/"):
        raise ExportError("map 必须是以 / 开头的 UE 关卡资产路径")
    check_actor_locator(request.get("caster"), "caster")
    if mode == "capture":
        if not isinstance(request.get("skill_id"), str) or not request["skill_id"].strip():
            raise ExportError("capture 需要非空 skill_id")
    elif request.get("skill_id") is not None and not isinstance(request["skill_id"], str):
        raise ExportError("skill_id 必须是字符串")
    target = request.get("target")
    if target is not None:
        if not isinstance(target, dict) or len(target) != 1 or not ("actor" in target or "ground_location" in target):
            raise ExportError("target 只能包含 actor 或 ground_location")
        if "actor" in target:
            check_actor_locator(target["actor"], "target.actor")
        else:
            check_vector(target["ground_location"], "target.ground_location")
    if "related_actor" in request:
        check_actor_locator(request["related_actor"], "related_actor")

    for key, default, minimum, strict in (("warmup_seconds", 1.0, 0, False),
                                          ("pre_request_seconds", 0.25, 0, False),
                                          ("duration_seconds", None, 0, True),
                                          ("sample_hz", 30.0, 0, True),
                                          ("timeout_seconds", 120.0, 0, True)):
        if mode == "inspect" and key == "duration_seconds":
            request[key] = 0.0
            continue
        if request.get(key) is None and default is None:
            raise ExportError(f"{key} 是必填字段")
        request[key] = finite_number(request.get(key, default), key, minimum=minimum, strict=strict)
    if mode == "inspect":
        request["duration_seconds"] = 0.0
        request["pre_request_seconds"] = 0.0
    views = request.get("views", [{"id": "game", "type": "game"}])
    if not isinstance(views, list) or not views:
        raise ExportError("views 必须是非空数组")
    ids = set()
    for index, view in enumerate(views):
        if not isinstance(view, dict) or not isinstance(view.get("id"), str) or not view["id"]:
            raise ExportError(f"views[{index}] 需要非空 id")
        if view["id"] in ids or Path(view["id"]).name != view["id"] or view["id"] in {".", ".."}:
            raise ExportError(f"views[{index}].id 必须唯一且不能包含路径分隔符")
        ids.add(view["id"])
        if view.get("type") == "game":
            continue
        if view.get("type") != "custom":
            raise ExportError(f"views[{index}].type 只支持 game/custom")
        check_vector(view.get("location_cm"), f"views[{index}].location_cm")
        check_vector(view.get("rotation_deg"), f"views[{index}].rotation_deg")
        fov = finite_number(view.get("fov_deg"), f"views[{index}].fov_deg", minimum=0, strict=True)
        if fov >= 180:
            raise ExportError(f"views[{index}].fov_deg 必须小于 180")
    request["views"] = views
    resolution = request.get("resolution", [1280, 720])
    if (not isinstance(resolution, list) or len(resolution) != 2
            or any(isinstance(v, bool) or not isinstance(v, int) or v <= 0 for v in resolution)):
        raise ExportError("resolution 必须是两个正整数 [width,height]")
    request["resolution"] = resolution
    if not isinstance(request.get("markers", False), bool):
        raise ExportError("markers 必须是布尔值")
    request.setdefault("markers", False)
    queries = request.get("reference_queries", [])
    if not isinstance(queries, list):
        raise ExportError("reference_queries 必须是数组")
    for index, query in enumerate(queries):
        if not isinstance(query, dict) or query.get("source") not in {"caster", "target", "related_actor"}:
            raise ExportError(f"reference_queries[{index}].source 无效")
        if query.get("kind") not in {"bone", "socket"} or not all(isinstance(query.get(k), str) and query[k] for k in ("component_path", "name")):
            raise ExportError(f"reference_queries[{index}] 需要 component_path、kind 和 name")
    request["reference_queries"] = queries
    request["project_path"] = str(project)
    known_runtime = request["warmup_seconds"] + request["pre_request_seconds"] + request["duration_seconds"]
    if request["timeout_seconds"] <= known_runtime:
        raise ExportError("timeout_seconds 必须长于 warmup、pre_request 和 capture duration 的已知时间预算")
    return request


def png_size(path: Path) -> tuple[int, int] | None:
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) < 45 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", data[16:24])
    offset, compressed, ended = 8, bytearray(), False
    try:
        while offset + 12 <= len(data):
            length = struct.unpack(">I", data[offset:offset + 4])[0]
            end = offset + 12 + length
            if end > len(data):
                return None
            kind = data[offset + 4:offset + 8]
            body = data[offset + 8:offset + 8 + length]
            crc = struct.unpack(">I", data[offset + 8 + length:end])[0]
            if zlib.crc32(kind + body) & 0xFFFFFFFF != crc:
                return None
            if kind == b"IDAT":
                compressed.extend(body)
            if kind == b"IEND":
                ended = end == len(data)
                break
            offset = end
        if not ended:
            return None
        pixels = zlib.decompress(compressed)
        bit_depth, color_type, interlace = data[24], data[25], data[28]
        channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color_type)
        if bit_depth not in {8, 16} or interlace != 0 or channels is None or len(pixels) < (width * channels * (bit_depth // 8) + 1) * height:
            return None
    except (ValueError, zlib.error, struct.error):
        return None
    return width, height


def read_jsonl(path: Path) -> tuple[list[dict], str | None]:
    try:
        records = []
        for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
            if not line.strip():
                continue
            value = json.loads(line)
            if not isinstance(value, dict):
                raise ValueError(f"第 {line_number} 行不是 JSON object")
            records.append(value)
        return records, None
    except (OSError, ValueError) as exc:
        return [], str(exc)


def finalize_manifest(output: Path, request: dict, mode: str, run_id: str,
                      process_code: int | None, errors: list[str]) -> dict:
    receipt_path = output / "ue_receipt.json"
    try:
        receipt = read_json(receipt_path) if receipt_path.is_file() else {}
    except ExportError as exc:
        receipt = {}
        errors.append(f"ue_receipt.json 无效: {exc}")
    files = []
    missing = []
    required = ["request.json", "request.original.json", "configuration.json", "references.json", "ue_receipt.json", "ue.log"]
    if mode == "capture":
        required += ["events.jsonl", "samples.jsonl"]
    for relative in required:
        if (output / relative).is_file():
            files.append(relative)
        else:
            missing.append(relative)
    if (output / "ue_phase.json").is_file():
        files.append("ue_phase.json")
    configuration = {}
    references = {}
    for relative, target in (("configuration.json", configuration), ("references.json", references)):
        path = output / relative
        if path.is_file():
            try:
                target.update(read_json(path))
            except ExportError as exc:
                errors.append(f"{relative} 无效: {exc}")
                missing.append(f"{relative}:valid_json")
    if not isinstance(configuration.get("granted_skills"), list):
        errors.append("configuration.json 缺少 granted_skills 数组")
        missing.append("configuration.json:granted_skills")
    runtime_identity = configuration.get("caster_runtime")
    if (not isinstance(runtime_identity, dict) or not runtime_identity.get("actor_path")
            or not runtime_identity.get("runtime_component_path") or not runtime_identity.get("generation")):
        errors.append("configuration.json 缺少稳定 caster/runtime/generation 身份")
        missing.append("configuration.json:caster_runtime")
    selected_skill = configuration.get("selected_skill_id")
    if mode == "capture" and (not isinstance(selected_skill, str) or selected_skill != request.get("skill_id")):
        errors.append("configuration.json selected_skill_id 与请求不匹配")
        missing.append("configuration.json:selected_skill_id")
    if mode == "capture" and isinstance(configuration.get("granted_skills"), list):
        granted_ids = {item.get("skill_id") for item in configuration["granted_skills"] if isinstance(item, dict)}
        if selected_skill not in granted_ids:
            errors.append("configuration.json selected_skill_id 不属于导出的有效授予定义")
            missing.append("configuration.json:selected_skill_grant")
    scene_preservation = configuration.get("scene_preservation")
    if mode == "capture" and (not isinstance(scene_preservation, dict)
            or not isinstance(scene_preservation.get("skill_lab_dummy_tag_count"), int)
            or not isinstance(scene_preservation.get("protected_actors"), list)):
        errors.append("configuration.json 缺少场景保真记录")
        missing.append("configuration.json:scene_preservation")
    elif mode == "capture":
        for actor in scene_preservation["protected_actors"]:
            if not isinstance(actor, dict) or not actor.get("actor_path") or "collision_enabled" not in actor or "hidden_in_game" not in actor:
                errors.append("configuration.json 场景保真 Actor 缺少路径、碰撞或可见状态")
                missing.append("configuration.json:scene_preservation_actor_state")
                break
    if not isinstance(references.get("caster_path"), str) or not references.get("caster_path"):
        errors.append("references.json 缺少 caster_path")
        missing.append("references.json:caster_path")
    if (not isinstance(references.get("actors"), list) or not references.get("actors")
            or not any(isinstance(actor, dict) and actor.get("role") == "caster" for actor in references["actors"])):
        errors.append("references.json 缺少 actors 数组或施法者记录")
        missing.append("references.json:actors")
    pngs = sorted(output.glob("frames/*/*.png")) if mode == "capture" else []
    marked_pngs = sorted(output.glob("marked/*/*.png")) if mode == "capture" else []
    valid_pngs = []
    for png in pngs:
        size = png_size(png)
        if size == tuple(request["resolution"]):
            valid_pngs.append(png.relative_to(output).as_posix())
        else:
            errors.append(f"PNG 缺失或尺寸不符: {png}")
    files.extend(valid_pngs)
    valid_marked = []
    for png in marked_pngs:
        if png_size(png) == tuple(request["resolution"]):
            relative = png.relative_to(output).as_posix()
            valid_marked.append(relative)
            files.append(relative)
        else:
            errors.append(f"标记 PNG 缺失或尺寸不符: {png}")
    if mode == "capture":
        event_records, event_error = read_jsonl(output / "events.jsonl") if (output / "events.jsonl").is_file() else ([], "events.jsonl 不存在")
        if event_error:
            errors.append(f"events.jsonl 无法解析: {event_error}")
        sample_records, sample_error = read_jsonl(output / "samples.jsonl") if (output / "samples.jsonl").is_file() else ([], "samples.jsonl 不存在")
        if sample_error:
            errors.append(f"samples.jsonl 无法解析: {sample_error}")
        if receipt.get("request_accepted") and not any(
                event.get("event") == "request_return" and event.get("accepted") is True
                for event in event_records):
            errors.append("已受理请求缺少 accepted request_return 事件")
        if receipt.get("request_accepted") and not any(
                event.get("event") == "ESkillLabSkillEvent::Started"
                and event.get("request_id", 0) > 0 and event.get("cast_id", 0) > 0
                and event.get("runtime_generation", 0) > 0
                for event in event_records):
            errors.append("已受理请求缺少含 Request/Cast/Runtime generation 的 Started 身份")
        if receipt.get("complete") and not receipt.get("request_accepted"):
            errors.append("UE receipt 声明 complete 但没有受理请求")
        views = request.get("views", [{"id": "game", "type": "game"}])
        expected_ids = {view["id"] for view in views}
        frame_ids = {Path(png).parent.name for png in valid_pngs}
        if frame_ids != expected_ids:
            errors.append(f"PNG 视角集合不符: expected={sorted(expected_ids)}, actual={sorted(frame_ids)}")
        if request.get("markers"):
            marked_ids = {Path(png).parent.name for png in valid_marked}
            if marked_ids != expected_ids:
                errors.append(f"标记 PNG 视角集合不符: expected={sorted(expected_ids)}, actual={sorted(marked_ids)}")
        expected_samples = {sample.get("sample_id") for sample in sample_records}
        image_sets = [("frames", valid_pngs)]
        if request.get("markers"):
            image_sets.append(("marked", valid_marked))
        for root, found in image_sets:
            for view_id in expected_ids:
                actual_samples = {Path(path).stem for path in found if Path(path).parent.name == view_id}
                if expected_samples != actual_samples:
                    errors.append(f"{root}/{view_id} 样本编号不匹配: expected={len(expected_samples)}, actual={len(actual_samples)}")
        for sample in sample_records:
            if (not isinstance(sample.get("world_time_seconds"), (int, float))
                    or "since_request_world_seconds" not in sample
                    or not sample.get("request_time_relation")
                    or not isinstance(sample.get("game_frame"), int)):
                errors.append(f"sample {sample.get('sample_id')} 缺少实际时间基准或 game frame 身份")
                break
            image_views = {view.get("view_id") for view in sample.get("views", []) if isinstance(view, dict)}
            if image_views != expected_ids:
                errors.append(f"sample {sample.get('sample_id')} 缺失视角: {sorted(image_views)}")
                break
            for view in sample.get("views", []):
                if (not view.get("render_capture_id") or not isinstance(view.get("game_frame"), int)
                        or not view.get("image")):
                    errors.append(f"sample {sample.get('sample_id')} 的视角缺少图像/渲染关联身份")
                    break
        for event in event_records:
            if event.get("event") in {"feedback_created", "feedback_removed", "projectile_created", "projectile_delivery", "projectile_removed"}:
                if (event.get("request_id", 0) <= 0 or event.get("cast_id", 0) <= 0
                        or event.get("runtime_generation", 0) <= 0 or not event.get("submission_key")):
                    errors.append(f"{event.get('event')} 缺少稳定来源关联键")
                    break
    if mode == "capture" and not valid_pngs:
        errors.append("没有可用 PNG；capture 要求真实 RHI 画面")
    if process_code not in (0, None):
        errors.append(f"UE 进程退出码: {process_code}")
    errors.extend(receipt.get("errors", []))
    diagnostic_lines = errors + [f"限制：{item}" for item in receipt.get("limitations", [])]
    diagnostic_path = output / "diagnostics.txt"
    diagnostic_path.write_text("\n".join(diagnostic_lines) + ("\n" if diagnostic_lines else ""), encoding="utf-8")
    files.append("diagnostics.txt")
    if errors or not receipt.get("complete") or missing or receipt.get("limitations"):
        status = "partial" if receipt.get("request_accepted") and valid_pngs else "failed"
    else:
        status = "complete"
    manifest = {
        "schema_version": PROTOCOL_VERSION,
        "run_id": run_id,
        "mode": mode,
        "status": status,
        "request": "request.json",
        "project": request["project_path"],
        "map": request["map"],
        "caster": request["caster"],
        "skill_id": request.get("skill_id"),
        "started_at": receipt.get("started_at"),
        "ended_at": datetime.now(timezone.utc).isoformat(),
        "coverage": receipt.get("coverage", {}),
        "phase": receipt.get("phase"),
        "failure_phase": receipt.get("failure_phase"),
        "not_generated": ["events.jsonl", "samples.jsonl", "frames/", "marked/"] if mode == "inspect" else [],
        "files": files,
        "missing": missing,
        "errors": errors,
        "limitations": receipt.get("limitations", []),
        "ue_exit_code": process_code,
    }
    write_json(output / "manifest.json", manifest)
    return manifest


def result_line(run_id: str | None, status: str, output: Path | None, errors: list[str]) -> dict:
    return {"run_id": run_id, "status": status,
            "output_dir": str(output) if output else None,
            "manifest_path": str(output / "manifest.json") if output else None,
            "errors": errors}


def configure(args) -> int:
    project = Path(args.project).resolve() if args.project else DEFAULT_PROJECT
    if not project.is_file() or project.suffix.lower() != ".uproject":
        raise ExportError(f"项目文件不存在或扩展名错误: {project}")
    output = absolute_path(args.output_root, "output-root")
    output.mkdir(parents=True, exist_ok=True)
    if not os.access(output, os.W_OK):
        raise ExportError(f"输出目录不可写: {output}")
    settings = project.parent / SETTINGS_RELATIVE
    write_json(settings, {"output_root": str(output)})
    print(json.dumps({"status": "complete", "project_path": str(project),
                      "settings_path": str(settings), "output_root": str(output)}, ensure_ascii=False))
    return 0


def process_tree_stop(process: subprocess.Popen) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    else:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            return
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)


def run(args) -> int:
    output = None
    run_id = None
    errors: list[str] = []
    process_code = None
    try:
        request_path = absolute_path(args.request, "request", must_exist=True)
        original_request_bytes = request_path.read_bytes()
        project = Path(args.project).resolve() if args.project else DEFAULT_PROJECT
        if args.engine:
            engine = absolute_path(args.engine, "engine_executable", must_exist=True)
        else:
            engine = DEFAULT_ENGINE
        request = normalize_request(read_json(request_path), args.mode, project)
        if "engine_executable" in request:
            engine = absolute_path(request["engine_executable"], "engine_executable", must_exist=True)
        if not engine.is_file():
            raise ExportError(f"未找到 UE 运行程序: {engine}; 请通过 --engine 指定")
        root = request.get("output_root")
        if root:
            output_root = absolute_path(root, "output_root")
        else:
            settings = request["project_path"]
            settings_path = Path(settings).parent / SETTINGS_RELATIVE
            saved = read_json(settings_path).get("output_root") if settings_path.is_file() else None
            output_root = Path(saved) if saved else Path(settings).parent / "Saved/SpatialExports"
        output_root.mkdir(parents=True, exist_ok=True)
        probe = output_root / f".spatial-export-write-test-{secrets.token_hex(4)}"
        try:
            probe.write_bytes(b"")
        except OSError as exc:
            raise ExportError(f"输出根目录不可写: {output_root}: {exc}") from exc
        finally:
            probe.unlink(missing_ok=True)
        run_id = datetime.now().strftime("%Y%m%dT%H%M%S") + "-" + secrets.token_hex(4)
        output = output_root.resolve() / run_id
        output.mkdir(parents=True, exist_ok=False)
        request["output_root"] = str(output_root.resolve())
        request["run_id"] = run_id
        request["mode"] = args.mode
        request["protocol_version"] = PROTOCOL_VERSION
        write_json(output / "request.json", request)
        (output / "request.original.json").write_bytes(original_request_bytes)
        write_json(output / "manifest.json", {"schema_version": PROTOCOL_VERSION, "run_id": run_id,
                                               "mode": args.mode, "status": "failed", "files": [],
                                               "errors": ["UE 会话尚未回执"]})
        log_path = output / "ue.log"
        command = [str(engine), str(request["project_path"]), request["map"], "-game", "-windowed", "-d3d11",
                   f"-ResX={request['resolution'][0]}", f"-ResY={request['resolution'][1]}",
                   "-nosplash", "-nop4", f"-SkillLabSpatialExportRequest={output / 'request.json'}",
                   f"-SkillLabSpatialExportOutput={output}", f"-SkillLabSpatialExportMode={args.mode}",
                   f"-SkillLabSpatialExportRunId={run_id}"]
        with log_path.open("wb") as log:
            kwargs = {"stdout": log, "stderr": subprocess.STDOUT, "cwd": Path(request["project_path"]).parent}
            if os.name == "nt":
                kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
            else:
                kwargs["start_new_session"] = True
            process = subprocess.Popen(command, **kwargs)
            try:
                process_code = process.wait(timeout=request["timeout_seconds"])
            except subprocess.TimeoutExpired:
                phase_path = output / "ue_phase.json"
                try:
                    phase = read_json(phase_path).get("phase", "unknown") if phase_path.is_file() else "unknown"
                except ExportError:
                    phase = "unavailable"
                errors.append(f"UE 墙钟超时：限制 {request['timeout_seconds']} 秒；最近阶段={phase}；进程树按本次 PID 收束")
                if os.name == "nt":
                    try:
                        process.send_signal(signal.CTRL_BREAK_EVENT)
                        process.wait(timeout=5)
                    except (OSError, subprocess.TimeoutExpired):
                        pass
                else:
                    try:
                        os.killpg(process.pid, signal.SIGTERM)
                        process.wait(timeout=5)
                    except (OSError, subprocess.TimeoutExpired):
                        pass
                process_tree_stop(process)
                process_code = process.wait(timeout=10)
        manifest = finalize_manifest(output, request, args.mode, run_id, process_code, errors)
        code = {"complete": 0, "partial": 2, "failed": 1}[manifest["status"]]
        print(json.dumps(result_line(run_id, manifest["status"], output, manifest["errors"]), ensure_ascii=False))
        return code
    except (ExportError, OSError, subprocess.SubprocessError, ValueError) as exc:
        errors.append(str(exc))
        if output is not None and run_id is not None:
            try:
                fallback = {"project_path": str(DEFAULT_PROJECT), "map": None, "caster": None}
                write_json(output / "diagnostics.txt", {"errors": errors})
                write_json(output / "manifest.json", {"schema_version": PROTOCOL_VERSION, "run_id": run_id,
                                                       "mode": args.mode, "status": "failed", "errors": errors})
            except OSError:
                pass
        print(json.dumps(result_line(run_id, "failed", output, errors), ensure_ascii=False))
        return 1


def main(argv=None) -> int:
    parser = JsonArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("inspect", "capture"):
        sub = commands.add_parser(name)
        sub.add_argument("--request", required=True)
        sub.add_argument("--project")
        sub.add_argument("--engine")
        sub.set_defaults(handler=run, mode=name)
    sub = commands.add_parser("configure")
    sub.add_argument("--output-root", required=True)
    sub.add_argument("--save-default", action="store_true", required=True)
    sub.add_argument("--project")
    sub.set_defaults(handler=configure)
    args = parser.parse_args(argv)
    try:
        return args.handler(args)
    except (ExportError, OSError) as exc:
        print(json.dumps({"status": "failed", "errors": [str(exc)]}, ensure_ascii=False))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
