# Dropship Auto-Block Feature - Project Documentation

## Overview
Fork of [dropship](https://github.com/stowmyy/dropship) (Overwatch 2 server selector with Windows Firewall integration) adding **automatic route degradation detection** to block degraded IPs without manual intervention.

**Problem**: Players in SA region suffer intermittent packet loss and high latency due to route degradation at specific network hops, not server failure. Current dropship requires manual blocking. Players reconnect to the same degraded route repeatedly.

**Solution**: Auto-block monitors ping metrics and automatically blocks IPs when degradation is detected, preventing reconnection to degraded routes.

## Project Structure

```
dropship/dropship/
├── src/
│   ├── components/
│   │   ├── Endpoint.h/cpp        → Endpoint2 class: per-server UI element, ping state, block state
│   │   ├── EndpointRender.cpp    → UI rendering (needs visual indicator for auto-block)
│   │   └── PopupButton.h/cpp
│   ├── core/
│   │   ├── Firewall.h/cpp        → Windows Firewall integration via COM API
│   │   ├── Settings.h/cpp        → JSON-based app settings persistence
│   │   ├── Dashboard.h/cpp       → Main UI orchestrator
│   │   ├── Tunneling.h/cpp
│   │   └── Update.h/cpp
│   ├── util/
│   │   ├── ping/
│   │   │   ├── Pinger.h/cpp      → ICMP ping infrastructure (async via ASIO)
│   │   │   └── asio/             → ASIO TCP/IP stack integration
│   │   ├── watcher/              → Window/process monitoring
│   │   ├── win/                  → Windows-specific utilities
│   │   └── timer/                → Timer utility
│   ├── App.h/cpp                 → Main application loop
│   ├── main.cpp
│   └── pch.h                      → Precompiled headers
├── vendor/
│   ├── imgui-docking/            → Dear ImGui UI framework
│   └── stb_image/
└── dropship.sln

```

## Key Classes & Design

### Endpoint2 (components/Endpoint.h)
- **Current fields**: 
  - `title`: Display name (e.g., "Brazil")
  - `description`: Server code (e.g., "GBR1")
  - `ip_ping`: IP to ping (optional)
  - `blocked`: Current state
  - `blocked_desired`: Desired state (set by user or auto-block)
  - `pinger`: Unique pointer to Pinger for ICMP ping
  - `ping`: Last ping result (in ms)

- **Methods**:
  - `setBlockDesired(bool)`: Request block state change
  - `start_pinging()` / `stop_pinging()`: Control ICMP pinging thread
  - `render(int)`: Render UI element

### Pinger (util/ping/Pinger.h)
- Async ICMP ping using ASIO
- Constructor takes IP and shared ping result pointer
- Methods: `start()`, `stop()`, `ping()`
- Updates `ping` value asynchronously

### Firewall (core/Firewall.h)
- Windows Firewall COM API integration
- `tryWriteSettingsToFirewall(data, block, tunneling_path)`: Apply firewall rules
- `tryFetchSettingsFromFirewall()`: Read current firewall state
- Uses JSON for rule serialization

### Settings (core/Settings.h)
- Manages app settings (JSON-based, file-backed)
- Contains server definitions (`__ow2_servers`) and endpoint definitions (`__ow2_endpoints`)
- Options: `auto_update`, `ping_servers`, `tunneling`
- Config: `blocked_endpoints`, `tunneling_path`
- Methods: `toggleOptionAutoUpdate()`, `addBlockedEndpoint()`, etc.

## Build & Development

**Build System**: Visual Studio (`.sln` project)
- C++17 standard
- Windows-only (uses COM API, WinAPI)
- Dependencies: ImGui, ASIO, nlohmann/json (vendored)

**Development**:
- No automated tests in codebase yet
- Manual testing via running `.exe`
- Uses ImGui for all UI rendering

## Scope: Auto-Block v1 (from PRD)

### In Scope
1. **Monitoring**: Continuous packet loss / latency tracking per endpoint (reuse Pinger)
2. **Threshold Logic**: Configurable (e.g., >X% loss in Y-second window, or latency > Z ms sustained)
3. **Auto-Block Action**: Trigger `setBlockDesired(true)` when threshold breached
4. **Cooldown**: Avoid re-evaluating same IP for N minutes after action
5. **UI Indicator**: Visual diff between manual block (user) vs. auto-block (system)
6. **Log**: Simple text log with auto-block events (timestamp, IP, reason, packet loss %)
7. **Toggle**: Settings option to enable/disable auto-block (disabled by default in v1)

### Out of Scope
- Blocking during active game match (not technically possible)
- Route optimization (multipath, tunneling) — dropship only blocks/unblocks
- Per-endpoint threshold customization (v1 uses global threshold)
- Automatic ASN/ISP identification

## Architecture: New Modules

### AutoBlock (core/AutoBlock.h / .cpp) — NEW
Decision engine for auto-block logic:
- Consumes stream of ping results from an Endpoint2
- Maintains rolling average (e.g., last 20-30 pings)
- Detects threshold breach (packet loss % or sustained latency)
- Triggers block action and logs event
- Respects cooldown (no re-evaluation for N minutes)

### AutoBlockLog (core/AutoBlockLog.h / .cpp) — NEW
Event logging for auto-block actions:
- Simple struct with timestamp, IP, event type, packet loss %, latency
- Append-only log file or JSON array in settings
- Human-readable format for user review

### Integration Points

1. **Settings.h/cpp**
   - Add fields: `auto_block_enabled`, `auto_block_packet_loss_threshold_percent`, `auto_block_latency_threshold_ms`, `auto_block_window_seconds`, `auto_block_cooldown_minutes`
   - Add to JSON serialization (to_json / from_json)
   - Defaults: `enabled=false`, `loss_threshold=5%`, `latency_threshold=150ms`, `window=30s`, `cooldown=10min`

2. **Endpoint.h/cpp**
   - Add `AutoBlock* auto_blocker` member (lifecycle tied to Endpoint2)
   - Feed ping results to auto_blocker when new ping arrives
   - Add field: `blocked_reason_auto` (string explaining why auto-blocked)

3. **EndpointRender.cpp**
   - Add visual indicator (e.g., orange/red tint or label) for auto-blocked endpoints
   - Show reason on hover or in status text

4. **Dashboard.h/cpp**
   - Wire auto-block toggle from Settings to enable/disable per-endpoint monitoring

5. **main.cpp / App.cpp**
   - Ensure AutoBlockLog is loaded/saved with Settings

## Testing Strategy (v1)

1. **Unit**: Test AutoBlock threshold logic in isolation (mock ping data)
2. **Integration**: Test auto-block + firewall block in controlled environment
3. **Manual**: 
   - Simulate degradation by temporarily blocking a healthy IP in Windows Firewall (outside app)
   - Verify app detects timeout / packet loss and auto-blocks
   - Verify UI shows auto-block indicator
   - Verify log records event
   - Verify no block/unblock loop for 30+ minutes with unstable network

## Validation Criteria (Acceptance)

- [ ] Build compiles without errors (MSVC)
- [ ] With auto-block disabled, behavior identical to original dropship
- [ ] With auto-block enabled and degraded IP, blocks within SLA (< 60s detection)
- [ ] Auto-blocked endpoint visually differentiated from manual block
- [ ] Log file generated and legible
- [ ] No block/unblock loops in 30+ minute stress test
- [ ] Existing firewall block (manual) still works correctly

## Known Risks & Mitigations

1. **False Positives** (jitter)
   - **Risk**: Short window + high variance = incorrect blocks
   - **Mitigation**: Rolling average + configurable window + user review of log

2. **Game Impact** (block timing)
   - **Risk**: Player expects instant relief from block; relief only occurs between matches
   - **Mitigation**: Clear UI messaging + PRD validation that this is expected

3. **Maintenance Burden** (upstream changes)
   - **Risk**: Battle.net IP changes or Game updates break endpoint definitions
   - **Mitigation**: Same as original dropship; user reports issues, maintainer updates config

## Implementation Phases

1. **Setup**: Verify clean build of original dropship
2. **AutoBlock Module**: Implement decision engine, test in isolation
3. **Firewall Integration**: Connect AutoBlock to existing block mechanism
4. **Settings**: Add toggle + threshold config to UI
5. **UI Indicator**: Render auto-blocked state visually
6. **Logging**: Record auto-block events
7. **Build & Test**: Full integration test, controlled stress test
8. **Validation**: Real-world usage validation
