"""只读 AnimSequence 总览调用、分页发布及回查。需要 Pillow；不修改 UE 资产。"""
from __future__ import annotations

import argparse
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import uuid

from PIL import Image, ImageDraw, ImageFont, ImageStat

REPO = Path(__file__).resolve().parents[1]
PROJECT = REPO / "ue/SkillLab/SkillLab.uproject"
EDITOR = Path("D:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe")
COLS, ROWS, HEADER, LABEL = 4, 3, 84, 32


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write(path, value):
    path = Path(path)
    temp = path.with_name(path.name + f".{uuid.uuid4().hex}.tmp")
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
    temp.replace(path)


def digest(path, algorithm="sha256"):
    h = hashlib.new(algorithm)
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def local_file(directory, relative):
    require(isinstance(relative, str) and relative, "产物相对路径不能为空")
    result = (directory / relative).resolve()
    require(result.is_relative_to(directory.resolve()) and result != directory.resolve(), f"产物路径越界: {relative}")
    return result


def check_inputs(m):
    require(len(m["inputs"]) >= 3, "缺少动画/Skeleton/Mesh 输入指纹")
    for item in m["inputs"]:
        require(not item["packageDirty"], f"输入包有未保存修改: {item['object']}")
        require(digest(item["file"], "md5").lower() == item["md5"].lower(), f"输入版本已变化: {item['file']}")
        for sidecar in item.get("sidecars", []):
            require(digest(sidecar["file"], "md5").lower() == sidecar["md5"].lower(), f"输入 sidecar 已变化: {sidecar['file']}")


def check_timeline(m):
    require(m["schemaVersion"] == 1, "不支持的 manifest 版本")
    timing = m["timing"]
    count, numerator, denominator = timing["sampleCount"], timing["numerator"], timing["denominator"]
    require(all(isinstance(v, int) and not isinstance(v, bool) for v in (count, numerator, denominator)), "帧表必须使用整数计数和有理帧率")
    require(count > 0 and numerator > 0 and denominator > 0, "无效帧表")
    require(timing["intervalCount"] == count - 1, "采样点数与间隔数不一致")
    require(len(m["frames"]) == count, "缺失或多余源帧")
    previous_revision, previous_identity = -1, -1
    for index, frame in enumerate(m["frames"]):
        require(frame["frame"] == index, f"帧索引重复或跳帧: {index}")
        expected = float(Fraction(index * denominator, numerator))
        require(math.isfinite(frame["seconds"]) and abs(frame["seconds"] - expected) < 1e-8, f"源时间不一致: {index}")
        if frame["status"] == "rendered":
            require(frame["boneRevision"] > previous_revision and frame["renderFrameIdentity"] > previous_identity,
                    f"GPU 帧身份或骨骼 revision 未推进: {index}")
            previous_revision, previous_identity = frame["boneRevision"], frame["renderFrameIdentity"]
    require(abs(m["frames"][-1]["seconds"] - timing["durationSeconds"]) < 1e-5, "末端采样点与时长不一致")


def preview_fingerprint(m):
    return hashlib.sha256(json.dumps(
        {"inputs": m["inputs"], "preview": m["preview"], "timing": m["timing"]}, sort_keys=True).encode()).hexdigest()


def decode(path, size=None):
    with Image.open(path) as source:
        source.load()
        require(source.format == "PNG", f"不是 PNG: {path}")
        require(size is None or source.size == tuple(size), f"图像尺寸不符: {path}")
        return source.convert("RGB")


def font(size=18):
    for candidate in ("C:/Windows/Fonts/consola.ttf", "DejaVuSansMono.ttf"):
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            pass
    return ImageFont.load_default(size=size)


def publish(directory):
    """framesReady 不是完成产物；全部 PNG 解码、分页及回读检查后才发布 complete。"""
    directory = Path(directory).resolve()
    m = read(directory / "manifest.json")
    require(m["status"] == "framesReady", f"不可发布状态: {m['status']}")
    current_frame, current_page = -1, -1
    try:
        check_timeline(m)
        check_inputs(m)
        require(Path(m["directory"]).resolve() == directory, "manifest 输出目录不一致")
        require(m["preview"]["rhi"] and "null" not in m["preview"]["rhi"].lower(), "NullRHI 不可发布图像")
        width, height = m["preview"]["width"], m["preview"]["height"]
        size = (width, height)
        pages = []
        for start in range(0, len(m["frames"]), COLS * ROWS):
            end = min(start + COLS * ROWS, len(m["frames"]))
            page_index = len(pages)
            current_page = page_index
            filename = f"page-{page_index:04d}.png"
            page_path = directory / filename
            require(not page_path.exists(), f"拒绝覆盖已有页: {page_path}")
            page = Image.new("RGB", (COLS * width, HEADER + ROWS * (height + LABEL)), "#18212b")
            draw = ImageDraw.Draw(page)
            draw.text((10, 4), m["asset"], font=font(15), fill="white")
            draw.text((10, 28), f"{m['batchId']} / {m['generationId'][:12]}", font=font(14), fill="#aabace")
            draw.text((10, 52), f"SOURCE | view={m['preview']['view']} | page={page_index} | frames={start}..{end - 1} | seconds=f*{m['timing']['denominator']}/{m['timing']['numerator']}", font=font(15), fill="white")
            for slot in range(COLS * ROWS):
                row, column = divmod(slot, COLS)
                x, y = column * width, HEADER + row * (height + LABEL)
                if start + slot >= end:
                    draw.text((x + 10, y + 20), "EMPTY - no source sample", font=font(), fill="#8999ab")
                    continue
                frame = m["frames"][start + slot]
                current_frame = frame["frame"]
                require(frame["status"] == "rendered", f"源帧尚未渲染: {frame['frame']}")
                image_path = local_file(directory, frame["image"])
                image = decode(image_path, size)
                # 仅筛掉全空/纯色回读，不把像素方差视为姿势或视觉验收。
                require(max(ImageStat.Stat(image).stddev) > 1, f"疑似空白/纯色帧: {frame['frame']}")
                page.paste(image, (x, y))
                draw.text((x + 8, y + height + 5), f"f{frame['frame']:06d}  {frame['seconds']:.6f}s", font=font(), fill="white")
                frame["sha256"] = digest(image_path)
                frame["page"] = page_index
                frame["row"], frame["column"] = row, column
                frame["pixelRect"] = [x, y, width, height]
            page.save(page_path, format="PNG")
            decode(page_path, page.size)
            pages.append({"page": page_index, "image": filename, "firstFrame": start,
                          "lastFrame": end - 1, "width": page.width, "height": page.height,
                          "sha256": digest(page_path)})
        m["pages"] = pages
        m["layout"] = {"columns": COLS, "rows": ROWS, "headerPixels": HEADER,
                       "labelPixels": LABEL, "indexBase": 0, "pageCount": len(pages)}
        m["inputPreviewFingerprintSha256"] = preview_fingerprint(m)
        check_inputs(m)
        m["status"] = "complete"
        validate_manifest(directory, m)
        write(directory / "manifest.json", m)
        return m
    except Exception as error:
        m["status"] = "failed"
        m["error"] = {"stage": "paginationOrIntegrity", "reason": str(error), "frame": current_frame, "page": current_page}
        write(directory / "manifest.json", m)
        raise


def validate_manifest(directory, m=None):
    directory = Path(directory).resolve()
    m = m or read(directory / "manifest.json")
    require(m["status"] == "complete", f"产物未完成: {m['status']}")
    require(Path(m["directory"]).resolve() == directory, "输出目录不一致")
    require(directory.is_relative_to(Path(m["outputRoot"]).resolve()), "输出目录不在根路径下")
    check_timeline(m)
    check_inputs(m)
    require(m["inputPreviewFingerprintSha256"] == preview_fingerprint(m), "输入/预览条件快照指纹不一致")
    require(m["preview"]["rhi"] and "null" not in m["preview"]["rhi"].lower(), "NullRHI 不可作为图像产物")
    layout = m["layout"]
    require(layout == {"columns": COLS, "rows": ROWS, "headerPixels": HEADER, "labelPixels": LABEL,
                       "indexBase": 0, "pageCount": len(m["pages"])}, "版面声明不一致")
    count = len(m["frames"])
    require(len(m["pages"]) == math.ceil(count / (COLS * ROWS)), "页数不完整")
    width, height = m["preview"]["width"], m["preview"]["height"]
    seen_images = set()
    for index, p in enumerate(m["pages"]):
        require(p["page"] == index and p["firstFrame"] == index * COLS * ROWS and
                p["lastFrame"] == min(count, (index + 1) * COLS * ROWS) - 1, f"页范围不一致: {index}")
        path = local_file(directory, p["image"])
        require(digest(path) == p["sha256"], f"页内容已变化: {path}")
        expected_size = (COLS * width, HEADER + ROWS * (height + LABEL))
        require((p["width"], p["height"]) == expected_size, "页尺寸声明不一致")
        page = decode(path, expected_size)
        for frame in m["frames"][p["firstFrame"]:p["lastFrame"] + 1]:
            slot = frame["frame"] % (COLS * ROWS)
            row, column = divmod(slot, COLS)
            rect = [column * width, HEADER + row * (height + LABEL), width, height]
            require(frame["status"] == "rendered" and frame["page"] == index and frame["row"] == row and
                    frame["column"] == column and frame["pixelRect"] == rect, f"页格映射不一致: {frame['frame']}")
            raw = local_file(directory, frame["image"])
            require(raw not in seen_images, "多个源帧引用同一文件")
            seen_images.add(raw)
            require(digest(raw) == frame["sha256"], f"逐帧图像已变化: {raw}")
            image = decode(raw, (width, height))
            x, y, w, h = rect
            require(page.crop((x, y, x + w, y + h)).tobytes() == image.tobytes(), f"总览格与源图不一致: {frame['frame']}")
    return m


def publish_batch(directory):
    directory = Path(directory).resolve()
    path = directory / "batch.json"
    batch = read(path)
    failed = False
    for item in batch["items"]:
        try:
            item_dir = Path(item["directory"]).resolve()
            require(item_dir.is_relative_to(directory), "批次项路径越界")
            m = read(item_dir / "manifest.json")
            require(m["asset"] == item["asset"] and m["batchId"] == batch["batchId"], "批次身份不一致")
            if m["status"] == "framesReady":
                m = publish(item_dir)
            elif m["status"] == "complete":
                validate_manifest(item_dir, m)
            item["status"] = m["status"]
            if m["status"] != "complete":
                failed = True
        except Exception as error:
            item["status"] = "failed"
            item["publicationError"] = str(error)
            failed = True
        print(json.dumps(item, ensure_ascii=False))
    batch["status"] = "incomplete" if failed else "complete"
    write(path, batch)
    print(f"Batch={path} Status={batch['status']}")
    return 2 if failed else 0


def mark_interrupted(receipt, reason):
    """只由已结束/超时的本次 UE 调用使用；保留已完成项，标明其余项未交付。"""
    if not receipt.is_file():
        return
    batch = read(receipt)
    path = Path(batch["directory"]) / "batch.json"
    batch = read(path)
    for item in batch["items"]:
        if item["status"] not in ("pending", "running"):
            continue
        manifest = Path(item["directory"]) / "manifest.json"
        if manifest.is_file():
            m = read(manifest)
            m["status"] = "failed"
            m["error"] = {"stage": "processInterrupted", "reason": reason, "frame": -1}
            write(manifest, m)
        item["status"] = "failed"
        item["publicationError"] = reason
    batch["status"] = "incomplete"
    write(path, batch)
    write(receipt, batch)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    configure = sub.add_parser("configure", help="保存本项目/本机输出根路径；项目默认归档到 Docs/ArtWorkflow/Exports")
    configure.add_argument("--output-root", required=True, type=Path)
    generate = sub.add_parser("generate", help="源采样、RHI 捕获、分页、完整性发布")
    generate.add_argument("--spec", required=True, type=Path)
    generate.add_argument("--metadata-only", action="store_true")
    generate.add_argument("--null-rhi", action="store_true", help="验证失败可见；不产生合格图像")
    for p in (configure, generate):
        p.add_argument("--editor", type=Path, default=EDITOR)
        p.add_argument("--project", type=Path, default=PROJECT)
        p.add_argument("--timeout", type=int, default=3600)
    for name in ("publish-batch", "validate"):
        p = sub.add_parser(name)
        p.add_argument("directory", type=Path)
    args = parser.parse_args()
    if args.command == "validate":
        m = validate_manifest(args.directory)
        print(f"Integrity=passed Asset={m['asset']} Frames={len(m['frames'])} Pages={len(m['pages'])} Visual={m['visualVerification']}")
        return 0
    if args.command == "publish-batch":
        return publish_batch(args.directory)
    calls = args.project.resolve().parent / "Saved/ActionOverviewCalls"
    calls.mkdir(parents=True, exist_ok=True)
    call_id = uuid.uuid4().hex
    receipt = calls / (call_id + ".json")
    log = calls / (call_id + ".log")
    cmd = [str(args.editor), str(args.project.resolve()), "-run=SkillLabActionOverview", "-unattended", "-nop4", "-nosplash", f"-abslog={log}"]
    if args.command == "configure":
        require(args.output_root.is_absolute(), "根路径必须是绝对路径")
        cmd += [f"-SetOutputRoot={args.output_root}", "-nullrhi"]
    else:
        cmd += [f"-Spec={args.spec.resolve()}", f"-Result={receipt}"]
        cmd += ["-nullrhi"] if args.null_rhi or args.metadata_only else ["-AllowCommandletRendering", "-d3d11", "-NoTextureStreaming"]
        if args.metadata_only:
            cmd += ["-MetadataOnly"]
    try:
        with log.with_suffix(".console.log").open("wb") as output:
            result = subprocess.run(cmd, stdout=output, stderr=subprocess.STDOUT, timeout=args.timeout)
    except subprocess.TimeoutExpired:
        mark_interrupted(receipt, f"UE 调用超过 {args.timeout}s；日志 {log}")
        raise
    print(f"UEExitCode={result.returncode} Log={log}")
    if args.command == "configure":
        return result.returncode
    require(receipt.is_file(), f"未取得本次批次 receipt；检查日志: {log}")
    if result.returncode not in (0, 2):
        mark_interrupted(receipt, f"UE 异常退出 {result.returncode}；日志 {log}")
    batch = read(receipt)
    if args.metadata_only:
        print(json.dumps(batch, ensure_ascii=False))
        return 2  # 元数据模式不宣称完成图像。
    publication = publish_batch(batch["directory"])
    return publication if result.returncode in (0, 2) else (result.returncode or 1)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        print(f"ActionOverview FAILED: {error}", file=sys.stderr)
        sys.exit(1)
