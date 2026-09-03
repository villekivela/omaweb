# Derives the project version from the nearest release tag so the binary, the
# macOS bundle, and the tag can never disagree. Sets, in the caller's scope:
#
#   OMAWEB_VERSION        x.y.z, suitable for project(VERSION)
#   OMAWEB_VERSION_STRING the full description, e.g. 0.2.0-14-gabc1234-dirty
#   OMAWEB_BUILD_NUMBER   commits reachable from HEAD, monotonic across releases
#
# A tarball with no .git, a clone with no tags, and a machine with no git all
# fall back to OMAWEB_FALLBACK_VERSION below. Raise it when cutting a release
# from a tree that will be distributed without history.

set(OMAWEB_FALLBACK_VERSION "0.1.0")

function(_omaweb_git output)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} ${ARGN}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE result
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE status
    )
    if(NOT status EQUAL 0)
        set(result "")
    endif()
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

function(omaweb_resolve_version)
    set(version "${OMAWEB_FALLBACK_VERSION}")
    set(description "${OMAWEB_FALLBACK_VERSION}")
    set(build_number "0")

    find_package(Git QUIET)
    if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
        # --match keeps stray tags out. A CI checkout is often shallow and has
        # no tags at all, which is why every failure here falls through.
        _omaweb_git(described describe --tags --dirty --match "v[0-9]*")
        _omaweb_git(counted rev-list --count HEAD)

        if(described)
            set(description "${described}")
            string(REGEX MATCH "^v([0-9]+\\.[0-9]+\\.[0-9]+)" matched "${described}")
            if(matched)
                set(version "${CMAKE_MATCH_1}")
                string(REGEX REPLACE "^v" "" description "${described}")
            endif()
        endif()
        if(counted)
            set(build_number "${counted}")
        endif()
    endif()

    set(OMAWEB_VERSION "${version}" PARENT_SCOPE)
    set(OMAWEB_VERSION_STRING "${description}" PARENT_SCOPE)
    set(OMAWEB_BUILD_NUMBER "${build_number}" PARENT_SCOPE)
endfunction()

# A new tag changes the version but touches no file CMake watches, so tell it to
# reconfigure when the ref HEAD points at moves.
function(omaweb_watch_git_head)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD")
        return()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD" head_line LIMIT_COUNT 1)
    if(head_line MATCHES "^ref: (.*)$")
        set(ref "${CMAKE_CURRENT_SOURCE_DIR}/.git/${CMAKE_MATCH_1}")
        if(EXISTS "${ref}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ref}")
        endif()
    endif()
endfunction()
