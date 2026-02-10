# CS2 Offset Auto-Updater

This project now includes an automatic offset updater that fetches the latest CS2 offsets from reliable sources.

## How It Works

The auto-updater system:

1. **On Startup**: Attempts to load cached offsets from `%APPDATA%\CS2-External\offsets_cache.json`
2. **Background Update**: Fetches latest offsets from:
   - Primary: `https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json`
   - Fallback: `https://raw.githubusercontent.com/diabloakar0/cs2-offsets/main/offsets.json`
3. **Caching**: Successfully fetched offsets are saved to cache for offline use
4. **Fallback**: If both online fetch and cache fail, uses hardcoded offsets (last known working)

## Features

- ✅ Automatic offset fetching on startup
- ✅ Offline cache support
- ✅ Multiple source fallbacks
- ✅ Non-blocking background updates
- ✅ Automatic cache updates

## Manual Update

If you want to manually trigger an offset update, you can call:

```cpp
COffsetUpdater::OffsetData offsets;
if (g_OffsetUpdater.FetchLatestOffsets(offsets))
{
    // Offsets fetched successfully
}
```

## Cache Location

Offsets are cached at: `%APPDATA%\CS2-External\offsets_cache.json`

## Notes

- The updater runs in a background thread and doesn't block application startup
- If internet is unavailable, cached offsets are used automatically
- Hardcoded fallback offsets ensure the application always works, even without cache

## Integration

The offset updater is automatically integrated into `Globals.cpp` and runs on first initialization. No additional setup required!

