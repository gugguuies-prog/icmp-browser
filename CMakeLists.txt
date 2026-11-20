# CMakeLists.txt for ICMP Browser
cmake_minimum_required(VERSION 3.15)
project(ICMPBrowser VERSION 1.0.0 LANGUAGES C CXX)

# C/C++ Standards
set(CMAKE_C_STANDARD 11)
if(USE_CEF)
    set(CMAKE_CXX_STANDARD 20)  # CEF requires C++20
else()
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_CLIENT "Build Windows client" ON)
option(BUILD_SERVER "Build Linux server" ON)
option(USE_CEF "Use CEF instead of WebView2" ON)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
else()
    set(PLATFORM_LINUX TRUE)
endif()

# CEF Configuration
if(USE_CEF AND PLATFORM_WINDOWS)
    set(CEF_ROOT "C:/cef" CACHE PATH "Path to CEF root directory")
    set(CEF_INCLUDE_PATH "${CEF_ROOT}")
    set(CEF_LIBRARY_DEBUG "${CEF_ROOT}/Debug/libcef.lib")
    set(CEF_LIBRARY_RELEASE "${CEF_ROOT}/Release/libcef.lib")
    set(CEF_DLL_WRAPPER_DEBUG "${CEF_ROOT}/build/libcef_dll_wrapper/Debug/libcef_dll_wrapper.lib")
    set(CEF_DLL_WRAPPER_RELEASE "${CEF_ROOT}/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib")
    
    message(STATUS "Using CEF from: ${CEF_ROOT}")
endif()

# Find required packages
if(PLATFORM_WINDOWS AND NOT USE_CEF)
    # WebView2 SDK (installed via vcpkg)
    find_package(unofficial-webview2 CONFIG QUIET)
endif()

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

# Common source files
set(COMMON_SOURCES
    src/common/protocol.c
    src/common/handshake.c
    src/common/priority_queue.c
    src/common/compression.c
    src/common/statistics.c
)

# Crypto source files
set(CRYPTO_SOURCES
    src/crypto/aes_gcm.c
    src/crypto/diffie_hellman.c
)

# ============================================================================
# Windows Client
# ============================================================================
if(PLATFORM_WINDOWS AND BUILD_CLIENT)
    
    if(USE_CEF)
        # CEF Browser
        add_executable(icmpbrowser WIN32
            src/browser/cef_browser.cpp
            src/client/icmp_engine.c
            ${COMMON_SOURCES}
            ${CRYPTO_SOURCES}
        )
        
        target_include_directories(icmpbrowser PRIVATE
            ${CEF_INCLUDE_PATH}
        )
        
        target_link_libraries(icmpbrowser PRIVATE
            debug ${CEF_LIBRARY_DEBUG}
            optimized ${CEF_LIBRARY_RELEASE}
            debug ${CEF_DLL_WRAPPER_DEBUG}
            optimized ${CEF_DLL_WRAPPER_RELEASE}
            ${OPENSSL_LIBRARIES}
            ${ZLIB_LIBRARIES}
            Threads::Threads
            iphlpapi
            ws2_32
            comctl32
            shlwapi
            dbghelp
            psapi
            version
            winmm
            propsys
        )
        
        # CEF-specific compiler flags
        target_compile_definitions(icmpbrowser PRIVATE
            WIN32
            _WIN32
            _WINDOWS
            UNICODE
            _UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _HAS_EXCEPTIONS=0
        )
        
        target_compile_options(icmpbrowser PRIVATE
            /MP
            /W3
            /wd4100 /wd4127 /wd4244 /wd4512 /wd4701 /wd4702 /wd4996
        )
        
        set_target_properties(icmpbrowser PROPERTIES
            WIN32_EXECUTABLE TRUE
            LINK_FLAGS "/SUBSYSTEM:WINDOWS /MANIFEST:NO"
        )
        
        # Copy CEF binaries
        add_custom_command(TARGET icmpbrowser POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_ROOT}/Release/libcef.dll"
                "$<TARGET_FILE_DIR:icmpbrowser>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_ROOT}/Release/chrome_elf.dll"
                "$<TARGET_FILE_DIR:icmpbrowser>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CEF_ROOT}/Resources"
                "$<TARGET_FILE_DIR:icmpbrowser>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CEF_ROOT}/Release"
                "$<TARGET_FILE_DIR:icmpbrowser>"
        )
        
        message(STATUS "Building CEF browser: icmpbrowser.exe")
        
    else()
        # WebView2 Browser
        set(CLIENT_SOURCES
            src/client/icmp_engine.c
            src/client/http_proxy.c
            src/client/main.cpp
            src/client/ui/main_window.cpp
            src/client/ui/webview_controller.cpp
            src/client/ui/status_bar.cpp
            src/client/ui/settings_dialog.cpp
        )
        
        add_executable(icmpbrowser WIN32
            ${CLIENT_SOURCES}
            ${COMMON_SOURCES}
            ${CRYPTO_SOURCES}
        )
        
        target_link_libraries(icmpbrowser PRIVATE
            ${OPENSSL_LIBRARIES}
            ${ZLIB_LIBRARIES}
            Threads::Threads
            iphlpapi
            ws2_32
            comctl32
            unofficial::webview2::webview2
        )
        
        set_target_properties(icmpbrowser PROPERTIES
            WIN32_EXECUTABLE TRUE
            LINK_FLAGS "/SUBSYSTEM:WINDOWS"
        )
        
        message(STATUS "Building WebView2 browser: icmpbrowser.exe")
    endif()
    
    # Simple browser (no WebView2 dependency)
    add_executable(icmp_browser_simple WIN32
        src/browser/icmp_browser_simple.cpp
        src/browser/html_renderer.cpp
        src/client/icmp_engine.c
        ${COMMON_SOURCES}
        ${CRYPTO_SOURCES}
    )
    
    target_link_libraries(icmp_browser_simple PRIVATE
        ${OPENSSL_LIBRARIES}
        ${ZLIB_LIBRARIES}
        Threads::Threads
        iphlpapi
        ws2_32
        comctl32
    )
    
    set_target_properties(icmp_browser_simple PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    message(STATUS "Building simple browser: icmp_browser_simple.exe")
    
endif()

# ============================================================================
# Linux Server
# ============================================================================
if(PLATFORM_LINUX AND BUILD_SERVER)
    
    # Server sources
    set(SERVER_SOURCES
        src/server/icmp_handler.c
        src/server/session_manager.c
        src/server/tun_device.c
    )
    
    # Server library
    add_library(icmp_server_lib STATIC
        ${SERVER_SOURCES}
        ${COMMON_SOURCES}
        ${CRYPTO_SOURCES}
    )
    
    target_link_libraries(icmp_server_lib PUBLIC
        ${OPENSSL_LIBRARIES}
        ${ZLIB_LIBRARIES}
        Threads::Threads
    )
    
    # Server executable
    add_executable(icmp_server
        src/server/main.c
    )
    
    target_link_libraries(icmp_server PRIVATE
        icmp_server_lib
        ${OPENSSL_LIBRARIES}
        ${ZLIB_LIBRARIES}
        Threads::Threads
    )
    
    message(STATUS "Building Linux server: icmp_server")
    
endif()

# ============================================================================
# Tests
# ============================================================================
if(BUILD_TESTS)
    enable_testing()
    
    # Helper function to add a test
    function(add_icmp_test TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
        
        if(PLATFORM_WINDOWS)
            target_link_libraries(${TEST_NAME} PRIVATE
                ${OPENSSL_LIBRARIES}
                ${ZLIB_LIBRARIES}
                Threads::Threads
                iphlpapi
                ws2_32
            )
        else()
            target_link_libraries(${TEST_NAME} PRIVATE
                ${OPENSSL_LIBRARIES}
                ${ZLIB_LIBRARIES}
                Threads::Threads
            )
        endif()
        
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()
    
    # Crypto tests
    add_icmp_test(test_aes_gcm tests/test_aes_gcm.c ${CRYPTO_SOURCES})
    add_icmp_test(test_dh tests/test_dh.c ${CRYPTO_SOURCES})
    
    # Protocol tests
    add_icmp_test(test_protocol tests/test_protocol.c 
                  ${COMMON_SOURCES} ${CRYPTO_SOURCES})
    add_icmp_test(test_handshake tests/test_handshake.c 
                  ${COMMON_SOURCES} ${CRYPTO_SOURCES})
    
    # Optimization tests
    add_icmp_test(test_priority_queue tests/test_priority_queue.c
                  src/common/priority_queue.c)
    add_icmp_test(test_compression tests/test_compression.c
                  src/common/compression.c)
    add_icmp_test(test_statistics tests/test_statistics.c
                  src/common/statistics.c)
    
    # HTTP Proxy tests
    add_icmp_test(test_http_proxy tests/test_http_proxy.c
                  src/client/http_proxy.c
                  src/client/icmp_engine.c
                  src/common/protocol.c
                  src/crypto/aes_gcm.c
                  src/crypto/diffie_hellman.c)
    
    # Platform-specific tests
    if(PLATFORM_WINDOWS)
        add_icmp_test(test_icmp_engine tests/test_icmp_engine.c
                      src/client/icmp_engine.c)
        
        # Test d'encapsulation réelle (requête HTTP via ICMP)
        add_icmp_test(test_encapsulation_real tests/test_encapsulation_real.c
                      src/client/icmp_engine.c
                      ${COMMON_SOURCES}
                      ${CRYPTO_SOURCES})
    endif()
    
    if(PLATFORM_LINUX)
        add_icmp_test(test_icmp_handler tests/test_icmp_handler.c
                      ${SERVER_SOURCES})
        add_icmp_test(test_session_manager tests/test_session_manager.c
                      src/server/session_manager.c)
        add_icmp_test(test_tun_device tests/test_tun_device.c
                      src/server/tun_device.c)
    endif()
    
    message(STATUS "Building tests enabled")
    
endif()

# ============================================================================
# Install targets
# ============================================================================
if(PLATFORM_WINDOWS AND BUILD_CLIENT)
    install(TARGETS icmpbrowser
            RUNTIME DESTINATION bin)
endif()

# Print summary
message(STATUS "========================================")
message(STATUS "ICMP Browser Build Configuration")
message(STATUS "========================================")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "========================================")
