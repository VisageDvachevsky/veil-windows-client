# Experiments CMake configuration
# These are standalone test programs for debugging and investigation

add_executable(cross_platform_crypto_test cross_platform_crypto_test.cpp)
target_link_libraries(cross_platform_crypto_test PRIVATE veil_common)

# Issue #96: RetransmitBuffer performance benchmark
add_executable(retransmit_buffer_benchmark retransmit_buffer_benchmark.cpp)

# Issue #126: Shortcut creation test (Windows only)
if(WIN32)
  add_executable(test_shortcut_creation test_shortcut_creation.cpp)
  target_link_libraries(test_shortcut_creation PRIVATE veil_common)
  target_include_directories(test_shortcut_creation PRIVATE ${CMAKE_SOURCE_DIR}/src)
endif()
