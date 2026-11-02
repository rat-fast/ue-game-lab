"""产物发布故障注入；合成图仅验证索引/完整性，不作为 UE 图像证据。"""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

from PIL import Image

spec = importlib.util.spec_from_file_location("overview", Path(__file__).parents[1] / "skilllab_action_overview.py")
overview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overview)


class PublicationIntegrity(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.directory = self.root / "batch/item"
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
            {"asset": "/Game/Missing", "directory": str(failure)}]})
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(overview.publish_batch(self.directory.parent), 2)
        batch = overview.read(self.directory.parent / "batch.json")
        self.assertEqual(batch["status"], "incomplete")
        self.assertEqual([item["status"] for item in batch["items"]], ["complete", "failed"])

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


if __name__ == "__main__":
    unittest.main()
