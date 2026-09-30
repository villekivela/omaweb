# The Agent skill is where docs/development.md tells the reader to link it
# from, `share/omaweb/skills/omaweb`, under whatever prefix the package uses.
#
# An Agent finds a skill by its directory's name, so the file arriving under
# another name, or not at all, fails only when a reader follows the docs.

set(prefix "${OMAWEB_BINARY_DIR}/agent-skill-install")
file(REMOVE_RECURSE "${prefix}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${OMAWEB_BINARY_DIR}"
        --component agent-skill --prefix "${prefix}"
    RESULT_VARIABLE result
    OUTPUT_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installing the agent-skill component failed: ${result}")
endif()

set(installed "${prefix}/share/omaweb/skills/omaweb/SKILL.md")
if(NOT EXISTS "${installed}")
    message(FATAL_ERROR "The package installs no ${installed}.")
endif()
file(READ "${installed}" copy)
file(READ "${OMAWEB_SKILL}" source)
if(NOT copy STREQUAL source)
    message(FATAL_ERROR "${installed} is not integrations/agent/omaweb/SKILL.md.")
endif()
file(REMOVE_RECURSE "${prefix}")
