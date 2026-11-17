"""Request validation and result integrity checks for the spatial export CLI."""
import importlib.util
import contextlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location("export_skill_spatial", Path(__file__).parents[1] / "export_skill_spatial.py")
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class SpatialExportInputs(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.project = self.root / "Fixture.uproject"
        self.project.write_text("{}", encoding="utf-8")
        self.base = {"map": "/Game/Test/L_Test", "caster": {"player_index": 0}, "skill_id": "Skill_Test",
                     "duration_seconds": 1, "target": {"ground_location": [1, 2, 3]}}

    def tearDown(self):
        self.temp.cleanup()

    def normalize(self, request=None):
        return exporter.normalize_request(request or self.base, "capture", self.project)

    def test_defaults_are_filled_without_changing_ground_target(self):
        request = self.normalize()
        self.assertEqual(request["resolution"], [1280, 720])
        self.assertEqual(request["views"], [{"id": "game", "type": "game"}])
        self.assertEqual(request["target"]["ground_location"], [1.0, 2.0, 3.0])

    def test_self_cast_can_omit_target(self):
        request = dict(self.base)
        request.pop("target")
        request["duration_seconds"] = 0.2
        self.assertNotIn("target", self.normalize(request))

    def test_rejects_fuzzy_actor_locator_and_invalid_sampling(self):
        request = dict(self.base, caster={"player_index": 0, "level_actor_path": "/Game/A"})
        with self.assertRaisesRegex(exporter.ExportError, "只能包含"):
            self.normalize(request)
        request = dict(self.base, sample_hz=float("inf"))
        with self.assertRaisesRegex(exporter.ExportError, "有限数字"):
            self.normalize(request)

    def test_custom_views_require_perspective_parameters_and_unique_safe_ids(self):
        request = dict(self.base, views=[{"id": "game", "type": "game"},
                                        {"id": "../other", "type": "game"}])
        with self.assertRaisesRegex(exporter.ExportError, "路径分隔符"):
            self.normalize(request)
        request["views"] = [{"id": "custom", "type": "custom", "location_cm": [0, 0, 0],
                             "rotation_deg": [0, 0, 0], "fov_deg": 90}]
        self.assertEqual(self.normalize(request)["views"][0]["fov_deg"], 90)

    def test_png_index_requires_png_header_and_dimensions(self):
        image = self.root / "frame.png"
        def chunk(kind, body):
            return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
        header = struct.pack(">IIBBBBB", 1280, 720, 8, 2, 0, 0, 0)
        rows = (b"\x00" + b"\x00" * (1280 * 3)) * 720
        image.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))
        self.assertEqual(exporter.png_size(image), (1280, 720))
        image.write_bytes(image.read_bytes()[:-1] + b"\x01")
        self.assertIsNone(exporter.png_size(image))

    def test_configure_saves_project_local_default(self):
        output = self.root / "exports"
        args = SimpleNamespace(project=str(self.project), output_root=str(output))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(exporter.configure(args), 0)
        settings = json.loads((self.root / "Saved/SpatialExportTool/settings.json").read_text(encoding="utf-8"))
        self.assertEqual(settings["output_root"], str(output.resolve()))

    def test_argument_errors_return_json_protocol(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            with self.assertRaises(SystemExit) as raised:
                exporter.main(["capture"])
        self.assertEqual(raised.exception.code, 1)
        self.assertEqual(json.loads(output.getvalue())["status"], "failed")

    def test_capture_with_real_materials_and_declared_limitations_is_partial(self):
        output = self.root / "run"
        (output / "frames/game").mkdir(parents=True)
        for name in ("request.json", "events.jsonl", "samples.jsonl"):
            (output / name).write_text("{}\n", encoding="utf-8")
        (output / "configuration.json").write_text(json.dumps({"selected_skill_id": "Skill_Test", "granted_skills": [{"skill_id": "Skill_Test"}], "caster_runtime": {"actor_path": "/Game/Test.Player", "runtime_component_path": "/Game/Test.Player.Runtime", "generation": 1}, "scene_preservation": {"skill_lab_dummy_tag_count": 0, "protected_actors": []}}), encoding="utf-8")
        (output / "references.json").write_text(json.dumps({"caster_path": "/Game/Test.Player", "actors": [{"role": "caster"}]}), encoding="utf-8")
        def chunk(kind, body):
            return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
        header = struct.pack(">IIBBBBB", 1280, 720, 8, 2, 0, 0, 0)
        rows = (b"\x00" + b"\x00" * (1280 * 3)) * 720
        png = output / "frames/game/000001.png"
        png.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))
        exporter.write_json(output / "ue_receipt.json", {"complete": True, "request_accepted": True,
                                                          "limitations": ["pose/render identity unavailable"]})
        request = {"resolution": [1280, 720], "project_path": str(self.project), "map": "/Game/Test",
                   "caster": {"player_index": 0}, "skill_id": "Skill_Test"}
        manifest = exporter.finalize_manifest(output, request, "capture", "fixture", 0, [])
        self.assertEqual(manifest["status"], "partial")
        self.assertIn("frames/game/000001.png", manifest["files"])

    def test_manifest_requires_each_requested_view_and_indexed_marked_png(self):
        output = self.root / "multi"
        for directory in ("frames/game", "frames/custom", "marked/game", "marked/custom"):
            (output / directory).mkdir(parents=True)
        for name in ("request.json", "request.original.json", "samples.jsonl", "ue.log"):
            (output / name).write_text("{}\n", encoding="utf-8")
        (output / "configuration.json").write_text(json.dumps({"selected_skill_id": "Skill_Test", "granted_skills": [{"skill_id": "Skill_Test"}], "caster_runtime": {"actor_path": "/Game/Test.Player", "runtime_component_path": "/Game/Test.Player.Runtime", "generation": 1}, "scene_preservation": {"skill_lab_dummy_tag_count": 0, "protected_actors": []}}), encoding="utf-8")
        (output / "references.json").write_text(json.dumps({"caster_path": "/Game/Test.Player", "actors": [{"role": "caster"}]}), encoding="utf-8")
        events = [
            {"event": "ESkillLabSkillEvent::Started", "request_id": 1, "cast_id": 1, "runtime_generation": 1},
            {"event": "request_return", "accepted": True},
        ]
        (output / "events.jsonl").write_text("\n".join(json.dumps(event) for event in events) + "\n", encoding="utf-8")
        def chunk(kind, body):
            return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
        header = struct.pack(">IIBBBBB", 1280, 720, 8, 2, 0, 0, 0)
        rows = (b"\x00" + b"\x00" * (1280 * 3)) * 720
        png_data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
        for directory in ("frames/game", "frames/custom", "marked/game", "marked/custom"):
            (output / directory / "000001.png").write_bytes(png_data)
        sample = {"sample_id": "000001", "world_time_seconds": 1.0, "since_request_world_seconds": None,
                  "request_time_relation": "before_request", "game_frame": 1,
                  "views": [{"view_id": "game", "image": "frames/game/000001.png", "game_frame": 1, "render_capture_id": "fixture:game"},
                            {"view_id": "custom", "image": "frames/custom/000001.png", "game_frame": 1, "render_capture_id": "fixture:custom"}]}
        (output / "samples.jsonl").write_text(json.dumps(sample) + "\n", encoding="utf-8")
        exporter.write_json(output / "ue_receipt.json", {"complete": True, "request_accepted": True, "limitations": []})
        request = {"resolution": [1280, 720], "project_path": str(self.project), "map": "/Game/Test",
                   "caster": {"player_index": 0}, "skill_id": "Skill_Test", "markers": True,
                   "views": [{"id": "game", "type": "game"}, {"id": "custom", "type": "custom"}]}
        manifest = exporter.finalize_manifest(output, request, "capture", "fixture", 0, [])
        self.assertEqual(manifest["status"], "complete")
        self.assertIn("marked/custom/000001.png", manifest["files"])
        (output / "events.jsonl").write_text('{"event":\n', encoding="utf-8")
        manifest = exporter.finalize_manifest(output, request, "capture", "fixture", 0, [])
        self.assertEqual(manifest["status"], "partial")
        self.assertTrue(any("events.jsonl 无法解析" in error for error in manifest["errors"]))
        (output / "events.jsonl").write_text("", encoding="utf-8")
        manifest = exporter.finalize_manifest(output, request, "capture", "fixture", 0, [])
        self.assertEqual(manifest["status"], "partial")
        self.assertTrue(any("缺少 accepted request_return" in error for error in manifest["errors"]))


if __name__ == "__main__":
    unittest.main()
