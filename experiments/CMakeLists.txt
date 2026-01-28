# Experiments CMake configuration
# These are standalone test programs for debugging and investigation

add_executable(cross_platform_crypto_test cross_platform_crypto_test.cpp)
target_link_libraries(cross_platform_crypto_test PRIVATE veil_common)
