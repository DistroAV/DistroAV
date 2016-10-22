cmake_minimum_required(VERSION 3.5)
project(obs-ndi)

include(external/FindLibObs.cmake)
find_package(LibObs REQUIRED)

set(obs-ndi_SOURCES 
	obs-ndi.cpp
	obs-ndi-source.cpp)

set(obs-ndi_HEADERS
	obs-ndi.h)

add_library(obs-ndi MODULE 
	${obs-ndi_SOURCES}
	${obs-ndi_HEADERS})
include_directories( 
	"${NDISDK_DIR}/Include")
target_link_libraries(obs-ndi 
	libobs
	"${NDISDK_DIR}/Lib/x86/Processing.NDI.Lib.x86.lib")