---
description: How to cleanup invalid themes
---

1. Open `core/ThemeManager.h`
2. Locate the `loadTemplates()` function.
3. Ensure it iterates through the loaded settings and checks `QFileInfo::exists()` for each theme's content path.
4. If a file is missing, the entry should be skipped and `saveTemplates()` called to update the persistent storage.

Or simply run the application once, and the `ThemeManager` will automatically prune any missing files on startup.
