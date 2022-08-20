# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "RelWithDebInfo")
  file(REMOVE_RECURSE
  "CMakeFiles/obs-ndi_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/obs-ndi_autogen.dir/ParseCache.txt"
  "obs-ndi_autogen"
  )
endif()
