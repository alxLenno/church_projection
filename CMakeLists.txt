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

find_package(Qt6 COMPONENTS Widgets Multimedia MultimediaWidgets OpenGLWidgets WebEngineWidgets Network REQUIRED)
qt_standard_project_setup()

# macOS App Icon
set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/assets/AppIcon.icns")
set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

add_executable(ChurchProjection WIN32 MACOSX_BUNDLE
    ${APP_ICON_MACOS}
    main.cpp
    resources.qrc
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

set_target_properties(ChurchProjection PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.lennoxkk.ChurchProjection"
    MACOSX_BUNDLE_BUNDLE_NAME "ChurchProjection"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.2.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.2.0"
)

if(WIN32)
    enable_language(RC)
    configure_file(assets/windows.rc.in "${CMAKE_CURRENT_BINARY_DIR}/windows.rc" @ONLY)
    target_sources(ChurchProjection PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/windows.rc")
endif()

# Link Qt
target_link_libraries(ChurchProjection Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets Qt6::OpenGLWidgets Qt6::WebEngineWidgets Qt6::Network)

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
    install(DIRECTORY assets/bible assets/default_themes DESTINATION ChurchProjection.app/Contents/Resources/assets PATTERN "*.bak" EXCLUDE PATTERN ".DS_Store" EXCLUDE)
else()
    install(DIRECTORY assets/bible assets/default_themes DESTINATION assets PATTERN "*.bak" EXCLUDE PATTERN ".DS_Store" EXCLUDE)
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
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    find_program(MACDEPLOYQT_EXE macdeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(MACDEPLOYQT_EXE)
        install(CODE "
            message(STATUS \"Running macdeployqt...\")
            execute_process(
                COMMAND \"${MACDEPLOYQT_EXE}\" \"\${CMAKE_INSTALL_PREFIX}/ChurchProjection.app\" -always-overwrite -libpath=\"${Qt6_DIR}/../../../lib\"
                RESULT_VARIABLE res
            )
            if(NOT res EQUAL 0)
                message(FATAL_ERROR \"macdeployqt failed: \${res}\")
            endif()
            execute_process(
                COMMAND \"${Python3_EXECUTABLE}\" \"${CMAKE_SOURCE_DIR}/scripts/fix-macos-dependencies.py\" \"\${CMAKE_INSTALL_PREFIX}/ChurchProjection.app\"
                RESULT_VARIABLE repair_result
            )
            if(NOT repair_result EQUAL 0)
                message(FATAL_ERROR \"Mac runtime dependency repair failed\")
            endif()
            execute_process(
                COMMAND /usr/bin/codesign --force --deep --sign - \"\${CMAKE_INSTALL_PREFIX}/ChurchProjection.app\"
                RESULT_VARIABLE sign_result
            )
            if(NOT sign_result EQUAL 0)
                message(FATAL_ERROR \"Mac app signing failed\")
            endif()
        ")
    endif()
endif()

# 4. CPack Configuration
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_NAME "ChurchProjection")
set(CPACK_PACKAGE_VENDOR "LennoxKK")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Church Projection System")
set(CPACK_PACKAGE_VERSION "1.2.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ChurchProjection 1.2.0")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "ChurchProjection 1.2.0")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE") # Optional: Create a LICENSE file if needed

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")
    set(CPACK_NSIS_DISPLAY_NAME "Church Projection 1.2.0")
    set(CPACK_NSIS_PACKAGE_NAME "Church Projection 1.2.0")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/assets/AppIcon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/assets/AppIcon.ico")
    # Explicit shortcuts are independent of the optional NSIS menu page.
    set(CPACK_PACKAGE_EXECUTABLES "")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS [=[
      CreateDirectory "$SMPROGRAMS\ChurchProjection 1.2.0"
      CreateShortCut "$SMPROGRAMS\ChurchProjection 1.2.0\ChurchProjection.lnk" "$INSTDIR\bin\ChurchProjection.exe" "" "$INSTDIR\bin\ChurchProjection.exe" 0
      CreateShortCut "$DESKTOP\ChurchProjection 1.2.0.lnk" "$INSTDIR\bin\ChurchProjection.exe" "" "$INSTDIR\bin\ChurchProjection.exe" 0
    ]=])
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [=[
      Delete "$SMPROGRAMS\ChurchProjection 1.2.0\ChurchProjection.lnk"
      Delete "$DESKTOP\ChurchProjection 1.2.0.lnk"
      RMDir "$SMPROGRAMS\ChurchProjection 1.2.0"
    ]=])
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "ChurchProjection")
    set(CPACK_DMG_FORMAT "ULMO")
    # set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/dmg_setup.scpt") # Optional - disabled as file missing
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)

# Exercise the real projection renderer and native fullscreen transitions.
include(CTest)
if(BUILD_TESTING)
    add_executable(ProjectionSmoke tests/projection-smoke.cpp ui/ProjectionWindow.cpp ui/ProjectionWindow.h)
    target_include_directories(ProjectionSmoke PRIVATE "${CMAKE_SOURCE_DIR}")
    target_link_libraries(ProjectionSmoke PRIVATE Qt6::Widgets Qt6::Multimedia)
    add_test(NAME projection-fullscreen COMMAND ProjectionSmoke)
    set_tests_properties(projection-fullscreen PROPERTIES TIMEOUT 20)
endif()
