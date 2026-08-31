# The vendored Omarchy component kit is a byte-for-byte copy pinned by
# MANIFEST.json. An edit to a copy makes the next sync a merge, so fail the
# build instead of letting the drift settle in. Adapt in src/ui.

file(READ "${TANTO_OMARCHY_ROOT}/MANIFEST.json" manifest)
string(JSON manifest_files GET "${manifest}" files)
string(JSON manifest_ref GET "${manifest}" ref)
string(JSON file_count LENGTH "${manifest_files}")

set(problems "")
set(tracked "")
if(file_count GREATER 0)
    math(EXPR last_file "${file_count} - 1")
    foreach(index RANGE 0 ${last_file})
        string(JSON vendored_path MEMBER "${manifest_files}" ${index})
        string(JSON expected GET "${manifest_files}" "${vendored_path}" sha256)
        list(APPEND tracked "${TANTO_OMARCHY_ROOT}/${vendored_path}")
        if(NOT EXISTS "${TANTO_OMARCHY_ROOT}/${vendored_path}")
            list(APPEND problems "missing: ${vendored_path}")
        else()
            file(SHA256 "${TANTO_OMARCHY_ROOT}/${vendored_path}" actual)
            if(NOT actual STREQUAL expected)
                list(APPEND problems "edited locally: ${vendored_path}")
            endif()
        endif()
    endforeach()
endif()

file(GLOB_RECURSE present "${TANTO_OMARCHY_ROOT}/qs/*")
foreach(path IN LISTS present)
    if(NOT path IN_LIST tracked)
        file(RELATIVE_PATH relative "${TANTO_OMARCHY_ROOT}" "${path}")
        list(APPEND problems "not in the manifest: ${relative}")
    endif()
endforeach()

if(problems)
    list(JOIN problems "\n  " report)
    message(FATAL_ERROR
        "Vendored Omarchy kit does not match MANIFEST.json:\n  ${report}\n\n"
        "Vendored files are copies and must not be edited. Adapt in src/ui, or run\n"
        "scripts/sync_omarchy_ui.py --sync to restore them.")
endif()

message(STATUS "${file_count} vendored Omarchy files match the manifest (${manifest_ref})")
