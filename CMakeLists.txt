cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(VRBaseStation VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    FiltersCore
    FiltersGeneral
    IOGeometry
    InteractionStyle
    RenderingCore
    RenderingOpenGL2
    RenderingOpenVR
    RenderingAnnotation
    GUISupportQt
)

if(NOT TARGET VTK::RenderingOpenVR)
    message(FATAL_ERROR "VTK was built without OpenVR support. Rebuild VTK with VTK_MODULE_ENABLE_VTK_RenderingOpenVR=YES.")
endif()

if(DEFINED ENV{OPENVR_DIR})
    set(OpenVR_INCLUDE_DIR "$ENV{OPENVR_DIR}/headers")
    set(OpenVR_LIBRARY "$ENV{OPENVR_DIR}/lib/win64/openvr_api.lib")
else()
    message(FATAL_ERROR "OPENVR_DIR environment variable not set. See README.md.")
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MainWindow.ui
    src/ModelPart.cpp
    src/ModelPart.h
    src/ModelPartList.cpp
    src/ModelPartList.h
    src/OptionDialog.cpp
    src/OptionDialog.h
    src/OptionDialog.ui
    src/AboutDialog.cpp
    src/AboutDialog.h
    src/AboutDialog.ui
    src/VRRenderThread.cpp
    src/VRRenderThread.h
    src/LightingHelper.h
    src/EnvironmentHelper.h
    src/app.rc
    icons/icons.qrc
)

add_executable(VRBaseStation WIN32 ${PROJECT_SOURCES})

target_include_directories(VRBaseStation PRIVATE
    ${OpenVR_INCLUDE_DIR}
    src/
)

target_link_libraries(VRBaseStation PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    ${VTK_LIBRARIES}
    ${OpenVR_LIBRARY}
)

vtk_module_autoinit(
    TARGETS VRBaseStation
    MODULES ${VTK_LIBRARIES}
)

add_custom_command(TARGET VRBaseStation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/vrbindings
        $<TARGET_FILE_DIR:VRBaseStation>/vrbindings
)

add_custom_command(TARGET VRBaseStation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:VRBaseStation>/assets
)

install(TARGETS VRBaseStation RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/vrbindings/ DESTINATION bin/vrbindings)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION bin/assets)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/cad/ DESTINATION cad FILES_MATCHING PATTERN "*.stl")

if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(WINDEPLOYQT_EXE)
        install(CODE "
            execute_process(COMMAND \"${WINDEPLOYQT_EXE}\"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                \"\${CMAKE_INSTALL_PREFIX}/bin/VRBaseStation.exe\")
        ")
    endif()

    foreach(vtk_lib IN LISTS VTK_LIBRARIES)
        get_target_property(lib_location ${vtk_lib} IMPORTED_LOCATION_RELEASE)
        if(lib_location)
            get_filename_component(lib_dir ${lib_location} DIRECTORY)
            install(DIRECTORY ${lib_dir}/ DESTINATION bin FILES_MATCHING PATTERN "*.dll")
        endif()
    endforeach()

    install(FILES $ENV{OPENVR_DIR}/bin/win64/openvr_api.dll DESTINATION bin OPTIONAL)
endif()

set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin COMPONENT RuntimeLibraries)
endif()

set(CPACK_PACKAGE_NAME "VR Base Station")
set(CPACK_PACKAGE_VENDOR "EEEE2076 Group 1")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VR visualisation of a Formula Student car")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "VR Base Station")
    set(CPACK_NSIS_PACKAGE_NAME "VR Base Station")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\VRBaseStation.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/icons/app.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/icons/app.ico")
    set(CPACK_PACKAGE_EXECUTABLES "VRBaseStation;VR Base Station")
    set(CPACK_CREATE_DESKTOP_SHORTCUTS "VRBaseStation")
    set(CPACK_NSIS_MENU_LINKS
        "bin/VRBaseStation.exe" "VR Base Station"
        "Uninstall.exe" "Uninstall VR Base Station"
    )
endif()

include(CPack)