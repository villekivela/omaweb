# A vendored tree is a byte-for-byte copy of upstream pinned by MANIFEST.json.
# An edit to a copy makes the next sync a merge, so fail the build instead of
# letting the drift settle in.
#
# Callers pass:
#   OMAWEB_VENDOR_ROOT      the directory holding MANIFEST.json
#   OMAWEB_VENDOR_TRACKED   glob under that root that must hold only tracked files
#   OMAWEB_VENDOR_NAME      what to call the tree in a failure
#   OMAWEB_VENDOR_ADVICE    what to do about a failure
#
# A manifest may also carry a `generated` list: files built from the copies and
# committed beside them, each pinned by its own digest.

file(READ "${OMAWEB_VENDOR_ROOT}/MANIFEST.json" manifest)
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
        list(APPEND tracked "${OMAWEB_VENDOR_ROOT}/${vendored_path}")
        if(NOT EXISTS "${OMAWEB_VENDOR_ROOT}/${vendored_path}")
            list(APPEND problems "missing: ${vendored_path}")
        else()
            file(SHA256 "${OMAWEB_VENDOR_ROOT}/${vendored_path}" actual)
            if(NOT actual STREQUAL expected)
                list(APPEND problems "edited locally: ${vendored_path}")
            endif()
        endif()
    endforeach()
endif()

file(GLOB_RECURSE present "${OMAWEB_VENDOR_ROOT}/${OMAWEB_VENDOR_TRACKED}")
foreach(path IN LISTS present)
    if(NOT path IN_LIST tracked)
        file(RELATIVE_PATH relative "${OMAWEB_VENDOR_ROOT}" "${path}")
        list(APPEND problems "not in the manifest: ${relative}")
    endif()
endforeach()

set(generated "")
string(JSON generated_entries ERROR_VARIABLE no_generated GET "${manifest}" generated)
if(generated_entries AND NOT generated_entries STREQUAL "generated-NOTFOUND")
    string(JSON generated_count LENGTH "${generated_entries}")
    if(generated_count GREATER 0)
        math(EXPR last_generated "${generated_count} - 1")
        foreach(index RANGE 0 ${last_generated})
            string(JSON generated_entry GET "${generated_entries}" ${index})
            string(JSON generated_path GET "${generated_entry}" path)
            string(JSON generated_expected GET "${generated_entry}" sha256)
            list(APPEND generated "${generated_path}")
            if(NOT EXISTS "${OMAWEB_VENDOR_ROOT}/${generated_path}")
                list(APPEND problems "missing: ${generated_path}")
            else()
                file(SHA256 "${OMAWEB_VENDOR_ROOT}/${generated_path}" generated_actual)
                if(NOT generated_actual STREQUAL generated_expected)
                    list(APPEND problems "does not match its pin: ${generated_path}")
                endif()
            endif()
        endforeach()
    endif()
endif()

if(problems)
    list(JOIN problems "\n  " report)
    message(FATAL_ERROR
        "Vendored ${OMAWEB_VENDOR_NAME} does not match MANIFEST.json:\n  ${report}\n\n"
        "Vendored files are copies and must not be edited. ${OMAWEB_VENDOR_ADVICE}")
endif()

if(generated)
    list(JOIN generated ", " generated_names)
    message(STATUS "${file_count} vendored ${OMAWEB_VENDOR_NAME} files and ${generated_names} "
        "match the manifest (${manifest_ref})")
else()
    message(STATUS
        "${file_count} vendored ${OMAWEB_VENDOR_NAME} files match the manifest (${manifest_ref})")
endif()
