import unittest
import shutil
import tempfile
import datetime
from pathlib import Path
from kmagmux.core.storage import StorageManager
from kmagmux.core.statemachine import StateMachine
from kmagmux.core.models import ItemState

class TestStateMachine(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.storage = StorageManager(self.test_dir)
        self.sm = StateMachine(self.storage)

        # Create a dummy item
        self.sample_torrent = Path(self.test_dir) / "test.torrent"
        with open(self.sample_torrent, "w") as f:
            f.write("dummy content")
        self.item = self.storage.create_item(str(self.sample_torrent))

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_queue_item(self):
        self.sm.queue_item(self.item.id)
        item = self.storage.load_metadata(self.item.id)
        self.assertEqual(item.state, ItemState.QUEUED)
        self.assertTrue((Path(self.test_dir) / "queue" / item.payload_filename).exists())

    def test_schedule_item(self):
        future_time = datetime.datetime.now() + datetime.timedelta(hours=1)
        self.sm.schedule_item(self.item.id, future_time)
        item = self.storage.load_metadata(self.item.id)
        self.assertEqual(item.state, ItemState.SCHEDULED)
        self.assertEqual(item.scheduled_at, future_time)
        self.assertTrue((Path(self.test_dir) / "scheduled" / item.payload_filename).exists())

    def test_archive_item(self):
        self.sm.archive_item(self.item.id)
        item = self.storage.load_metadata(self.item.id)
        self.assertEqual(item.state, ItemState.ARCHIVED)
        self.assertTrue((Path(self.test_dir) / "archive" / item.payload_filename).exists())

if __name__ == '__main__':
    unittest.main()
