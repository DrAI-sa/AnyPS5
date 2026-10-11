add_library(libSceNetCtlApDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNetCtlApDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNetCtlApDialog)

if(BUILD_TESTING)
    add_test_executable(guest_netctl_ap_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestNetCtlApDialog.cpp)
    target_include_directories(guest_netctl_ap_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_netctl_ap_dialog_tests PRIVATE libSceNetCtlApDialog)
    add_test(NAME guest_netctl_ap_dialog COMMAND guest_netctl_ap_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_netctl_ap_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNetCtlApDialog>")
    endif()
endif()
