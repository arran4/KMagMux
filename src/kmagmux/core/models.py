from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, List
import datetime
import hashlib

class ItemState(Enum):
    UNPROCESSED = "unprocessed"
    QUEUED = "queued"
    SCHEDULED = "scheduled"
    HELD = "held"
    DISPATCHED = "dispatched"
    FAILED = "failed"
    ARCHIVED = "archived"

@dataclass
class ItemMetadata:
    id: str
    source_type: str # 'magnet' or 'torrent'
    original_path: Optional[str] = None
    destination_path: Optional[str] = None
    connector_id: Optional[str] = None
    labels: List[str] = field(default_factory=list)
    created_at: datetime.datetime = field(default_factory=datetime.datetime.now)
    scheduled_at: Optional[datetime.datetime] = None
    state: ItemState = ItemState.UNPROCESSED
    delete_original: bool = False
    payload_filename: Optional[str] = None

    def to_dict(self):
        return {
            "id": self.id,
            "source_type": self.source_type,
            "original_path": self.original_path,
            "destination_path": self.destination_path,
            "connector_id": self.connector_id,
            "labels": self.labels,
            "created_at": self.created_at.isoformat(),
            "scheduled_at": self.scheduled_at.isoformat() if self.scheduled_at else None,
            "state": self.state.value,
            "delete_original": self.delete_original,
            "payload_filename": self.payload_filename
        }

    @classmethod
    def from_dict(cls, data):
        # Handle potential string to datetime conversion
        created_at = datetime.datetime.fromisoformat(data["created_at"]) if isinstance(data["created_at"], str) else data["created_at"]
        scheduled_at = datetime.datetime.fromisoformat(data["scheduled_at"]) if data.get("scheduled_at") and isinstance(data["scheduled_at"], str) else data.get("scheduled_at")

        return cls(
            id=data["id"],
            source_type=data["source_type"],
            original_path=data.get("original_path"),
            destination_path=data.get("destination_path"),
            connector_id=data.get("connector_id"),
            labels=data.get("labels", []),
            created_at=created_at,
            scheduled_at=scheduled_at,
            state=ItemState(data["state"]),
            delete_original=data.get("delete_original", False),
            payload_filename=data.get("payload_filename")
        )

def generate_item_id(content_hash: str) -> str:
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    short_hash = content_hash[:6]
    return f"{timestamp}_{short_hash}"
