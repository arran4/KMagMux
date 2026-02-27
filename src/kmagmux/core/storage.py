import os
import json
import shutil
import hashlib
from typing import List, Optional
from pathlib import Path
from .models import ItemMetadata, ItemState, generate_item_id

class StorageManager:
    def __init__(self, base_dir: str):
        self.base_dir = Path(base_dir).expanduser()
        self.dirs = {
            "inbox": self.base_dir / "inbox",
            "queue": self.base_dir / "queue",
            "scheduled": self.base_dir / "scheduled",
            "hold": self.base_dir / "hold",
            "archive": self.base_dir / "archive",
            "dispatched": self.base_dir / "dispatched",
            "data": self.base_dir / "data",
            "logs": self.base_dir / "logs",
        }
        self._ensure_dirs()

    def _ensure_dirs(self):
        for path in self.dirs.values():
            path.mkdir(parents=True, exist_ok=True)

    def create_item(self, source_path_or_magnet: str, is_magnet: bool = False, delete_original: bool = False) -> ItemMetadata:
        # Generate ID
        if is_magnet:
            content = source_path_or_magnet.encode('utf-8')
            source_type = 'magnet'
            original_path = None # Magnets don't have a file path
        else:
            with open(source_path_or_magnet, 'rb') as f:
                content = f.read()
            source_type = 'torrent'
            original_path = str(Path(source_path_or_magnet).absolute())

        content_hash = hashlib.sha256(content).hexdigest()
        item_id = generate_item_id(content_hash)

        # Determine payload filename
        if is_magnet:
            payload_filename = f"{item_id}.magnet"
        else:
            payload_filename = f"{item_id}.torrent"

        metadata = ItemMetadata(
            id=item_id,
            source_type=source_type,
            original_path=original_path,
            delete_original=delete_original,
            payload_filename=payload_filename,
            state=ItemState.UNPROCESSED
        )

        # Save payload to inbox
        payload_path = self.dirs['inbox'] / payload_filename
        if is_magnet:
            with open(payload_path, 'w') as f:
                f.write(source_path_or_magnet)
        else:
            # Copy first to inbox
            shutil.copy2(source_path_or_magnet, payload_path)

            # Handle delete_original logic later in the workflow or here?
            # The prompt says: "If 'delete after' is checked... Move the torrent file into an internal managed directory"
            # Since we are creating it, let's just copy it for now. The "delete original" action happens when user confirms.
            # Wait, prompt says: "If 'delete after' is checked and user chooses Queue/Schedule/Save... Move..."
            # For now, we are just ingesting. We will support moving later.

        # Save metadata
        self.save_metadata(metadata)

        return metadata

    def save_metadata(self, metadata: ItemMetadata):
        data_path = self.dirs['data'] / f"{metadata.id}.json"
        with open(data_path, 'w') as f:
            json.dump(metadata.to_dict(), f, indent=2)

    def load_metadata(self, item_id: str) -> Optional[ItemMetadata]:
        data_path = self.dirs['data'] / f"{item_id}.json"
        if not data_path.exists():
            return None
        with open(data_path, 'r') as f:
            data = json.load(f)
            return ItemMetadata.from_dict(data)

    def get_payload_path(self, metadata: ItemMetadata) -> Path:
        # Find where the payload is based on state.
        # Actually, the payload might just stay in 'inbox' or move to 'queue' etc.
        # The prompt implies folders per state: "inbox/ (unprocessed items)", "queue/ (queued items)".

        # Map state to folder name
        state_folder_map = {
            ItemState.UNPROCESSED: 'inbox',
            ItemState.QUEUED: 'queue',
            ItemState.SCHEDULED: 'scheduled',
            ItemState.HELD: 'hold',
            ItemState.DISPATCHED: 'dispatched',
            ItemState.ARCHIVED: 'archive'
        }

        folder_name = state_folder_map.get(metadata.state)

        if not folder_name:
             if metadata.state == ItemState.FAILED:
                 folder_name = 'queue'
             else:
                 # Default fallback if unknown state
                 folder_name = 'inbox'

        return self.dirs[folder_name] / metadata.payload_filename

    def move_item_file(self, metadata: ItemMetadata, new_state: ItemState):
        current_payload_path = self.get_payload_path(metadata)

        # Update state in metadata object (temporarily to find new path)
        old_state = metadata.state
        metadata.state = new_state
        new_payload_path = self.get_payload_path(metadata)
        metadata.state = old_state # Revert

        if current_payload_path != new_payload_path:
             if current_payload_path.exists():
                 shutil.move(str(current_payload_path), str(new_payload_path))

        metadata.state = new_state
        self.save_metadata(metadata)

    def list_items(self, state: Optional[ItemState] = None) -> List[ItemMetadata]:
        items = []
        for metadata_file in self.dirs['data'].glob("*.json"):
            item = self.load_metadata(metadata_file.stem)
            if item:
                if state is None or item.state == state:
                    items.append(item)
        return items
