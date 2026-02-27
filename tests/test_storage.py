import unittest
import shutil
import tempfile
import os
from pathlib import Path
from kmagmux.core.storage import StorageManager
from kmagmux.core.models import ItemState

class TestStorageManager(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.storage = StorageManager(self.test_dir)
        self.sample_torrent = Path(self.test_dir) / "test.torrent"
        with open(self.sample_torrent, "w") as f:
            f.write("d8:announce33:http://torrent.ubuntu.com:6969/announcee")

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_directory_creation(self):
        for dir_name in ["inbox", "queue", "scheduled", "hold", "archive", "data", "logs"]:
            self.assertTrue((Path(self.test_dir) / dir_name).exists())

    def test_create_item_torrent(self):
        item = self.storage.create_item(str(self.sample_torrent))
        self.assertEqual(item.source_type, "torrent")
        self.assertEqual(item.state, ItemState.UNPROCESSED)
        self.assertTrue((Path(self.test_dir) / "inbox" / item.payload_filename).exists())
        self.assertTrue((Path(self.test_dir) / "data" / f"{item.id}.json").exists())

    def test_create_item_magnet(self):
        magnet_link = "magnet:?xt=urn:btih:1234567890abcdef"
        item = self.storage.create_item(magnet_link, is_magnet=True)
        self.assertEqual(item.source_type, "magnet")
        self.assertEqual(item.state, ItemState.UNPROCESSED)
        self.assertTrue((Path(self.test_dir) / "inbox" / item.payload_filename).exists())
        with open(Path(self.test_dir) / "inbox" / item.payload_filename, "r") as f:
            content = f.read()
            self.assertEqual(content, magnet_link)

    def test_move_item(self):
        item = self.storage.create_item(str(self.sample_torrent))
        original_path = self.storage.get_payload_path(item)

        self.storage.move_item_file(item, ItemState.QUEUED)

        item_reloaded = self.storage.load_metadata(item.id)
        self.assertEqual(item_reloaded.state, ItemState.QUEUED)

        new_path = self.storage.get_payload_path(item_reloaded)
        self.assertTrue(new_path.exists())
        self.assertFalse(original_path.exists())
        self.assertEqual(new_path.parent.name, "queue")

if __name__ == '__main__':
    unittest.main()
