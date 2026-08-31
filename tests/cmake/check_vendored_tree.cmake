# A vendored tree is a byte-for-byte copy of upstream pinned by MANIFEST.json.
# An edit to a copy makes the next sync a merge, so fail the build instead of
# letting the drift settle in.
#
# Callers pass:
#   TANTO_VENDOR_ROOT      the directory holding MANIFEST.json
#   TANTO_VENDOR_TRACKED   glob under that root that must hold only tracked files
#   TANTO_VENDOR_NAME      what to call the tree in a failure
#   TANTO_VENDOR_ADVICE    what to do about a failure
#
# A manifest may also carry a `generated` entry: a file built from the copies
# and committed beside them, pinned by its own digest.

file(READ "${TANTO_VENDOR_ROOT}/MANIFEST.json" manifest)
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
        list(APPEND tracked "${TANTO_VENDOR_ROOT}/${vendored_path}")
        if(NOT EXISTS "${TANTO_VENDOR_ROOT}/${vendored_path}")
            list(APPEND problems "missing: ${vendored_path}")
        else()
            file(SHA256 "${TANTO_VENDOR_ROOT}/${vendored_path}" actual)
            if(NOT actual STREQUAL expected)
                list(APPEND problems "edited locally: ${vendored_path}")
            endif()
        endif()
    endforeach()
endif()

file(GLOB_RECURSE present "${TANTO_VENDOR_ROOT}/${TANTO_VENDOR_TRACKED}")
foreach(path IN LISTS present)
    if(NOT path IN_LIST tracked)
        file(RELATIVE_PATH relative "${TANTO_VENDOR_ROOT}" "${path}")
        list(APPEND problems "not in the manifest: ${relative}")
    endif()
endforeach()

set(generated "")
string(JSON generated_entry ERROR_VARIABLE no_generated GET "${manifest}" generated)
if(generated_entry AND NOT generated_entry STREQUAL "generated-NOTFOUND")
    string(JSON generated GET "${generated_entry}" path)
    string(JSON generated_expected GET "${generated_entry}" sha256)
    if(NOT EXISTS "${TANTO_VENDOR_ROOT}/${generated}")
        list(APPEND problems "missing: ${generated}")
    else()
        file(SHA256 "${TANTO_VENDOR_ROOT}/${generated}" generated_actual)
        if(NOT generated_actual STREQUAL generated_expected)
            list(APPEND problems "does not match its pin: ${generated}")
        endif()
    endif()
endif()

if(problems)
    list(JOIN problems "\n  " report)
    message(FATAL_ERROR
        "Vendored ${TANTO_VENDOR_NAME} does not match MANIFEST.json:\n  ${report}\n\n"
        "Vendored files are copies and must not be edited. ${TANTO_VENDOR_ADVICE}")
endif()

if(generated)
    message(STATUS "${file_count} vendored ${TANTO_VENDOR_NAME} files and ${generated} "
        "match the manifest (${manifest_ref})")
else()
    message(STATUS
        "${file_count} vendored ${TANTO_VENDOR_NAME} files match the manifest (${manifest_ref})")
endif()
