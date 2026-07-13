# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\ViewerApp_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\ViewerApp_autogen.dir\\ParseCache.txt"
  "ViewerApp_autogen"
  )
endif()
