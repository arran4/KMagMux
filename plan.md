1. **Understand the Vulnerability:**
   - The file `src/plugins/qbittorrent/QBittorrentConnector.cpp` contains hardcoded credentials (`"admin"` and `"adminadmin"`) for connecting to a qBittorrent instance.
   - Hardcoded credentials are a security risk because they can be extracted from the compiled binary or source code. If deployed or shared, anyone can potentially gain unauthorized access to the qBittorrent instance if it's reachable.

2. **Assess the Risk:**
   - **Data/Functionality Compromised:** Unauthorized control over a qBittorrent instance. An attacker could add, pause, resume, delete torrents, and potentially access downloaded files or consume network bandwidth.
   - **Exploitation:** Anyone with access to the source code or binary who also has network access to the target qBittorrent Web UI.
   - **Blast Radius:** Depends on the qBittorrent setup, but could lead to complete compromise of the torrenting service.
   - **Solution:** Remove the hardcoded credentials and URL. Ideally, read these from a configuration file, environment variables, or allow them to be set securely. The class already provides `setBaseUrl` and `setCredentials` methods. For now, we will simply initialize these to empty strings or let the user configure them before dispatching. Since the application currently doesn't have a settings storage connected to connectors, leaving them empty means they need to be configured before use. Let's initialize them to empty strings.

3. **Implement the Fix:**
   - Modify `src/plugins/qbittorrent/QBittorrentConnector.cpp` to initialize `m_baseUrl`, `m_username`, and `m_password` to empty strings in the constructor.
   - Ensure the code handles empty credentials gracefully (e.g., skip login if empty, or just let the API request fail). For now, initializing them to empty strings is the standard way to remove hardcoded secrets.

4. **Verify the Fix:**
   - Run format checks.
   - Run build and test suite.

5. **Document the Fix:**
   - Create PR with appropriate security tags.
