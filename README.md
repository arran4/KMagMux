# KMagMux
Torrent and Magent file and protocol handler for routing to programs / services

KMagMux intercepts `.torrent` files and `magnet:` URIs, allowing you to route them to various services and programs, both local and remote (such as qBittorrent, Put.io, Premiumize, TorBox, and more).

## Functionality Overview

### Main Interface
The main interface allows you to view and manage your items across different states: Unprocessed, Queue, Done, Error, and Archived.

*(Imagine screenshot showing the main interface with Unprocessed items)*

### Adding Items
You can manually add items via the UI by pasting paths, URLs, or magnet links. The system automatically extracts data and attempts to connect to available routing destinations.

*(Imagine screenshot showing the Add Items dialog)*

### Processing Queue
Once added, items are moved to the Queue, where the Engine attempts to dispatch them to the selected Connectors (services/programs).

*(Imagine screenshot showing the Queue tab)*

### Finished and Failed Dispatches
Items that successfully dispatched are found in the Done tab, while those that encountered errors or failed authentication move to the Error tab.

*(Imagine screenshot showing the Done tab)*

*(Imagine screenshot showing the Error tab)*

### Archiving and Clean Up
You can move completed or unwanted items to the Archived tab for later review, or permanently delete them from the interface and storage cache.

*(Imagine screenshot showing the Archived tab)*

## Mock / Screenshot Generation Mode
KMagMux includes a mock build `KMagMuxMock` (found in `.jules/mock`) that disables background processing and network interaction while populating the UI with sample data. This is primarily used for generating reproducible screenshots for documentation.

To run:
```bash
KMAGMUX_MOCK_MODE=1 ./build/.jules/mock/KMagMuxMock
```
