# Whether the build graph compiles QML ahead of time. Expected is `compiled`
# for a build that embeds the QML, and `source` for one that loads it from the
# working tree, where a compile step would only be a cost. The graph read is
# Ninja's, which every preset generates.
if(NOT EXISTS "${OMAWEB_BUILD_GRAPH}")
    message(FATAL_ERROR "No build graph at ${OMAWEB_BUILD_GRAPH}")
endif()
file(READ "${OMAWEB_BUILD_GRAPH}" build_graph)

# One representative file per module: the shared UI, the kit and the engine
# view. qmlcachegen names its output after the source file.
set(compiled_units Main_qml.cpp Style_qml.cpp Button_qml.cpp EngineView_qml.cpp)

if(OMAWEB_EXPECTED STREQUAL "compiled")
    foreach(unit IN LISTS compiled_units)
        if(NOT build_graph MATCHES "qmlcache/[^\n]*${unit}")
            message(FATAL_ERROR "The build graph compiles no ${unit}")
        endif()
    endforeach()
elseif(OMAWEB_EXPECTED STREQUAL "source")
    if(build_graph MATCHES "qmlcachegen")
        message(FATAL_ERROR "A source-tree build compiles QML ahead of time")
    endif()
else()
    message(FATAL_ERROR "OMAWEB_EXPECTED must be compiled or source, not ${OMAWEB_EXPECTED}")
endif()
