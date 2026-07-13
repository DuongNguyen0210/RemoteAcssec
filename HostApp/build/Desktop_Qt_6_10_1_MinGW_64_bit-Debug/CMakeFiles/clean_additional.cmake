# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\HostApp_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\HostApp_autogen.dir\\ParseCache.txt"
  "HostApp_autogen"
  )
endif()
