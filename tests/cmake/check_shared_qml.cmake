file(GLOB shared_qml_files "${OMAWEB_UI_DIRECTORY}/*.qml")

foreach(shared_qml_file IN LISTS shared_qml_files)
    file(READ "${shared_qml_file}" shared_qml_contents)
    if(shared_qml_contents MATCHES "import[ \t]+QtWebEngine")
        message(FATAL_ERROR "QtWebEngine import leaked into shared QML: ${shared_qml_file}")
    endif()
endforeach()
