# The small client runs where only Qt's base is installed: a container, a
# Distrobox, a VM, another host. A library it links that the base does not
# carry is one such a machine does not have, and the client would not start
# there. So the libraries the built binary asks the loader for are read off it,
# rather than off the build's own account of what it links.

if(APPLE)
    execute_process(COMMAND otool -L "${OMAWEB_CLIENT}"
        OUTPUT_VARIABLE needed RESULT_VARIABLE result)
else()
    execute_process(COMMAND readelf --dynamic --wide "${OMAWEB_CLIENT}"
        OUTPUT_VARIABLE needed RESULT_VARIABLE result)
endif()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not read the libraries ${OMAWEB_CLIENT} links: ${result}")
endif()

# A check that reads nothing passes everything, so it has to find what the
# client does link before it is believed about what the client does not.
foreach(expected Core Network)
    if(NOT needed MATCHES "Qt6?${expected}")
        message(FATAL_ERROR "Found no Qt ${expected} among what the client links:\n${needed}")
    endif()
endforeach()

foreach(forbidden WebEngine Quick Qml Gui)
    if(needed MATCHES "Qt6?${forbidden}")
        message(FATAL_ERROR "The client links Qt ${forbidden}:\n${needed}")
    endif()
endforeach()
