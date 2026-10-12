include(CheckPIESupported)
check_pie_supported(LANGUAGES CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(HEADSET_SANITIZERS "Instrument project code with AddressSanitizer and UBSan" OFF)

function(headset_target_options target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wformat=2 -Werror=format-security
            -fstack-protector-strong
        )
        target_compile_definitions(${target} PRIVATE
            _GLIBCXX_ASSERTIONS
            "$<$<CONFIG:Release,RelWithDebInfo>:_FORTIFY_SOURCE=3>"
        )
        target_link_options(${target} PRIVATE -Wl,-z,relro,-z,now,-z,noexecstack)
        if(HEADSET_SANITIZERS)
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        endif()
    elseif(HEADSET_SANITIZERS)
        message(FATAL_ERROR "HEADSET_SANITIZERS requires GCC or Clang")
    endif()
endfunction()
