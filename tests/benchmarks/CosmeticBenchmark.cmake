# The benchmark harness for issue #100. It is included from the top-level
# CMakeLists.txt of whichever revision is under measurement, so the same file
# builds the same harness against baseline and implementation sources.

if(NOT BUILD_TESTING OR NOT OMAWEB_BUILD_BROWSER)
    return()
endif()

qt_add_executable(omaweb-cosmetic-benchmark tests/benchmarks/cosmetic_benchmark.cpp)
target_link_libraries(omaweb-cosmetic-benchmark PRIVATE
    omaweb-engine-qt
    omaweb-content-blocking
    omaweb-platform
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
)
target_compile_definitions(omaweb-cosmetic-benchmark PRIVATE
    OMAWEB_QT_ENGINE_VIEW_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/engine/qt/EngineView.qml"
)
omaweb_strict_target(omaweb-cosmetic-benchmark)
