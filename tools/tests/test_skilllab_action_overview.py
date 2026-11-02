"""产物发布故障注入；合成图仅验证索引/完整性，不作为 UE 图像证据。"""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
import json

from PIL import Image

spec = importlib.util.spec_from_file_location("overview", Path(__file__).parents[1] / "skilllab_action_overview.py")
overview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overview)


class PublicationIntegrity(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.directory = self.root / "batch/item"
        self.art_root = self.root / "Docs/ArtWorkflow"
        (self.directory / "frames").mkdir(parents=True)
        inputs = []
        for index in range(3):
            path = self.root / f"source-{index}.uasset"
            path.write_bytes(f"fixture-{index}".encode())
            inputs.append({"object": str(index), "class": "fixture", "packageDirty": False,
                           "file": str(path), "md5": overview.digest(path, "md5")})
        frames = []
        for index in range(13):
            image = Image.new("RGB", (32, 32))
            image.putdata([(x * 7, y * 7, index * 17) for y in range(32) for x in range(32)])
            name = f"frames/{index}.png"
            image.save(self.directory / name)
            frames.append({"frame": index, "seconds": index * 1001 / 30000,
                           "status": "rendered", "image": name, "boneRevision": index + 1, "renderFrameIdentity": index + 1})
        self.m = {"schemaVersion": 1, "batchId": "fixtureBatch", "generationId": "fixtureGeneration",
                  "asset": "/Game/Fixture.Fixture", "status": "framesReady", "directory": str(self.directory),
                  "outputRoot": str(self.root), "inputs": inputs, "frames": frames,
                  "visualVerification": "pendingIndependentInspection",
                  "timing": {"sampleCount": 13, "intervalCount": 12, "numerator": 30000,
                             "denominator": 1001, "durationSeconds": 0.4004,
                             "targetRateNumerator": 30, "targetRateDenominator": 1},
                  "preview": {"view": "fixture", "rhi": "D3D11_fixtureOnly", "width": 32, "height": 32}}
        overview.write(self.directory / "manifest.json", self.m)

    def tearDown(self):
        self.temp.cleanup()

    def test_fractional_source_endpoint_and_page_boundary(self):
        m = overview.publish(self.directory)
        self.assertEqual(m["frames"][12]["seconds"], 0.4004)
        self.assertEqual((m["frames"][12]["page"], m["frames"][12]["row"], m["frames"][12]["column"]), (1, 0, 0))
        self.assertEqual(len(m["pages"]), 2)
        overview.validate_manifest(self.directory)

    def test_missing_frame_cannot_complete(self):
        (self.directory / "frames/7.png").unlink()
        with self.assertRaises(OSError):
            overview.publish(self.directory)
        self.assertEqual(overview.read(self.directory / "manifest.json")["status"], "failed")

    def test_corrupt_page_detected(self):
        overview.publish(self.directory)
        (self.directory / "page-0001.png").write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "页内容已变化"):
            overview.validate_manifest(self.directory)

    def test_changed_input_detected_without_editing_ue_assets(self):
        overview.publish(self.directory)
        Path(self.m["inputs"][0]["file"]).write_bytes(b"changed-fixture")
        with self.assertRaisesRegex(ValueError, "输入版本已变化"):
            overview.validate_manifest(self.directory)

    def test_duplicate_source_index_rejected(self):
        self.m["frames"][7]["frame"] = 6
        overview.write(self.directory / "manifest.json", self.m)
        with self.assertRaisesRegex(ValueError, "帧索引重复或跳帧"):
            overview.publish(self.directory)

    def test_frozen_gpu_revision_rejected(self):
        self.m["frames"][7]["boneRevision"] = self.m["frames"][6]["boneRevision"]
        overview.write(self.directory / "manifest.json", self.m)
        with self.assertRaisesRegex(ValueError, "GPU 帧身份或骨骼 revision 未推进"):
            overview.publish(self.directory)

    def test_existing_page_not_overwritten(self):
        path = self.directory / "page-0000.png"
        path.write_bytes(b"old-result")
        with self.assertRaisesRegex(ValueError, "拒绝覆盖"):
            overview.publish(self.directory)
        self.assertEqual(path.read_bytes(), b"old-result")

    def test_mixed_batch_retains_success_and_failure(self):
        failure = self.directory.parent / "bad"
        failure.mkdir()
        overview.write(failure / "manifest.json", {"asset": "/Game/Missing", "batchId": "fixtureBatch", "status": "failed"})
        overview.write(self.directory.parent / "batch.json", {"batchId": "fixtureBatch", "items": [
            {"asset": self.m["asset"], "directory": str(self.directory)},
            {"asset": "/Game/Missing", "directory": str(failure)}], "outputRoot": str(self.root / "Saved/ActionOverviews")})
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(overview.publish_batch(self.directory.parent, self.art_root), 2)
        batch = overview.read(self.directory.parent / "batch.json")
        self.assertEqual(batch["status"], "incomplete")
        self.assertEqual([item["status"] for item in batch["items"]], ["complete", "failed"])
        self.assertTrue(Path(batch["items"][0]["archiveDirectory"]).is_dir())
        self.assertTrue(Path(batch["items"][1]["diagnosticDirectory"]).is_dir())
        self.assertFalse((self.art_root / "Actions" / "Missing").exists())

    def test_frame_to_cell_tampering_detected(self):
        m = overview.publish(self.directory)
        m["frames"][12]["page"] = 0
        with self.assertRaisesRegex(ValueError, "页格映射不一致"):
            overview.validate_manifest(self.directory, m)

    def test_interrupted_process_preserves_batch_identity(self):
        batch = {"batchId": "fixtureBatch", "directory": str(self.directory.parent), "status": "running", "items": [
            {"asset": self.m["asset"], "directory": str(self.directory), "status": "running"}]}
        receipt = self.root / "receipt.json"
        overview.write(receipt, batch)
        overview.write(self.directory.parent / "batch.json", batch)
        overview.mark_interrupted(receipt, "injected renderer crash")
        self.assertEqual(overview.read(receipt)["status"], "incomplete")
        self.assertEqual(overview.read(self.directory / "manifest.json")["error"]["stage"], "processInterrupted")
        self.assertEqual(overview.read(self.directory / "manifest.json")["status"], "incomplete")

    def test_asset_routing_uses_path_and_stably_disambiguates_name_collision(self):
        a = "/Game/PackA/Anims/AS_Hit.AS_Hit"
        b = "/Game/PackB/Anims/AS_Hit.AS_Hit"
        key_a = overview.asset_key(a, {a, b})
        key_b = overview.asset_key(b, {a, b})
        self.assertNotEqual(key_a, key_b)
        self.assertTrue(key_a.startswith("AS_Hit--"))
        self.assertEqual(key_a, overview.asset_key(a, {b, a}))
        self.assertEqual(overview.asset_key(a, {a}), "AS_Hit")

    def test_complete_publication_is_flat_and_keeps_images_beside_index(self):
        m = overview.publish(self.directory)
        legacy = self.art_root / "Actions" / "Fixture" / "Overview" / "oldGeneration"
        legacy.mkdir(parents=True)
        (legacy / "overview.json").write_text("legacy", encoding="utf-8")
        target = overview.archive_complete(m, self.directory, self.art_root / "Actions", {m["asset"]})
        official = overview.validate_official_overview(target)
        self.assertEqual(official["assetObjectPath"], m["asset"])
        self.assertEqual(official["objectName"], "Fixture")
        self.assertEqual(official["frameRateNumerator"], 30000)
        self.assertEqual(official["frameRateDenominator"], 1001)
        self.assertEqual(official["sampleCount"], 13)
        self.assertAlmostEqual(official["durationSeconds"], 0.4004)
        self.assertEqual(official["pages"][-1]["firstFrame"], 12)
        self.assertEqual(official["pages"][-1]["lastFrame"], 12)
        self.assertEqual(set(p.name for p in target.iterdir()), {"overview.json", "page-0000.png", "page-0001.png"})
        self.assertNotIn("frames", official)
        self.assertNotIn("rootMotion", official)
        self.assertNotIn("frames", official)
        self.assertTrue(all("seconds" not in page for page in official["pages"]))
        self.assertEqual(target.name, "Fixture")
        self.assertFalse((target / "Overview").exists())

    def test_noncomplete_results_are_routed_to_status_diagnostics(self):
        for status in ("metadataOnly", "failed", "incomplete"):
            manifest = {"asset": "/Game/Pack/AS_Test.AS_Test", "generationId": f"gen-{status}", "status": status}
            target = overview.archive_diagnostic(manifest, status, self.art_root / "Diagnostics", {manifest["asset"]})
            self.assertTrue(target.is_relative_to((self.art_root / "Diagnostics" / overview.DIAGNOSTIC_DIRS[status]).resolve()))
            self.assertEqual(overview.read(target / "report.json")["status"], status)
            self.assertFalse((self.art_root / "Actions" / "AS_Test").exists())

    def test_batch_routes_each_noncomplete_state_to_exact_diagnostics_bucket(self):
        for status in ("metadataOnly", "failed", "incomplete"):
            batch_dir = self.root / f"batch-{status}"
            item_dir = batch_dir / "item"
            item_dir.mkdir(parents=True)
            manifest = {"asset": f"/Game/Pack/AS_{status}.AS_{status}", "generationId": f"g-{status}",
                        "batchId": f"b-{status}", "status": status}
            overview.write(item_dir / "manifest.json", manifest)
            item = {"asset": manifest["asset"], "directory": str(item_dir), "status": status}
            overview.write(batch_dir / "batch.json", {"batchId": manifest["batchId"], "outputRoot": str(self.root / "Saved/ActionOverviews"), "items": [item]})
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(overview.publish_batch(batch_dir, self.art_root), 2)
            published_batch = overview.read(batch_dir / "batch.json")
            diag_root = self.art_root / "Diagnostics" / overview.DIAGNOSTIC_DIRS[status]
            self.assertTrue(Path(published_batch["items"][0]["diagnosticDirectory"]).is_relative_to(diag_root.resolve()))
            self.assertTrue((diag_root / "Batches" / manifest["batchId"] / "batch.json").is_file())
        self.assertFalse((self.art_root / "Actions").exists())

    def test_regeneration_replaces_current_archive_and_removes_stale_pages(self):
        m = overview.publish(self.directory)
        target = overview.archive_complete(m, self.directory, self.art_root / "Actions", {m["asset"]})
        (target / "page-9999.png").write_bytes(b"stale")
        m["generationId"] = "nextGeneration"
        overview.archive_complete(m, self.directory, self.art_root / "Actions", {m["asset"]})
        current = overview.validate_official_overview(target)
        self.assertEqual(current["generationId"], "nextGeneration")
        self.assertFalse((target / "page-9999.png").exists())
        self.assertEqual({p.name for p in target.glob("page-*.png")}, {"page-0000.png", "page-0001.png"})


if __name__ == "__main__":
    unittest.main()
