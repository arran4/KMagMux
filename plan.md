1. Add `bencode` parsing to a new `BencodeParser` in `src/core/` to read .torrent files.
2. Add a `TorrentParser` class in `src/core/` to extract info_hash, name, and trackers from magnet links and torrent files using the `BencodeParser` for torrent files.
3. Add a `TrackerClient` class in `src/core/` to perform UDP/HTTP scrape and announce requests to tracker URLs given an info hash.
4. Add a `TorrentInfoDialog` class in `src/ui/` that takes a `QString sourcePath` (magnet or .torrent file).
5. In `TorrentInfoDialog`, parse the sourcePath. If valid, display basic info and list of trackers.
6. Provide a "Get Info" or "Query Trackers" button to start querying the trackers interactively (can be cancelled).
7. In `AddItemDialog.cpp` and `ProcessItemDialog.cpp`, replace the `QMessageBox::information` in `infoAction` with showing `TorrentInfoDialog`.
