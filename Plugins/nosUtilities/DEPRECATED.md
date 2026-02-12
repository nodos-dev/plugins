# nosUtilities Plugin Deprecation Notice

## Status: DEPRECATED

The `nosUtilities` plugin (nos.utilities v4.0.0) has been split into four focused, smaller plugins:

1. **nosImageProcessing** (`nos.imageprocessing` v1.0.0) - GPU image/texture processing
2. **nosMemory** (`nos.memory` v1.0.0) - Buffer and memory management
3. **nosExecution** (`nos.execution` v1.0.0) - Execution flow control
4. **nosLayout** (`nos.layout` v1.0.0) - Layout system

## Reason for Split

The original nosUtilities plugin contained 62 diverse nodes, making it:
- Large and slow to compile
- Difficult to maintain
- Hard for new contributors to understand
- Lacking clear separation of concerns

## Migration Path

### For Users

Existing `.nosdef` files will continue to work. The new plugins implement automatic type migration to handle old references.

### For Developers

Update your plugin dependencies:

**Old:**
```json
{
  "dependencies": [
    { "name": "nos.utilities", "version": "4.0" }
  ]
}
```

**New:**
```json
{
  "dependencies": [
    { "name": "nos.imageprocessing", "version": "1.0" },
    { "name": "nos.memory", "version": "1.0" },
    { "name": "nos.execution", "version": "1.0" },
    { "name": "nos.layout", "version": "1.0" }
  ]
}
```

## Node Migration Map

See [PLUGIN_SPLIT.md](../PLUGIN_SPLIT.md) for complete node-to-plugin mapping.

### Quick Reference

| Old Node | New Plugin |
|----------|------------|
| Gradient, Checkerboard, Color, Swizzle, YADIF | nosImageProcessing |
| BufferProvider, TextureProvider, RingBuffer, Merge, Resize | nosMemory |
| ConditionalTrigger, Sink, Time, ScheduleOnRequest | nosExecution |
| LayoutDrawer, FreeLayout, GridLayout | nosLayout |

## Timeline

- **2026-02-12**: Split implemented, new plugins released (v1.0.0)
- **Current**: nosUtilities marked as deprecated
- **Next 2-3 releases**: Deprecation warning period
- **Future major version**: nosUtilities removed

## Documentation

- [PLUGIN_SPLIT.md](../PLUGIN_SPLIT.md) - Complete split documentation
- Migration guides for each new plugin in their respective directories

## Support

If you encounter issues with the migration, please file an issue in the repository.
