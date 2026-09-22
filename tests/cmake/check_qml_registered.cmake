# Every QML file in the directory has to be in the module that ships it.
#
# A file on disk and not in the module works in a source build, where QML is
# read from the directory, and is simply absent from a compiled one, where the
# module is the only place QML lives. So it passes every test a developer runs
# and fails in the package: the type is "not a type", whatever used it fails to
# load, and the window with it.
#
# `omaweb-qml-load` catches that, but only in the release preset. This catches
# it wherever the tests run, and names the file rather than the symptom.

file(GLOB present "${OMAWEB_UI_DIRECTORY}/*.qml")

set(missing "")
foreach(file IN LISTS present)
    get_filename_component(name "${file}" NAME)
    # Matched with the directory, because a bare name could be satisfied by a
    # file of the same name in another module.
    if(NOT OMAWEB_REGISTERED MATCHES "(^|;)[^;]*/${name}(;|$)")
        list(APPEND missing "${name}")
    endif()
endforeach()

if(missing)
    list(JOIN missing "\n  " listed)
    message(FATAL_ERROR
        "QML in ${OMAWEB_UI_DIRECTORY} that no module ships:\n  ${listed}\n"
        "Add each to omaweb_ui_files in CMakeLists.txt. Until then a source "
        "build finds them and a package does not.")
endif()
