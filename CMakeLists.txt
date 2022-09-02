cmake_minimum_required(VERSION 3.16...3.21)

project(obs-ndi VERSION 5.0.0)
add_library(obs-ndi MODULE)
set(NDILIB_HEADERS_VERSION 5.0.10)

set(PLUGIN_AUTHOR "tt2468")

set(MACOS_BUNDLEID "net.tt2468.obs-ndi")

set(LINUX_MAINTAINER_EMAIL "tt2468@irltoolkit.com")

set(OBS_NDI_VERSION "${CMAKE_PROJECT_VERSION}")

# ----------------------------------------------------------------------

target_sources(
  obs-ndi
  PRIVATE src/obs-ndi.cpp
          src/obs-ndi.h
          src/output-manager.cpp
          src/output-manager.h
          src/config.cpp
          src/config.h
          src/input.cpp
          src/input.h
          src/input-utils.cpp
          src/output.cpp
          src/output.h
          src/aux-output.cpp
          src/aux-output.h
          src/plugin-macros.generated.h)

target_sources(obs-ndi PRIVATE src/forms/SettingsDialog.cpp src/forms/SettingsDialog.h
                               src/forms/resources.qrc)

target_include_directories(obs-ndi PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/lib/ndi)

# ----------------------------------------------------------------------

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
include(cmake/ObsPluginHelpers.cmake)
find_qt(COMPONENTS Widgets Core)

configure_file(src/plugin-macros.h.in ${CMAKE_SOURCE_DIR}/src/plugin-macros.generated.h)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-macros.generated.h)

target_include_directories(obs-ndi PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(obs-ndi OBS::libobs OBS::obs-frontend-api Qt::Core Qt::Widgets)

set_target_properties(
  obs-ndi
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

target_compile_features(obs-ndi PRIVATE cxx_std_17)

# ----------------------------------------------------------------------

# --- Windows-specific build settings and tasks ---
if(OS_WINDOWS)
  configure_file(cmake/bundle/windows/installer-Windows.iss.in
                 ${CMAKE_BINARY_DIR}/installer-Windows.generated.iss)

  if(MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4)
  endif()
  # --- End of section ---

  # -- macOS specific build settings and tasks --
elseif(OS_MACOS)
  configure_file(cmake/bundle/macos/installer-macos.pkgproj.in
                 ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)

  set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
  set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
  set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")

  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)

  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
  # --- End of section ---

  # --- Linux-specific build settings and tasks ---
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)

  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
endif()
# --- End of section ---

setup_plugin_target(${CMAKE_PROJECT_NAME})
