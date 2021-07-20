cmake_minimum_required(VERSION 3.16...3.20)
project(obs-ndi VERSION 5.0.0)
set(LIBNDI_VERSION 5.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prohibit in-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" _LOC_PATH)
if(EXISTS "${LOC_PATH}")
	message(FATAL_ERROR "obs-ndi: You cannot build in a source directory (or any directory with "
						"CMakeLists.txt file). Please make a build subdirectory. Feel free to "
						"remove CMakeCache.txt and CMakeFiles.")
endif()
unset(_LOC_PATH)


# Allow selection of common build types via UI
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
		"OBS build type [Release, RelWithDebInfo, Debug, MinSizeRel]" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release RelWithDebInfo Debug MinSizeRel)
endif()


# Qt build stuff
set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_AUTORCC ON) # For resources.qrc


# Arm build fixes
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
	set(CMAKE_CXX_FLAGS "-mfpu=neon")
endif()


# Find libobs
if (WIN32 OR APPLE)
	include(cmake/FindLibObs.cmake)
endif()
find_package(LibObs REQUIRED)


# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)


# Configure files
configure_file(
	src/obs-ndi-macros.h.in
	../src/obs-ndi-macros.generated.h
)
configure_file(
	installer/installer-windows.iss.in
	../installer/installer-windows.generated.iss)


# Include sources and headers
set(obs-ndi_SOURCES
	src/obs-ndi.cpp)

set(obs-ndi_HEADERS
	src/obs-ndi.h)


# Platform-independent build settings
add_library(obs-ndi MODULE
	${obs-ndi_SOURCES}
	${obs-ndi_HEADERS})

include_directories(
    "${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
    "lib/ndi")

target_link_libraries(obs-ndi
	libobs
	Qt5::Core
	Qt5::Widgets)


# Linux-specific build settings
if(UNIX AND NOT APPLE)
	include(GNUInstallDirs)

    set_target_properties(obs-ndi PROPERTIES PREFIX "")
	target_link_libraries(obs-ndi obs-frontend-api)

    # ARM optimizations
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") 
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -mtune=arm7 -Ofast")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -mtune=core2 -Ofast")
	endif()

	# Manually set permissions for locales
	file(GLOB locale_files data/locale/*.ini)
		set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
		OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    # Set target binary install directory
	install(TARGETS obs-ndi LIBRARY
		DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins"
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ) # Manually set file permissions for binary

    # OBS on Ubuntu installs into a different directory than most linux distros
	if(${USE_UBUNTU_FIX})
		install(TARGETS obs-ndi LIBRARY
			DESTINATION "/usr/lib/obs-plugins"
			PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
	endif()

    # Set target locale install directory
	install(FILES ${locale_files}
		DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/obs-ndi/locale")
endif()
