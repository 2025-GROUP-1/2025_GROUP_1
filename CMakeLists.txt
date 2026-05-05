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

qt_add_executable(VRBaseStation
    WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
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

include(GNUInstallDirs)

# Main executable
install(TARGETS VRBaseStation
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Qt DLLs via windeployqt
qt_generate_deploy_app_script(
    TARGET VRBaseStation
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# Icons, VR bindings, and skybox
install(DIRECTORY ${CMAKE_SOURCE_DIR}/icons
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/vrbindings
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${CMAKE_SOURCE_DIR}/room2.png
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${CMAKE_SOURCE_DIR}/VRBaseStation/room.jpg
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${CMAKE_SOURCE_DIR}/garage__warehouse.glb
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Microsoft Visual C++ runtime DLLs
include(InstallRequiredSystemLibraries)

# OpenVR DLL
if(EXISTS "C:/OpenVr/bin/win64/openvr_api.dll")
    install(FILES "C:/OpenVr/bin/win64/openvr_api.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# VTK DLLs - detected dynamically from linked targets
foreach(_vtk_lib ${VTK_LIBRARIES})
    if(TARGET ${_vtk_lib})
        get_target_property(_lib_type ${_vtk_lib} TYPE)
        if(_lib_type STREQUAL "SHARED_LIBRARY")
            get_target_property(_lib_loc ${_vtk_lib} IMPORTED_LOCATION_RELEASE)
            if(_lib_loc AND EXISTS "${_lib_loc}")
                install(FILES "${_lib_loc}" DESTINATION ${CMAKE_INSTALL_BINDIR})
            endif()
        endif()
    endif()
endforeach()

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

set(CPACK_NSIS_DISPLAY_NAME "VRBaseStation")
set(CPACK_NSIS_PACKAGE_NAME "VRBaseStation")
set(CPACK_NSIS_CONTACT "EEEE2076 Group 1")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

# Start menu shortcut for the app
set(CPACK_PACKAGE_EXECUTABLES "VRBaseStation" "VRBaseStation")

# Add uninstaller link to start menu (required by marking rubric)
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Uninstall VRBaseStation.lnk' '$INSTDIR\\\\Uninstall.exe'"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Uninstall VRBaseStation.lnk'"
)

include(CPack)
