# Packaging for Windows

Since this application is built using C++ and Qt 6, you can easily package it for Windows by following these steps.

## Prerequisites

1.  **Install Visual Studio 2022**: Download from [visualstudio.microsoft.com](https://visualstudio.microsoft.com/). Include the **Desktop development with C++** workload.
2.  **Install Qt 6 for Windows**: Use the [Qt Online Installer](https://www.qt.io/download-open-source). Ensure you select **Qt 6.x** and the **MSVC 2022 64-bit** component.
3.  **Install CMake**: Download from [cmake.org](https://cmake.org/download/).

## Building on Windows

1.  **Open Developer Command Prompt for VS 2022**.
2.  **Clone/Copy the project** to your Windows machine.
3.  **Create a build directory**:
    ```cmd
    cd church-projection
    mkdir build
    cd build
    ```
4.  **Configure with CMake**:
    *(Replace `C:\Qt\6.x.x\msvc2022_64` with your actual Qt installation path)*
    ```cmd
    cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64" ..
    ```
5.  **Build the project**:
    ```cmd
    cmake --build . --config Release
    ```

## Packaging & Deployment

To make the app run on other Windows computers without Qt installed, you need to deploy the dependencies.

### 1. Manual Deployment (Portable Folder)
1.  Locate `ChurchProjection.exe` (usually in `build\Release`).
2.  Run `windeployqt` (available in your Qt bin folder):
    ```cmd
    C:\Qt\6.x.x\msvc2022_64\bin\windeployqt.exe --multimedia --openglwidgets Release\ChurchProjection.exe
    ```
3.  The `Release` folder now contains all necessary DLLs and can be shared as a ZIP.

### 2. Automatic Installer (NSIS)
If you have [NSIS](https://nsis.sourceforge.io/Download) installed, you can generate a professional installer:
```cmd
cpack -C Release
```
This will create `ChurchProjection-1.1.0-win64.exe` in your build directory.

---

> [!TIP]
> **FFmpeg**: If you experience issues with video playback on Windows, ensure the `ffmpeg` backend for Qt Multimedia is active or that necessary codecs are installed. `windeployqt` usually handles the basic Qt dependencies.
