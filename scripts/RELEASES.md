Desktop releases

macOS uses an .app bundle, not an .exe. Windows uses a compressed NSIS .exe installer that includes Qt runtime DLLs, video playback plugins, and the embedded browser helper.

Build locally on macOS with scripts/package-macos.sh. Set QT_PREFIX to the Qt installation that includes WebEngine and Multimedia. The local Homebrew Qt build requires macOS 26 and Apple Silicon. The GitHub workflow uses official Qt builds; check the downloaded bundle's minimum OS version before distributing it.

The smallest local archive uses xz -9e. Extract the .tar.xz and move ChurchProjection.app to Applications. The app is signed locally with an ad-hoc signature, not an Apple Developer ID, and has not been notarized.

On Windows, build with cmake --build build --config MinSizeRel and package with cpack -C MinSizeRel from build/. The NSIS installer uses solid LZMA compression. The GitHub Actions Build and Package workflow performs these steps and uploads installers as artifacts.

Packages include Bible XML files and default themes. Personal imported media, song libraries, settings, source files, and build caches are excluded. Existing user data is stored separately. Browser runtimes account for most of the distribution size and are retained for full functionality.
