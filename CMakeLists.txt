cmake_minimum_required(VERSION 3.20)

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
    GUISupportQt
)

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
    src/ModelPart.cpp
    src/ModelPart.h
    src/ModelPartList.cpp
    src/ModelPartList.h
    src/optiondialog.cpp
    src/optiondialog.h
    src/optiondialog.ui
    src/AboutDialog.cpp
    src/AboutDialog.h
    src/AboutDialog.ui
    
)

add_executable(VRBaseStation ${PROJECT_SOURCES})

target_include_directories(VRBaseStation PRIVATE src)

target_link_libraries(VRBaseStation PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    ${VTK_LIBRARIES}
)
vtk_module_autoinit(
    TARGETS VRBaseStation
    MODULES ${VTK_LIBRARIES}
)