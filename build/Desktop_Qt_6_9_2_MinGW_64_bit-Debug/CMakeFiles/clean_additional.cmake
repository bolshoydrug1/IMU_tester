# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\IMU_tester_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\IMU_tester_autogen.dir\\ParseCache.txt"
  "IMU_tester_autogen"
  )
endif()
