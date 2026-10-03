# Multiplayer Networking Subsystem (`src/net/`)

The `net` subsystem provides client-server UDP packet transport, deterministic terrain seeding, delta-compressed voxel mutations, and player snapshot reconciliation.

---

## 📁 Source Files

| File | Primary Responsibility | Key Classes / Structs |
| :--- | :--- | :--- |
| [`packet_types.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/packet_types.hpp) | Binary packet layouts, bit-packed headers, button flags, serialization utilities. | [`PacketHeader`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/packet_types.hpp#L28), [`PacketType`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/packet_types.hpp#L10), [`BlockDeltaPacket`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/packet_types.hpp#L110) |
| [`net_host.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/net_host.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/net_host.cpp) | Authoritative host server socket, client session tracking, state broadcasting. | [`NetHost`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/net_host.hpp#L16) |
| [`net_client.hpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/net_client.hpp) / [`.cpp`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/net_client.cpp) | Client UDP socket, local prediction, snapshot interpolation, delta application. | [`NetClient`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/net_client.hpp#L16) |

---

## 📦 Packet Types & Protocol Structure

Every packet begins with a packed 12-byte header (`sizeof(PacketHeader) == 12`):
- `magic`: `0x5644` ("VD" for Voidfall Dredge)
- `type`: `PacketType` enum (1..13)
- `flags`: Reliable bit (0x01), Compressed bit (0x02)
- `sequence`: Outgoing sequence identifier
- `ack`: Acknowledged sequence number from remote peer

### Core Packet Types

| Type | Name | Purpose |
| :--- | :--- | :--- |
| `1` | `HandshakeRequest` | Client joins server with protocol version and player handle. |
| `2` | `HandshakeResponse` | Server assigns player ID, tick rate, and initial network parameters. |
| `3` | `WorldSeedSync` | Shared 32-bit world generation seed ensuring deterministic terrain across peers. |
| `4` | `PlayerInput` | Client transmission of compressed movement and button bitmasks. |
| `5` | `PlayerStateSnapshot` | Server-authoritative position, velocity, and health for all connected delvers. |
| `6` | `BlockDelta` | Compact packet synchronizing mined or placed voxels across the session. |
| `7` | `DynamicDebrisSpawn` | Spawns synchronized falling boulder rigid bodies after structural cave-ins. |
| `8` | `DynamicDebrisSync` | Position and angular rotation updates for active debris clusters. |
| `9` | `SonarPingBroadcast` | Relays seismic sonar pulse pings so team members share mineral outlines. |
| `11`| `ExtractionState` | Synchronizes beacon countdown, emergency siren strobe, and pod touchdown. |
| `12`| `HazardClockSync` | Synchronizes radiation levels and impending tremor warnings. |

---

## 🌐 Deterministic Voxel Networking Principle

Instead of transmitting millions of voxel blocks across the network:
1. Both client and server generate the exact same cavern layout from a shared 32-bit `world_seed`.
2. Only mutations (drilling blocks, placing bulkheads, or cave-in collapses) transmit a 10-byte [`BlockDeltaPacket`](file:///d:/Projects/voxel_3d_voidfall_dredge/src/net/packet_types.hpp#L110).
3. This achieves extreme bandwidth efficiency ($< 5\text{ KB/s}$ during heavy mining).
