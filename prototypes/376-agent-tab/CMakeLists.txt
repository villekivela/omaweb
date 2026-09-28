cmake_minimum_required(VERSION 3.30)
project(omaweb_376_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Gui GuiPrivate Quick WebEngineQuick)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(omaweb-376-probe main.cpp probe.cpp probe.h)
qt_add_qml_module(
    omaweb-376-probe
    URI AgentTab
    QML_FILES main.qml
    RESOURCES agent.html reader.html
)
target_link_libraries(
    omaweb-376-probe
    PRIVATE Qt6::Gui Qt6::GuiPrivate Qt6::Quick Qt6::WebEngineQuick
)
