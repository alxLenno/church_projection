cmake_minimum_required(VERSION 3.16)
project(ChurchProjection)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt path should be provided by system or -DCMAKE_PREFIX_PATH
# if(APPLE)
#     set(CMAKE_PREFIX_PATH "/Users/lennoxkk/Qt/6.10.2/macos")
# endif()

find_package(Qt6 COMPONENTS Widgets Multimedia MultimediaWidgets OpenGLWidgets REQUIRED)
qt_standard_project_setup()

# macOS App Icon
set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/assets/AppIcon.icns")
set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

add_executable(ChurchProjection MACOSX_BUNDLE
    ${APP_ICON_MACOS}
    main.cpp
    core/Song.h
    core/SongManager.h
    core/ThemeManager.h
    core/ThemeManager.cpp
    core/BibleManager.cpp
    core/PdfRenderer.h
    core/PdfRenderer.cpp
    ui/ControlWindow.cpp
    ui/ControlWindow.h
    ui/ProjectionPreview.cpp
    ui/ProjectionPreview.h
    ui/ProjectionWindow.cpp
    ui/ProjectionWindow.h
    ui/ThemeEditorDialog.cpp
    ui/ThemeEditorDialog.h
    ui/NotesWidget.cpp
    ui/NotesWidget.h
)

# Link Qt
target_link_libraries(ChurchProjection Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets Qt6::OpenGLWidgets)

# Link CoreGraphics on macOS for PDF rendering
if(APPLE)
    target_link_libraries(ChurchProjection "-framework CoreGraphics")
endif()

# Dev-mode asset path so BibleManager can find assets from source tree
target_compile_definitions(ChurchProjection PRIVATE
    CHURCH_PROJECTION_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# --- Installation and Packaging ---

# 1. Install Executable
install(TARGETS ChurchProjection
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# 2. Install Assets
# On Windows/Linux, install to "assets" folder next to binary
# On macOS, install to Resources/assets inside the bundle
if(APPLE)
    # Install directly into the bundle's Resources directory.
    # Note: DESTINATION is relative to CMAKE_INSTALL_PREFIX.
    install(DIRECTORY assets DESTINATION ChurchProjection.app/Contents/Resources)
else()
    install(DIRECTORY assets DESTINATION .)
endif()

# 3. Deployment Logic (Qt Dependencies)
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(WINDEPLOYQT_EXE)
        # Run windeployqt after build to populate bin/ with Qt DLLs
        # Note: For CPack, we usually want to run this on the INSTALLED directory, 
        # but running key windeployqt on the build output is a common simplified approach for ZIPs.
        # For NSIS, CPack handles some dependency collection, but windeployqt is safer.
        # We'll add a custom command to deploy to the build directory for testing, 
        # and assume CPack picks up the "installed" files. 
        # Actually, standard practice: use specific CPack Qt modules or install(CODE ...) calling windeployqt.
        
        # Simple approach: Install Qt plugins/libs explicitly or use a script.
        # Let's use the install(CODE) approach to run windeployqt on the installation step.
        install(CODE "
            message(STATUS \"Running windeployqt...\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXE}\" --no-translations --compiler-runtime \"\${CMAKE_INSTALL_PREFIX}/bin/ChurchProjection.exe\"
                WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin\"
                RESULT_VARIABLE res
            )
            if(NOT res EQUAL 0)
                message(FATAL_ERROR \"windeployqt failed with error: \${res}\")
            endif()
        ")
    endif()
elseif(APPLE)
    find_program(MACDEPLOYQT_EXE macdeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(MACDEPLOYQT_EXE)
        install(CODE "
            message(STATUS \"Running macdeployqt...\")
            execute_process(
                COMMAND \"${MACDEPLOYQT_EXE}\" \"\${CMAKE_INSTALL_PREFIX}/ChurchProjection.app\" -always-overwrite
            )
        ")
    endif()
endif()

# 4. CPack Configuration
set(CPACK_PACKAGE_NAME "ChurchProjection")
set(CPACK_PACKAGE_VENDOR "LennoxKK")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Church Projection System")
set(CPACK_PACKAGE_VERSION "1.1.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ChurchProjection")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE") # Optional: Create a LICENSE file if needed

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_DISPLAY_NAME "Church Projection")
    # set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico") # enable if you have an icon
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "ChurchProjection")
    # set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/dmg_setup.scpt") # Optional - disabled as file missing
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)
