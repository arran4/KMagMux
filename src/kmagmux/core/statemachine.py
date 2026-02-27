from typing import Optional
import datetime
from .models import ItemMetadata, ItemState
from .storage import StorageManager

class StateMachine:
    def __init__(self, storage_manager: StorageManager):
        self.storage = storage_manager

    def transition(self, item_id: str, new_state: ItemState) -> Optional[ItemMetadata]:
        item = self.storage.load_metadata(item_id)
        if not item:
            raise ValueError(f"Item with ID {item_id} not found.")

        # Validate transitions (basic validation)
        # Incoming -> Unprocessed (Implicit in creation)
        # Unprocessed -> Queued / Scheduled / Held / Dispatched / Archived
        # Scheduled -> Queued (when time reached)
        # Queued -> Dispatched or Failed
        # Failed -> Queued (retry) or Archived (manual)

        # We can implement specific checks if needed, but for now allow flexible transitions
        # as per the "manage everything" nature of the tool.

        # Perform the move
        self.storage.move_item_file(item, new_state)

        # After move, 'item' object in memory has new state because move_item_file updates it and saves it.
        # But let's reload or trust the update.
        # move_item_file updates the item object passed to it.

        return item

    def queue_item(self, item_id: str):
        return self.transition(item_id, ItemState.QUEUED)

    def schedule_item(self, item_id: str, schedule_time: datetime.datetime):
        item = self.storage.load_metadata(item_id)
        if not item:
            raise ValueError(f"Item {item_id} not found")

        item.scheduled_at = schedule_time
        # Save the schedule time first
        self.storage.save_metadata(item)

        return self.transition(item_id, ItemState.SCHEDULED)

    def hold_item(self, item_id: str):
        return self.transition(item_id, ItemState.HELD)

    def archive_item(self, item_id: str):
        return self.transition(item_id, ItemState.ARCHIVED)

    def dispatch_item(self, item_id: str):
        # This is marking it as dispatched. The actual dispatching logic (calling connector)
        # would happen in the Queue Processor.
        return self.transition(item_id, ItemState.DISPATCHED)

    def fail_item(self, item_id: str):
        return self.transition(item_id, ItemState.FAILED)
