# Record Track (COLMAP) Node

## Summary

A new node "Record Track (COLMAP)" added to the `nosTrack` plugin. It records incoming camera tracking data per frame and exports it in COLMAP's text format (`cameras.txt` + `images.txt`).

## Files Changed

### New files
- `Source/RecordTrackCOLMAP.cpp` — Node implementation
- `Config/RecordTrackCOLMAP.nosdef` — Node definition (pins, functions, metadata)

### Modified files
- `Source/TrackMain.cpp` — Added `RecordTrackCOLMAP` to the `TrackNode` enum and `ExportNodeFunctions` switch
- `Track.noscfg` — Added `Config/RecordTrackCOLMAP.nosdef` to `node_definitions`

## Node Design

### Pins
| Pin | Type | Direction | Description |
|-----|------|-----------|-------------|
| Track | `nos.track.Track` | Input | Incoming tracking data |
| Track Out | `nos.track.Track` | Output (only) | Pass-through of input |
| Output Directory | `string` | Property | Folder picker for output |
| Image Resolution | `nos.fb.vec2u` | Property | Width/height (default 1920x1080) |
| Record | `bool` | Property | Mirrors Record/Stop functions |
| Frame Count | `uint` | Output (only) | Frames in buffer |

### Functions
| Function | Behavior |
|----------|----------|
| Record | Validates folder is empty, clears buffer, starts recording. Orphaned while recording. |
| Stop | Stops recording (does NOT save). Orphaned while idle. |
| Save | Writes `cameras.txt` + `images.txt` to disk. Does not clear buffer. |
| Clear | Clears frame buffer and resets count. |
| Open Folder | Opens output directory in explorer (Windows) or xdg-open (Linux). |

### State Management
- Record pin and functions are kept in sync bidirectionally. A `SyncingRecordPin` bool guard prevents re-entrant loops between pin changes and function calls.
- Function orphan states: Record/Stop toggle via `SetNodeOrphanState` using a `Name -> UUID` map built in constructor.
- Status messages show recording state + frame count, and persist error messages (e.g., "Target folder is not empty") via `LastError` until user changes the output directory.
- Non-empty folder check: Recording fails with a FAILURE status if the target folder already has files.

### COLMAP Output Format
- `cameras.txt`: One OPENCV camera per frame — `fx, fy, cx, cy, k1, k2, p1, p2` derived from Track FOV, sensor size, pixel aspect ratio, lens distortion.
- `images.txt`: Per-frame pose — Euler angles converted to quaternion (world-to-camera), translation as `t = -R * C`.

## Known Review Points
- Euler-to-quaternion convention: The Track's rotation fields (roll/tilt/pan) are passed through `glm::quat(eulerRadians)` then inverted for COLMAP's world-to-camera convention. May need validation against actual tracker output.
- One camera per frame: Each frame gets its own camera entry. This handles zoom/FOV changes but may be unusual for COLMAP workflows with constant intrinsics.
- No `points3D.txt`: COLMAP expects this file too (can be empty). Not currently written.
- `std::system()` for Open Folder: Works but is a simple shell call. Could be replaced with platform APIs if needed.

## Build
```
./nodos dev build -p Project13 --target nosTrack
```
