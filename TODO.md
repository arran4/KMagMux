## To-do list for implementing [KMagMux](https://github.com/arran4/KMagMux) (KDE app)

### 0) Scope and guiding rules

* [ ] Define supported inputs: `magnet:` URLs, `.torrent` files, and “open with” from browser/file manager
* [ ] Define routing targets (initial list): local torrent clients (via CLI/RPC), optional SaaS “connectors”
* [ ] Commit to storage model: **everything is files**, human-inspectable, user-controlled
* [ ] Decide app shape: tray utility + main window, or main window only

---

## 1) UX skeleton (what user sees)

### 1.1 Main window (download-manager style)

* [ ] Tabs/sections:

  * [ ] **Unprocessed** (incoming items waiting for action)
  * [ ] **Queue** (items scheduled/queued for dispatch)
  * [ ] **Archive** (optional; toggle on/off)
* [ ] Basic actions per item:

  * [ ] Open details
  * [ ] Dispatch now
  * [ ] Queue
  * [ ] Schedule
  * [ ] Save for later (hold)
  * [ ] Remove (with “move to archive” vs “delete data”)
* [ ] Global controls:

  * [ ] Pause/resume queue processing
  * [ ] “Archive enabled” toggle
  * [ ] Search/filter (by name, tracker, label)
  * [ ] Status bar: queue size, next scheduled run, last dispatch result

### 1.2 “Add Torrent/Magnet” dialog (the core flow)

* [ ] Inputs:

  * [ ] Destination folder picker
  * [ ] Routing target selector (default + “remember per source/label”)
  * [ ] Buttons: **Dispatch**, **Queue**, **Schedule**, **Save for later**
  * [ ] Schedule controls (date/time + timezone)
  * [ ] Checkbox: **Delete original torrent file after action**
* [ ] Behavior requirement:

  * [ ] If “delete after” is checked and user chooses Queue/Schedule/Save:

    * [ ] Move the torrent file into an **internal managed directory** (not a hidden database)
    * [ ] Replace the original with nothing (or optionally keep a stub link file if you want)
  * [ ] For magnets: store a “.magnet” text record file in internal managed directory when queued/scheduled/saved

---

## 2) System integration (KDE/Linux plumbing)

* [ ] Register as handler for:

  * [ ] `magnet:` URL scheme (xdg + desktop entry)
  * [ ] `.torrent` MIME type (`application/x-bittorrent`)
* [ ] Provide “Send to VARAI” context action (optional):

  * [ ] Dolphin service menu for `.torrent`
  * [ ] Browser integration later (optional)
* [ ] Single-instance behavior:

  * [ ] If already running, new magnet/torrent opens in existing instance and adds to Unprocessed

---

## 3) Storage layout (file-based, user-controlled)

Choose a clear directory under an app folder the user can relocate.

* [x] Decide base directory (example): `~/.local/share/<appname>/`
* [x] Create subfolders:

  * [x] `inbox/` (unprocessed items)
  * [x] `queue/` (queued items ready to process)
  * [x] `scheduled/` (future)
  * [x] `hold/` (saved for later)
  * [x] `archive/` (past items; optional feature toggle)
  * [x] `data/` (per-item metadata files)
  * [x] `logs/` (dispatch logs per item)
* [x] Per item file convention:

  * [x] A stable item id: timestamp + short hash (e.g., `20260227-142233_ab12cd`)
  * [x] Payload file:

    * [x] `.torrent` file OR `.magnet` text file
  * [x] Metadata file (JSON/YAML/TOML—pick one):

    * [x] source type (torrent/magnet)
    * [x] original path / source app
    * [x] chosen destination path
    * [x] target connector
    * [x] labels/tags
    * [x] created time, scheduled time
    * [x] user choice (dispatch/queue/schedule/hold)
    * [x] delete_original flag + what happened (moved/deleted)
  * [x] Status file (or in metadata):

    * [x] state: unprocessed|queued|scheduled|held|dispatched|failed|archived
    * [x] last result, last error, retries count
* [ ] Provide “Open storage folder” in settings so users can see/edit files

---

## 4) Core engine: state machine + queue processor

* [x] Define state transitions (explicit, testable):

  * [x] Incoming → Unprocessed
  * [x] Unprocessed → Queued / Scheduled / Held / Dispatched / Archived
  * [x] Scheduled → Queued when time reached
  * [x] Queued → Dispatched or Failed
  * [x] Failed → Queued (retry) or Archived (manual)
* [ ] Implement file watcher:

  * [ ] Watch `inbox/` for new items
  * [ ] Watch queue folders for changes (optional)
* [ ] Queue processor loop:

  * [ ] If paused: do nothing
  * [ ] If scheduled items due: move to queue
  * [ ] Pop next queued item (deterministic ordering)
  * [ ] Dispatch via connector
  * [ ] Write result log + update status
  * [ ] Move to archive if enabled; otherwise keep in “dispatched/”

---

## 5) Connectors (routing to clients/SaaS)

Start with local clients; keep interface generic.

### 5.1 Connector interface

* [ ] Define a connector contract:

  * [ ] `capabilities` (magnet/torrent, labels, download-dir, paused, category)
  * [ ] `dispatch(item, destination, options) -> result`
  * [ ] `healthcheck()`
* [ ] Store connector configs as files:

  * [ ] `connectors/<name>.json` (RPC URL, API key, CLI path, etc.)

### 5.2 Initial connectors (practical first)

* [ ] **qBittorrent** (Web API)
* [ ] **Transmission** (RPC)
* [ ] **aria2** (RPC, if you want a “download-manager feel”)
* [ ] “Custom command” connector:

  * [ ] user defines a command template like: `qbittorrent-nox --someflag "{payload}"`
  * [ ] safe escaping rules

### 5.3 SaaS (later)

* [ ] Define “external service” connector pattern:

  * [ ] credentials stored in user-owned files
  * [ ] explicit opt-in
  * [ ] clear logging and “what data is sent” view

---

## 6) Preferences & policy rules (VARAI behavior)

* [ ] Defaults:

  * [ ] default destination folder
  * [ ] default connector
  * [ ] archive enabled toggle
  * [ ] delete-after behavior default (off)
* [ ] Optional automation rules (simple to start):

  * [ ] If magnet contains tracker X → connector A
  * [ ] If filename matches regex → destination folder Y
  * [ ] If added during quiet hours → scheduled next morning
* [ ] Ensure rules are stored as readable files (e.g., `rules.json`)

---

## 7) Safety, transparency, and user control

* [ ] “Dry run” mode: show what would happen without dispatching
* [ ] A per-item “audit trail” view (render from logs/files)
* [ ] Never delete user files silently:

  * [ ] When “delete original torrent” is checked, UI explains: “Will move into managed storage”
* [ ] Export/import:

  * [ ] “Export queue/archive” is literally copying folders
* [ ] Clear failure handling:

  * [ ] failed item stays with logs + “retry” button

---

## 8) Testing plan (minimal but real)

* [ ] Unit tests for:

  * [x] state transitions
  * [x] metadata serialization
  * [x] file moves (delete-after behavior)
  * [ ] scheduling logic (timezone-safe)
* [ ] Integration tests for connectors (mock server for RPC)
* [ ] Manual test matrix:

  * [ ] magnet from browser
  * [ ] torrent from Dolphin
  * [ ] queue + app restart + resumes correctly
  * [ ] archive on/off

---

## 9) Implementation order (recommended)

1. [x] Storage layout + item model + state machine (no UI)
2. [ ] URL/mime handler integration → inbox ingestion
3. [ ] Basic UI list: Unprocessed + simple “Dispatch/Queue/Hold”
4. [ ] Move-to-managed-dir behavior + delete-after checkbox
5. [ ] Queue processor loop + logs
6. [ ] One connector (qBittorrent or Transmission)
7. [ ] Scheduling
8. [ ] Archive toggle + views
9. [ ] Rules/automation + more connectors

---

## Concrete deliverable checklist for “MVP”

* [ ] Magnet and `.torrent` open into app
* [ ] Add dialog: destination + Dispatch/Queue/Schedule/Hold + delete-after
* [ ] Unprocessed list + Queue list
* [x] File-based storage with readable metadata
* [ ] One working connector
* [ ] Logs + visible success/failure status
* [ ] Restart-safe queue persistence

## Self-hosted targets to route torrents/magnets to (with API docs)

Below is a comprehensive “first wave” set of **self-hosted** services and apps that can accept `.torrent` and/or `magnet:` inputs and can realistically be implemented as independent connectors in a “service registry lite” pattern.

For each, I’ve included the **primary API documentation** and noted typical **auth** + **what you can send**.

---

### A) Torrent clients with stable remote-control APIs (primary routing targets)

#### 1) qBittorrent (WebUI / Web API)

* Send: **magnet**, **torrent file**
* Auth: session cookie (login), optional reverse-proxy auth; API itself uses its login flow
* Docs:

```text
https://github.com/qbittorrent/qBittorrent/wiki/WebUI-API-%28qBittorrent-5.0%29
https://www.qbittorrent.org/openapi-demo/
```

([GitHub][1])

#### 2) Transmission (JSON-RPC)

* Send: **magnet**, **torrent file**
* Auth: typically basic auth / header token depending on deployment; RPC has “X-Transmission-Session-Id” handshake
* Docs:

```text
https://github.com/transmission/transmission/blob/main/docs/rpc-spec.md
```

([GitHub][2])

#### 3) Deluge (Web JSON-RPC)

* Send: **magnet**, **torrent file**
* Auth: JSON-RPC login via web UI (`/json`)
* Docs:

```text
https://deluge.readthedocs.io/en/latest/reference/webapi.html
https://deluge.readthedocs.io/en/latest/devguide/how-to/curl-jsonrpc.html
```

([deluge.readthedocs.io][3])

#### 4) rTorrent (XML-RPC via SCGI→HTTP, or HTTP XML-RPC)

* Send: **magnet**, **torrent file**
* Auth: usually handled at the HTTP layer (reverse proxy / basic auth), not by XML-RPC itself
* Docs:

```text
https://rtorrent-docs.readthedocs.io/en/latest/cmd-ref.html
https://github.com/rakshasa/rtorrent-doc/blob/master/RPC-Setup-XMLRPC.md
```

([rtorrent-docs.readthedocs.io][4])

#### 5) aria2 (JSON-RPC / WebSocket JSON-RPC)

* Send: **torrent file**, (and magnets via torrent/BT integration depending on configuration)
* Auth: optional RPC secret token
* Docs:

```text
https://aria2.github.io/manual/en/html/aria2c.html
https://aria2.github.io/manual/en/html/
```

([aria2.github.io][5])

---

### B) Self-hosted automation tools that can “receive” releases and then send to a download client

These are useful if your VARAI is acting as a **dispatcher into an automation pipeline**, not just directly into a torrent client.

#### 6) Sonarr (TV) API

* Can accept: release “push” workflows (used by external automation) and manage download clients
* Auth: API key header
* Docs:

```text
https://sonarr.tv/docs/api/
```

([sonarr.tv][6])

#### 7) Radarr (Movies) API

* Auth: API key header
* Docs:

```text
https://radarr.video/docs/api/
```

([radarr.video][7])

#### 8) Lidarr (Music) API

* Auth: API key header
* Docs:

```text
https://lidarr.audio/docs/api/
```

([lidarr.audio][8])

#### 9) Readarr (Books) API

* Auth: API key header
* Docs:

```text
https://readarr.com/docs/api/
```

([readarr.com][9])

#### 10) Whisparr API (adult media manager)

* Auth: API key header (Servarr-style)
* Docs:

```text
https://whisparr.com/docs/api/
```

([whisparr.com][10])

#### 11) Prowlarr (Indexer manager) API

* More niche as a “target” (it’s usually upstream), but still a real self-hosted API you may want to integrate for rule-based routing / indexer actions.
* Auth: API key header
* Docs:

```text
https://prowlarr.com/docs/api/
```

([prowlarr.com][11])

#### 12) autobrr (torrent automation) API

* Useful as a target when VARAI wants to “hand off” automation rather than download directly
* Auth: API key (autobrr supports keys for integrations)
* Docs:

```text
https://autobrr.com/api
```

([autobrr.com][12])

---

### C) Self-hosted request-management systems (useful as “inbox” targets)

These don’t download themselves, but they are valid routing targets if your VARAI can create requests/tickets/tasks in a media workflow.

#### 13) Overseerr API

* Auth: API key + cookie-based auth options
* Docs:

```text
https://api-docs.overseerr.dev/
```

([api-docs.overseerr.dev][13])

#### 14) Seerr (Jellyfin-focused request tool) API

* API docs are typically hosted inside the local install (`/api-docs`)
* Docs:

```text
https://github.com/seerr-team/seerr
https://docs.seerr.dev/
```

([GitHub][14])

---

### D) “Web UIs” that sit in front of torrent clients (usually not your best target)

Example: Flood is primarily a UI that talks to clients like rTorrent/Transmission. In a routing system, you generally target the **underlying client API**, not the UI service.

* Flood overview:

```text
https://github.com/jesec/flood
```

([GitHub][15])

ruTorrent’s “HTTP RPC” isn’t consistently documented as a stable public API; treat it as best-effort / community knowledge rather than a first-class connector. ([GitHub][16])

---

## Connector-friendly “service registry lite” pattern

### 1) Registry object model

Each connector is described by a **manifest file** (user-readable, shipped with the app; overrides supported):

* `id`, `name`, `kind` (torrent-client | automation | request | custom)
* `capabilities`:

  * `accepts`: `["magnet", "torrent"]`
  * `supportsDownloadDir`, `supportsLabels`, `supportsPaused`, `supportsCategory`, `supportsTags`
* `endpoints`:

  * base path(s), health-check path, add-torrent path style (multipart vs JSON-RPC vs XML-RPC)
* `auth` scheme(s):

  * `apiKeyHeader`, `basicAuth`, `bearerToken`, `cookieSession`, `rpcSecret`, `custom`
* `configSchema` (UI generation):

  * fields (host, port, tls, path, username, password, apiKey, rpcSecret, etc.)
  * validations (required, URL format, numeric range)
  * defaults

### 2) Service configuration pane (KDE)

The pane should be **generated from `configSchema`**, with consistent UX across connectors:

* Service list:

  * Toggle **Enabled**
  * Status indicator (reachable/auth OK/failed)
  * “Test connection” button
* Service detail:

  * Connection: base URL, TLS, path, timeout
  * Authentication: dropdown + fields required by that auth type
  * Capabilities preview (read-only, from manifest)
  * “Set as default target” option
  * Per-service options: default download dir, tags/labels mapping, paused-by-default, category mapping
* Diagnostics:

  * show last test time + last error
  * show which endpoint was used for test

### 3) Secrets handling with KWallet

Recommended split:

* **Non-secret config** stored as files in your app data dir (e.g., TOML/JSON).
* **Secrets** stored in KWallet; config file stores only a reference key, e.g.:

  * `secretRef = "kwallet:<wallet>/<folder>/<entry>"`

Implementation details:

* One wallet folder per connector instance: `VARAI/<serviceId>/<instanceName>/`
* Store each secret as a separate entry:

  * `apiKey`, `password`, `token`, `rpcSecret`
* “Reveal” buttons should require explicit user action and ideally re-auth to wallet (depending on user settings).
* Support “disable service” without deleting secrets; add explicit “Forget credentials” action.

Security note for aria2: treat RPC exposure carefully; enforce strong defaults (loopback-only suggestion, TLS or reverse proxy, secret required) because the RPC interface is powerful. ([aria2.github.io][5])

Here’s a **comprehensive list of “destination” services** you can route `.torrent` files and `magnet:` links to from your KDE app, including *self-hosted clients*, *cloud torrenting/seedbox APIs*, and *related SaaS-style services* — plus pointers to **API docs** and **what auth they require**. You can implement each as a **connector** in your service registry and configure credentials securely (e.g., via KWallet).

---

# **1) Native Torrent Client APIs (self-hosted)**

These are classic torrent clients with stable remote APIs.

| Service                | Input                     | Auth                       | API Docs                                                                                                                                                           |
| ---------------------- | ------------------------- | -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **qBittorrent**        | magnet & torrent          | Session login              | [https://github.com/qbittorrent/qBittorrent/wiki/WebUI-API-%28qBittorrent-5.0%29](https://github.com/qbittorrent/qBittorrent/wiki/WebUI-API-%28qBittorrent-5.0%29) |
| **Transmission**       | magnet & torrent          | Basic auth + session token | [https://github.com/transmission/transmission/blob/main/docs/rpc-spec.md](https://github.com/transmission/transmission/blob/main/docs/rpc-spec.md)                 |
| **Deluge Web UI**      | magnet & torrent          | JSON WebRPC login          | [https://deluge.readthedocs.io/en/latest/reference/webapi.html](https://deluge.readthedocs.io/en/latest/reference/webapi.html)                                     |
| **rTorrent (XML-RPC)** | magnet & torrent          | Proxy auth / host layer    | [https://rtorrent-docs.readthedocs.io/en/latest/cmd-ref.html](https://rtorrent-docs.readthedocs.io/en/latest/cmd-ref.html)                                         |
| **aria2 (JSON-RPC)**   | magnet & (torrent via BT) | Secret token               | [https://aria2.github.io/manual/en/html/aria2c.html](https://aria2.github.io/manual/en/html/aria2c.html)                                                           |

> For these, implement a connector that handles auth, builds the appropriate RPC call to add a torrent or magnet, and optionally set download directory/paused state.

---

# **2) Cloud Torrent / Seedbox Services (SaaS targets)**

These let you offload torrent fetching to their cloud, then pull the completed files.

## **TorBox**

* **Description:** Cloud torrent/seedbox service with API. You can add magnet/torrent links and it will fetch and optionally cache them for download. ([api.torbox.app][1])
* **API Overview:** There’s an official OpenAPI available showing endpoints like torrent creation, status, etc. ([api.torbox.app][1])
* **Auth:** Access token provided in account; passed to API calls. ([PyPI][2])
* **API docs:** OpenAPI/Swagger UI at [https://api.torbox.app/docs](https://api.torbox.app/docs) (you can base your connector on this). ([api.torbox.app][1])

## **put.io**

* **Description:** Cloud torrent fetch/download service — paste magnet/torrent link to start a transfer; files live in user’s cloud storage. ([put.io][3])
* **API:** Well-documented, supports OAuth2 + REST for transfers and file management. (Docs at [https://api.put.io](https://api.put.io) — not always publicly indexed but official).
* **Auth:** OAuth2 tokens (client ID/Client Secret + user consent).
* **Typical Use:** Create a *transfer* (add magnet/torrent to cloud), list files, then fetch via HTTP/WebDAV. ([guides.viren070.me][4])

## **Premiumize.me**

* **Description:** Cloud torrent downloader + cloud storage + Usenet + filehost support. Remote Downloader accepts magnet links and torrents. ([premiumize.me][5])
* **API docs:** Hosted at SwaggerHub: [https://app.swaggerhub.com/apis-docs/premiumize.me/api](https://app.swaggerhub.com/apis-docs/premiumize.me/api)
* **Auth:** API key (customer ID + API key), OAuth2, or pairing codes. ([premiumize.me][6])
* **Typical Operations:** Create a *transfer* (add magnet/torrent to cloud), list transfers, generate direct download links. ([premiumize.me][5])

---

# **3) Related Automation / Request Targets (via API)**

Even though these aren’t torrent downloaders, integrating with them lets your app be part of larger media workflows.

| Service       | Role                                    | Auth    | API Docs                                                           |
| ------------- | --------------------------------------- | ------- | ------------------------------------------------------------------ |
| **Sonarr**    | TV automation (send torrents to client) | API key | [https://sonarr.tv/docs/api/](https://sonarr.tv/docs/api/)         |
| **Radarr**    | Movie automation                        | API key | [https://radarr.video/docs/api/](https://radarr.video/docs/api/)   |
| **Lidarr**    | Music automation                        | API key | [https://lidarr.audio/docs/api/](https://lidarr.audio/docs/api/)   |
| **Readarr**   | Books automation                        | API key | [https://readarr.com/docs/api/](https://readarr.com/docs/api/)     |
| **autobrr**   | Torrent automation hub                  | API key | [https://autobrr.com/api](https://autobrr.com/api)                 |
| **Overseerr** | Media request system (requests)         | API key | [https://api-docs.overseerr.dev/](https://api-docs.overseerr.dev/) |

These connectors can let your app either *send downloads to clients already configured in the automation system* or *create request tickets* that downstream tools fulfill.

---

# **4) Secondary Targets / Seedbox Integrations**

Often you use clients that have remote APIs exposed (e.g., rTorrent via SCGI, Flood web UI), but it’s better to talk directly to the client rather than a UI layer.

* **Flood** (web UI for rTorrent/other backends) — UI only, not a stable public API; generally avoid as a first-class target.
* **ruTorrent** — primarily UI; upstream APIs aren’t standardized; implement only if you need server-side integration via plugins.

---

# **Connector Capability Matrix (draft)**

For each service, your connector should express supported features:

| Service           | Accepts Magnet | Accepts Torrent | Supports Direct REST | Auth Type     | Notes                       |
| ----------------- | -------------- | --------------- | -------------------- | ------------- | --------------------------- |
| qBittorrent       | ✓              | ✓               | WebUI REST           | Session login | upload file or URL          |
| Transmission      | ✓              | ✓               | JSON-RPC             | Basic + token | add via `torrent-add`       |
| Deluge            | ✓              | ✓               | JSON WebRPC          | login token   | supports file & URL         |
| rTorrent          | ✓              | ✓               | XML-RPC              | Basic         | add via `load_start`        |
| aria2             | ✓              | ✓               | JSON-RPC             | secret        | add via `addTorrent`/magnet |
| TorBox            | ✓              | ✓               | REST (OpenAPI)       | Access token  | cloud torrent service       |
| put.io            | ✓              | ✓               | OAuth2 REST          | OAuth2 token  | transfers API               |
| Premiumize.me     | ✓              | ✓               | Swagger REST         | API key/OAuth | cloud transfers             |
| Sonarr/Radarr/... | ✓              | ✗               | REST                 | API key       | dispatch to client          |

---

# **Service Configuration Pane Design (secure auth)**

Your config UI should let the user **enable/disable services** and securely supply credentials. Example schema per service:

```
serviceId (string)           // unique connector handle
enabled (bool)               // toggle on/off
baseUrl (url)                // API root
authType (enum)              // e.g.: ApiKey, OAuth2, Basic, Cookie
credentials:
  - For ApiKey: keyString
  - For OAuth2: clientId, clientSecret, accessToken, refreshToken
  - For Basic: username, password
  - For Cookie: sessionCookie
capabilities (read-only)     // features the service supports
defaultOptions (object)      // e.g. default dest folder, paused state
```

## **Secrets storage (recommended)**

Use KWallet to store all **secrets**:

* Store per-service credentials in KWallet entries:

  * `VARAI/<serviceId>/<instance>/<credentialType>`
  * e.g., `VARAI/putio/primary/accessToken`
* App only stores *references* to wallet entries in the config files.

## **UI fields by auth**

### **API key**

* API Key (masked)
* Option to *Reveal in Wallet*

### **OAuth2**

* Client ID
* Client Secret (wallet)
* Authorize button → browser trust flow
* Access token (wallet)
* Refresh token (wallet)

### **Basic**

* Username
* Password (wallet)

### **Token**

* Token value (wallet)
* Expiry + refresh controls

## **Common UI controls**

* Test connection (show success/failure message from health check)
* Show supported features (e.g., accept magnet/torrent, can set download dir)
* Default download folder per service
* Default pause/label/category options
* Option: “Use as default dispatch target”

---

# **Example API Resources (quick links)**

* ➤ **TorBox API docs (OpenAPI)**: [https://api.torbox.app/docs/](https://api.torbox.app/docs/) (explorable REST API) ([api.torbox.app][1])
* ➤ **Premiumize.me API on SwaggerHub**: [https://app.swaggerhub.com/apis-docs/premiumize.me/api](https://app.swaggerhub.com/apis-docs/premiumize.me/api) (with endpoints for transfers & jobs) ([premiumize.me][5])
* ➤ **put.io REST API (developer portal)**: [https://put.io/docs](https://put.io/docs) (OAUTH2 + transfer endpoints) ([put.io][3])

---

## Implementation Tips

* Always include **health check** endpoint per connector to show reachable/auth OK.
* For cloud services, show **transfer status** (queued, downloading, ready).
* Abstract “add torrent/magnet” so UI doesn’t need to know protocol details: connector → send appropriate call.
* Respect rate limits and auth flows (OAuth2 refresh).

---

Full github CI/CD pipeline
