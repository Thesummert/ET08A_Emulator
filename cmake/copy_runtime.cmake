# TARGET_RUNTIME_DLLS can be empty when all dependencies are static.
string(REPLACE "|" ";" runtime_dlls "${DLL_LIST}")
foreach(runtime_dll IN LISTS runtime_dlls)
    if(EXISTS "${runtime_dll}")
        file(COPY "${runtime_dll}" DESTINATION "${DESTINATION}")
    endif()
endforeach()
