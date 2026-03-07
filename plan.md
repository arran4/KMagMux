1. **Understand**
   - The issue states that in `src/core/Engine.cpp:101`, `Engine::processQueue` loads all items from storage on every tick (`m_storage->loadAllItems()`) just to find items in `Queued` or `Scheduled` state.
   - This causes $O(N)$ disk I/O and JSON parsing for $N$ items in the data directory every 5 seconds, which is highly inefficient.
   - A better approach is to add a `loadItemsByStates(const QList<ItemState> &states)` method in `StorageManager`, as mentioned in the memory ("`Engine::processQueue` is optimized to use `StorageManager::loadItemsByStates` to perform filtered, memory-only queries for 'Queued' and 'Scheduled' items, replacing the previous $O(N)$ disk-based scan."). Wait, the memory states:
     "To minimize redundant disk I/O, `StorageManager` implements an in-memory item cache (`QMap<QString, Item>`) that is populated from the data directory during initialization and kept synchronized via `saveItem`."
     Ah, but looking at `StorageManager.h` and `.cpp`, there is no `m_itemsCache` or similar `QMap<QString, Item>` yet. We need to implement this in-memory cache!

2. **Implement in-memory cache in `StorageManager`**
   - Add `QMap<QString, Item> m_itemCache;` to `StorageManager.h`.
   - In `StorageManager::init()` or the constructor, load all items once from disk into `m_itemCache`.
   - Update `StorageManager::saveItem()` to update `m_itemCache`.
   - Update `StorageManager::deleteItem()` to remove from `m_itemCache`.
   - Add `std::vector<Item> loadItemsByStates(const QList<ItemState> &states) const;` to `StorageManager` that uses the cache.
   - Update `StorageManager::loadAllItems()` to return items from the cache instead of reading from disk every time.
   - Wait, if `loadAllItems()` just uses the cache, do we still need `loadItemsByStates`? Yes, `loadItemsByStates` avoids creating a full copy of all items if we only need a few states. But wait, we can just use `loadItemsByStates` in `Engine::processQueue`. Let's actually check if `m_itemCache` is already implemented. Wait, `grep -R "QMap<QString, Item>" src/core/` returned nothing earlier.

   Let's create the cache.
   - `StorageManager.h`:
     ```cpp
     private:
       QMap<QString, Item> m_itemCache;
       bool m_cacheInitialized = false;
       void initializeCache();
     public:
       std::vector<Item> loadItemsByStates(const std::vector<ItemState> &states) const;
     ```
     Wait, `StorageManager` currently does not cache. Let's add it.

3. **Modify `Engine::processQueue`**
   - Use `m_storage->loadItemsByStates({ItemState::Queued, ItemState::Scheduled})`.
   - Iterate over these filtered items.

4. **Verify**
   - Run the `bench2` executable and verify the performance improvement.
   - Run existing tests.
