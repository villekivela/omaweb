# Whether the build graph compiles QML ahead of time. Expected is `compiled`
# for a build that embeds the QML, and `source` for one that loads it from the
# working tree, where a compile step would only be a cost.
#
# Each generator writes its graph its own way, so the graph is found by
# generator rather than under one file name: Ninja writes a single
# `build.ninja`, Make writes a `build.make` for every target. A generator this
# does not know fails rather than passes, because a check that found no graph
# has not looked at anything.
if(NOT IS_DIRECTORY "${OMAWEB_BUILD_DIRECTORY}")
    message(FATAL_ERROR "No build directory at ${OMAWEB_BUILD_DIRECTORY}")
endif()

if(OMAWEB_GENERATOR MATCHES "Ninja")
    set(graph_files "${OMAWEB_BUILD_DIRECTORY}/build.ninja")
elseif(OMAWEB_GENERATOR MATCHES "Makefiles")
    file(GLOB_RECURSE graph_files "${OMAWEB_BUILD_DIRECTORY}/CMakeFiles/*/build.make")
else()
    message(FATAL_ERROR
        "No build graph is known for the ${OMAWEB_GENERATOR} generator. "
        "Teach this check where that generator writes its graph.")
endif()

if(NOT graph_files)
    message(FATAL_ERROR "The ${OMAWEB_GENERATOR} generator wrote no build graph to read")
endif()

set(build_graph "")
foreach(graph_file IN LISTS graph_files)
    if(NOT EXISTS "${graph_file}")
        message(FATAL_ERROR "No build graph at ${graph_file}")
    endif()
    file(READ "${graph_file}" one_graph)
    string(APPEND build_graph "${one_graph}")
endforeach()

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
