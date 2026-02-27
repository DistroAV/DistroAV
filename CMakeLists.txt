cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Network)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/forms/output-settings.cpp
    src/forms/output-settings.h
    src/forms/update.cpp
    src/forms/update.h
    src/obs-support/obs-app.hpp
    src/obs-support/remote-text.cpp
    src/obs-support/remote-text.hpp
    src/config.cpp
    src/config.h
    src/main-output.cpp
    src/main-output.h
    src/ndi-filter.cpp
    src/ndi-finder.h
    src/ndi-finder.cpp
    src/ndi-output.cpp
    src/ndi-source.cpp
    src/ndi-shared.h
    src/plugin-main.cpp
    src/plugin-main.h
    src/premultiplied-alpha-filter.cpp
    src/preview-output.cpp
    src/preview-output.h
)

set(valid_uuid FALSE)
check_uuid(${WINDOWS_APP_UUID} valid_uuid)
if(NOT valid_uuid)
  message(FATAL_ERROR "Specified Windows package UUID is not a valid UUID value: ${WINDOWS_APP_UUID}")
endif()

configure_file(
  cmake/windows/resources/installer-Windows.iss.in
  "${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss"
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/ndi)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# -----------------------------------------------------------------------------
# Helper executable: ndi-server
# Build a small helper exe from src/ndi-server.cpp and place the resulting
# executable next to the plugin DLL so it's easy to locate and spawn.
# -----------------------------------------------------------------------------
add_executable(ndi-server
  src/ndi-server.cpp
)
install(TARGETS ndi-server
        RUNTIME DESTINATION distroav/bin/64bit)  # put executables under <prefix>/bin

# Use the same include paths required by the plugin (NDI headers and project src)
target_include_directories(ndi-server PRIVATE
  ${CMAKE_SOURCE_DIR}/lib/ndi
  ${CMAKE_SOURCE_DIR}/src
)

# Require C++14 for this target as well
set_target_properties(ndi-server PROPERTIES
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Apply MSVC-only optimization flags for Release/RelWithDebInfo builds
target_compile_options(ndi-server PRIVATE
  $<$<AND:$<C_COMPILER_ID:MSVC>,$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>>:/O2 /Oi /Ot /GL>
)

# If the NDI SDK provides a static library to link, you can link it here. For now
# the helper loads the NDI runtime dynamically, so no additional link libraries are required.

# End of ndi-server helper configuration
