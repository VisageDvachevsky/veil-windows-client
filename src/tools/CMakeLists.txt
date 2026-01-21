# VEIL Tools and Utilities
# These tools require transport layer and CLI11, which are not available on Windows yet

if(NOT WIN32)
  # Transport benchmark tool
  add_executable(veil-transport-bench
    transport_bench.cpp
  )

  target_link_libraries(veil-transport-bench PRIVATE
    veil_common
  )

  veil_set_warnings(veil-transport-bench)

  # Performance validation tool
  add_executable(veil-performance-validation
    performance_validation.cpp
  )

  target_link_libraries(veil-performance-validation PRIVATE
    veil_common
  )

  veil_set_warnings(veil-performance-validation)
endif()
