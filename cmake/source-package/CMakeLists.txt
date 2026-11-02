# Standalone project that only creates the source tarball, without libobs, Qt or a compiler:
#   cmake -S cmake/source-package -B build_source && cmake --build build_source -t package_source
cmake_minimum_required(VERSION 3.28...3.30)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH _cmake_dir)
cmake_path(GET _cmake_dir PARENT_PATH _project_root)

file(READ "${_project_root}/buildspec.json" buildspec)
string(JSON _name GET ${buildspec} name)
string(JSON _version GET ${buildspec} version)

project(${_name} VERSION ${_version} LANGUAGES NONE)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_SOURCE_INSTALLED_DIRECTORIES "${_project_root};/")
set(CPACK_OUTPUT_FILE_PREFIX "${_project_root}/release")

include("${_project_root}/cmake/common/sourcepackage.cmake")

include(CPack)
