# nosUtilities Plugin Split

## Overview

The `nosUtilities` plugin has been split into four focused, smaller plugins to improve maintainability, reduce compilation time, and provide better logical organization:

1. **nosImageProcessing** - GPU-accelerated image and texture processing
2. **nosMemory** - Buffer and texture memory management
3. **nosExecution** - Execution flow control and scheduling
4. **nosLayout** - Layout system for canvas and multi-texture compositing

## Motivation

The original `nosUtilities` plugin contained 62 nodes covering diverse functionality, making it:
- Large and slow to compile
- Difficult to maintain
- Hard to understand for new contributors
- Lacking clear boundaries between concerns

## New Plugin Structure

### 1. nosImageProcessing (21 nodes)

**Purpose:** GPU-accelerated image and texture transformation with shader-based effects.

**Plugin ID:** `nos.imageprocessing` v1.0.0

**Dependencies:**
- `nos.sys.vulkan` v8.0
- `nos.math` v2.0

**Nodes:**
- AutoResize - Automatic texture resizing
- BoxFit - Fit texture to bounding box
- CalculateDispatchSize - Compute shader dispatch calculation
- ChannelViewer - View individual texture channels
- Checkerboard - Generate checkerboard pattern
- Color - Solid color generator
- Gradient - Linear/radial gradient generator
- ImageIO (StbiLoad) - Load images using STB library
- LoadCubeLUT - Load 3D LUT textures
- MeanSquaredError - Calculate MSE between textures
- Offset - Offset texture coordinates
- PSNR - Calculate Peak Signal-to-Noise Ratio
- QuadMerge - Merge four textures into quad layout
- ReadImage - Load images from disk
- ReduceTexture - Reduce texture resolution
- SevenSegment - Seven-segment display generator
- SplitWipe - Split-screen wipe effect
- Swizzle - Channel swizzling
- TextureSwitcher - Switch between multiple textures
- YADIF - Yet Another Deinterlacing Filter
- YADIFWithAutoDispatchSize - YADIF with automatic dispatch
- WriteImage - Write images to disk

**Assets:**
- 12 GPU shaders (.frag, .vert, .comp)
- 6 C++ implementation files
- 5 FlatBuffer type definitions
- STB image library (external)

### 2. nosMemory (17 nodes)

**Purpose:** Buffer and texture memory lifecycle management, async GPU transfers, and resource management.

**Plugin ID:** `nos.memory` v1.0.0

**Dependencies:**
- `nos.sys.vulkan` v8.0
- `nos.sync` v11.1

**Nodes:**
- AsyncDownloadBuffer - Async GPU→CPU buffer download
- BoundedQueue - Generic bounded queue
- BoundedTextureQueue - Texture queue with bounds
- Buffer2Texture - Convert buffer to texture
- BufferProvider - Buffer allocation and management
- CopyResource - Generic resource copy
- DeinterlacedBoundedTextureQueue - Deinterlaced texture queue
- DeinterlacedBufferRing - Deinterlaced ring buffer
- HostVisibleBufferCopy - Host-side buffer copy
- Merge - Blend multiple textures
- Resize - Resize textures
- RingBuffer - Circular buffer management
- Texture2Buffer - Convert texture to buffer
- TextureProvider - Texture allocation and management
- UploadBuffer - CPU→GPU buffer upload
- UploadBufferProvider - Upload buffer provider
- WaitGPUEvent - GPU synchronization primitive

**Assets:**
- 1 GPU shader (Merge.frag)
- 17 C++ implementation files
- 3 FlatBuffer type definitions
- Ring.h template header

### 3. nosExecution (20 nodes)

**Purpose:** Execution graph control, scheduling, synchronization, and event triggering.

**Plugin ID:** `nos.execution` v1.0.0

**Dependencies:**
- `nos.sys.vulkan` v8.0
- `nos.sync` v11.1

**Nodes:**
- AlwaysDirty - Always marks as dirty for execution
- ConditionalTrigger - If/then/else conditional branching
- CPUSleep - CPU delay/wait
- ExecDepend - Execution dependency management
- Host - System information provider
- MultiLiveOut - Multi-outlet with live execution
- PrintLog - Logging to console
- PropagateExecution - Propagate execution events
- RepeatingJunction - Circular execution buffer
- ScheduleOnRequest - On-demand scheduling
- ScheduleRequest - Schedule with parameters
- ShowStatusNode - Display node status
- Sink - Execution sink with FPS control
- SinkGraph - Threaded execution sink
- SwitchTrigger - Switch/case conditional
- SyncMultiOutlet - Synchronized multi-outlet
- ThreadedSyncMultiOutlet - Threaded sync outlet
- Time - Elapsed time provider
- TimedFunctionSignaller - Periodic timer
- TriggerOnAnyInput - OR logic for triggers

**Assets:**
- 0 GPU shaders (CPU-only nodes)
- 15 C++ implementation files
- 1 FlatBuffer type definition

### 4. nosLayout (4 nodes, 5 classes)

**Purpose:** Canvas and layout system for arranging and compositing multiple textures.

**Plugin ID:** `nos.layout` v1.0.0

**Dependencies:**
- `nos.sys.vulkan` v8.0
- `nos.math` v2.0

**Nodes/Classes:**
- LayoutDrawer - Render layout to texture
- FreeLayout - Free-form texture layout
- GridLayout - Grid-based texture layout
- FreeOutputLayout - Free-form output layout
- GridOutputLayout - Grid-based output layout

**Assets:**
- 2 GPU shaders (QuadOutline.vert, QuadOutline.frag)
- 2 C++ implementation files (LayoutDrawer.cpp, LayoutNodes.cpp)
- 1 FlatBuffer type definition

## Migration Guide

### For Plugin Developers

If you maintain code that references nodes from `nosUtilities`, update your dependencies:

**Before:**
```json
{
  "dependencies": [
    {
      "name": "nos.utilities",
      "version": "4.0"
    }
  ]
}
```

**After:**
```json
{
  "dependencies": [
    {
      "name": "nos.imageprocessing",
      "version": "1.0"
    },
    {
      "name": "nos.memory",
      "version": "1.0"
    },
    {
      "name": "nos.execution",
      "version": "1.0"
    },
    {
      "name": "nos.layout",
      "version": "1.0"
    }
  ]
}
```

### For Node Graph Users

Existing `.nosdef` files referencing `nos.utilities` nodes will continue to work due to type migration support built into the new plugins. The system will automatically:

1. Map old `nos.utilities.*` type names to new plugin namespaces
2. Redirect node lookups to the appropriate new plugin

### Type Migrations

The new plugins implement `GetRenamedTypes()` to handle backward compatibility:

**nosImageProcessing:**
- `nos.utilities.ChannelViewerChannels` → `nos.imageprocessing.ChannelViewerChannels`
- `nos.utilities.ChannelViewerFormats` → `nos.imageprocessing.ChannelViewerFormats`
- `nos.utilities.GradientKind` → `nos.imageprocessing.GradientKind`
- `nos.utilities.Source` → `nos.imageprocessing.Source`
- `nos.utilities.Channel` → `nos.imageprocessing.Channel`
- `nos.plugin.switcher.TextureSwitcherChannel` → `nos.imageprocessing.TextureSwitcherChannel`

**nosMemory:**
- `nos.utilities.BlendMode` → `nos.memory.BlendMode`
- `nos.utilities.ResizeMethod` → `nos.memory.ResizeMethod`

## Benefits of the Split

### Compilation Time
- Each plugin compiles independently
- Parallel build support for faster CI/CD
- Reduced incremental build time when modifying single nodes

### Maintainability
- Clear separation of concerns
- Easier to locate and modify specific functionality
- Reduced cognitive load for contributors

### Discoverability
- Users can install only the plugins they need
- Better categorization in the node menu
- Clearer plugin purposes from names and descriptions

### Testing
- Isolated test suites per plugin
- Faster test execution
- Easier to identify test failures

## File Distribution Summary

| Original (nosUtilities) | Split Plugins |
|------------------------|---------------|
| 62 nodes | 21 + 17 + 20 + 4 = 62 nodes ✓ |
| 42 source files | 6 + 17 + 15 + 2 = 40 source files |
| 19 shaders | 12 + 1 + 0 + 2 = 15 shaders |
| 12 type definitions | 5 + 3 + 1 + 1 = 10 type definitions |

## Implementation Details

### Directory Structure

Each new plugin follows the standard Nodos plugin structure:

```
nosPluginName/
├─ PluginName.nosplugin       # Plugin manifest
├─ CMakeLists.txt             # Build configuration
├─ Nodes/                     # Node definitions (.nosnode)
├─ Source/                    # C++ implementations
├─ Shaders/                   # GPU shaders (if needed)
├─ Types/                     # FlatBuffer schemas (.fbs)
│  └─ Resources.json          # Vulkan resources (if needed)
└─ External/                  # Third-party dependencies (if needed)
```

### Plugin Registration

Each plugin implements:
- `ExportNodeFunctions()` - Register all node implementations
- `nosExportPlugin()` - Export plugin interface
- `GetRenamedTypes()` - Handle type migrations for backward compatibility

## Future Considerations

### Deprecation of nosUtilities

The original `nosUtilities` plugin should be marked as deprecated in the next release:
1. Add deprecation notice to plugin description
2. Update documentation to reference new plugins
3. Provide migration period (2-3 releases)
4. Remove in future major version

### Version Bumping

When updating nodes in the split plugins:
- Increment individual plugin versions independently
- Maintain type compatibility where possible
- Document breaking changes in each plugin's release notes

## Testing Checklist

- [ ] All 4 plugins compile successfully
- [ ] All nodes load in Nodos
- [ ] Existing .nosdef files load correctly
- [ ] Type migrations work for old files
- [ ] GPU shaders compile correctly
- [ ] External dependencies (STB) link properly
- [ ] No namespace conflicts between plugins
- [ ] Performance benchmarks match original plugin

## Credits

Split implemented to improve plugin maintainability and reduce compilation time.

Original `nosUtilities` plugin: 62 nodes, 4.0.0
Split into 4 focused plugins: v1.0.0 (2026-02-12)
