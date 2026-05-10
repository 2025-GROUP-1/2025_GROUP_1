cmake_minimum_required(VERSION 3.19)
project(VRBaseStation LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets Concurrent)
find_package(VTK REQUIRED)

qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ModelPart.cpp
    ModelPart.h
    ModelPartList.cpp
    ModelPartList.h
    VRRenderThread.cpp
    VRRenderThread.h
    icons.qrc
    optiondialog.cpp
    optiondialog.h
    optiondialog.ui
)

if(EXISTS "${CMAKE_SOURCE_DIR}/icons/VRBaseStationLogo.ico")
    set(_APP_ICON_RC "${CMAKE_SOURCE_DIR}/app_icon.rc")
else()
    set(_APP_ICON_RC "")
endif()

qt_add_executable(VRBaseStation
    WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
    ${_APP_ICON_RC}
)

target_include_directories(VRBaseStation PRIVATE "C:/OpenVr/headers")

target_link_libraries(VRBaseStation
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Concurrent
        ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS VRBaseStation
    MODULES ${VTK_LIBRARIES}
)

add_custom_target(VRBindings)
add_custom_command(TARGET VRBindings PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy_directory ${CMAKE_SOURCE_DIR}/vrbindings ${CMAKE_BINARY_DIR}/vrbindings)

# Copy VR bindings next to the exe after every build
add_custom_command(TARGET VRBaseStation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/vrbindings
        $<TARGET_FILE_DIR:VRBaseStation>/vrbindings
)

# Copy optional assets (skybox, garage) - won't fail if files are missing
if(EXISTS "${CMAKE_SOURCE_DIR}/room2.png")
    add_custom_command(TARGET VRBaseStation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/room2.png
            $<TARGET_FILE_DIR:VRBaseStation>/room2.png)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/VRBaseStation/room.jpg")
    add_custom_command(TARGET VRBaseStation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/VRBaseStation/room.jpg
            $<TARGET_FILE_DIR:VRBaseStation>/room.jpg)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/garage__warehouse.glb")
    add_custom_command(TARGET VRBaseStation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/garage__warehouse.glb
            $<TARGET_FILE_DIR:VRBaseStation>/garage__warehouse.glb
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/garage__warehouse.glb
            $<TARGET_FILE_DIR:VRBaseStation>/garage.glb)
endif()

# ─── Install rules ────────────────────────────────────────────
include(GNUInstallDirs)

# Main executable
install(TARGETS VRBaseStation
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Qt DLLs via windeployqt (--debug matches the debug VTK build)
get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin_dir}")

install(CODE "
    execute_process(
        COMMAND \"${WINDEPLOYQT}\"
            --debug
            --no-translations
            --no-compiler-runtime
            \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/VRBaseStation.exe\"
    )
")

# Icons, VR bindings, and optional skybox assets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/icons
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/vrbindings
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(EXISTS "${CMAKE_SOURCE_DIR}/room2.png")
    install(FILES ${CMAKE_SOURCE_DIR}/room2.png
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/VRBaseStation/room.jpg")
    install(FILES ${CMAKE_SOURCE_DIR}/VRBaseStation/room.jpg
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/garage__warehouse.glb")
    install(FILES ${CMAKE_SOURCE_DIR}/garage__warehouse.glb
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# MSVC debug CRT DLLs (needed because we build in Debug config against debug VTK)
set(_msvc_debug_crt "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC/14.50.35710/debug_nonredist/x64/Microsoft.VC145.DebugCRT")
if(EXISTS "${_msvc_debug_crt}")
    install(DIRECTORY "${_msvc_debug_crt}/"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILES_MATCHING PATTERN "*.dll"
    )
endif()

# OpenVR DLL
if(EXISTS "C:/OpenVr/bin/win64/openvr_api.dll")
    install(FILES "C:/OpenVr/bin/win64/openvr_api.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# VTK DLLs
install(DIRECTORY "C:/Program Files (x86)/VTK/bin/"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILES_MATCHING PATTERN "*.dll"
)

# ----------------------------
# CPack / NSIS Installer
# ----------------------------
set(CPACK_PACKAGE_NAME "VRBaseStation")
set(CPACK_PACKAGE_VENDOR "EEEE2076 Group 1")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VRBaseStation Qt VTK CAD Viewer")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VRBaseStation")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
else()
    set(CPACK_GENERATOR "ZIP")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/icons/VRBaseStationLogo.ico")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/icons/VRBaseStationLogo.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/icons/VRBaseStationLogo.ico")
endif()

set(CPACK_NSIS_DISPLAY_NAME "VRBaseStation")
set(CPACK_NSIS_PACKAGE_NAME "VRBaseStation")
set(CPACK_NSIS_CONTACT "EEEE2076 Group 1")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

set(CPACK_PACKAGE_EXECUTABLES "VRBaseStation" "VRBaseStation")

set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Uninstall VRBaseStation.lnk' '$INSTDIR\\\\Uninstall.exe'"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Uninstall VRBaseStation.lnk'"
)

include(CPack)
