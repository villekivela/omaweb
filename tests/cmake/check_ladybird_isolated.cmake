# Ladybird is configured and compiled only by a build that asks for it, so an
# edit to the Qt browser never touches it (ADR 0001, ADR 0073).

file(READ "${OMAWEB_SOURCE_DIR}/CMakeLists.txt" root_cmake)
file(READ "${OMAWEB_SOURCE_DIR}/CMakePresets.json" presets_json)

if(NOT root_cmake MATCHES "option\\(OMAWEB_BUILD_LADYBIRD [^\n]* OFF\\)")
    message(FATAL_ERROR "OMAWEB_BUILD_LADYBIRD must be an option that defaults to OFF")
endif()

# One guarded door: the root file names the Ladybird tree once, inside the
# option's test.
string(REGEX MATCHALL "engine/ladybird" mentions "${root_cmake}")
list(LENGTH mentions mention_count)
if(NOT mention_count EQUAL 1
        OR NOT root_cmake MATCHES
            "if\\(OMAWEB_BUILD_LADYBIRD\\)[ \t\n]+add_subdirectory\\(src/engine/ladybird\\)")
    message(FATAL_ERROR
        "src/engine/ladybird must be added only by if(OMAWEB_BUILD_LADYBIRD) in CMakeLists.txt")
endif()

# Only the `ladybird` preset, and no preset built on it, turns the option on.
string(JSON preset_count LENGTH "${presets_json}" configurePresets)
math(EXPR last_preset "${preset_count} - 1")
foreach(index RANGE ${last_preset})
    string(JSON name GET "${presets_json}" configurePresets ${index} name)
    string(JSON enabled ERROR_VARIABLE no_value
        GET "${presets_json}" configurePresets ${index} cacheVariables OMAWEB_BUILD_LADYBIRD)
    if(no_value STREQUAL "NOTFOUND" AND enabled AND NOT name STREQUAL "ladybird")
        message(FATAL_ERROR "preset ${name} enables Ladybird; only `ladybird` may")
    endif()
    string(JSON inherited ERROR_VARIABLE no_parent
        GET "${presets_json}" configurePresets ${index} inherits)
    if(no_parent STREQUAL "NOTFOUND" AND inherited STREQUAL "ladybird")
        message(FATAL_ERROR "preset ${name} inherits the Ladybird build")
    endif()
    if(name STREQUAL "ladybird")
        set(saw_ladybird TRUE)
        if(NOT no_value STREQUAL "NOTFOUND" OR NOT enabled)
            message(FATAL_ERROR "the ladybird preset must set OMAWEB_BUILD_LADYBIRD")
        endif()
    endif()
endforeach()
if(NOT saw_ladybird)
    message(FATAL_ERROR "CMakePresets.json has no ladybird preset")
endif()
