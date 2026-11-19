# ICMP Browser GUI Application

cmake_minimum_required(VERSION 3.15)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Source files
set(BROWSER_SOURCES
    icmp_browser_simple.cpp
    ../common/protocol.c
    ../common/handshake.c
    ../common/compression.c
    ../common/statistics.c
    ../common/priority_queue.c
    ../crypto/aes_gcm.c
    ../crypto/diffie_hellman.c
    ../client/icmp_engine.c
)

# Create executable
add_executable(icmp_browser WIN32 ${BROWSER_SOURCES})

# Include directories
target_include_directories(icmp_browser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(icmp_browser PRIVATE
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ws2_32
    iphlpapi
    comctl32
    user32
    gdi32
)

# Set Windows subsystem
set_target_properties(icmp_browser PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "ICMPBrowser"
)

# Copy to bin directory
install(TARGETS icmp_browser
    RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/bin
)
