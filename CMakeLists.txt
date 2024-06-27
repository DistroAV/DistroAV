cmake_minimum_required(VERSION 3.16...3.26)

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

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Core Widgets Network)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

# cmake-format: off
# Add your custom source files here.
# Header files are optional and only required for visibility.
# e.g. in Xcode or Visual Studio.
# cmake-format: on
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/forms/obsndi-update.cpp
          src/forms/obsndi-update.h
          src/forms/output-settings.cpp
          src/forms/output-settings.h
          src/obs-support/obs-app.cpp
          src/obs-support/obs-app.h
          src/obs-support/shared-update.cpp
          src/obs-support/shared-update.h
          src/cloud.cpp
          src/cloud.h
          src/Config.cpp
          src/Config.h
          src/main-output.cpp
          src/main-output.h
          src/obs-ndi-filter.cpp
          src/obs-ndi-output.cpp
          src/obs-ndi-source.cpp
          src/plugin-main.cpp
          src/plugin-main.h
          src/premultiplied-alpha-filter.cpp
          src/preview-output.cpp
          src/preview-output.h)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/ndi)

# Firebase C++ SDK: Begin

# "`firebase_app` is required and must always be listed last"
set(firebase_libs firebase_app_check firebase_auth firebase_firestore firebase_functions firebase_app)
if(APPLE)
  set(ADDITIONAL_LIBS
      gssapi_krb5
      pthread
      "-framework CoreFoundation"
      "-framework Foundation"
      "-framework GSS"
      "-framework Security"
      "-framework SystemConfiguration")
elseif(MSVC)
  set(ADDITIONAL_LIBS advapi32 ws2_32 crypt32 iphlpapi psapi userenv)
else()
  set(ADDITIONAL_LIBS pthread)
endif()

set(FIREBASE_SDK_DIR ${CMAKE_SOURCE_DIR}/lib/firebase_cpp_sdk)
add_subdirectory(${FIREBASE_SDK_DIR} EXCLUDE_FROM_ALL)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${FIREBASE_SDK_DIR}/include)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${firebase_libs} ${ADDITIONAL_LIBS})

# Firebase C++ SDK: End

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
