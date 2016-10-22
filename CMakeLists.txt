cmake_minimum_required(VERSION 3.5)
project(obs-ndi)

include(external/FindLibObs.cmake)
find_package(LibObs REQUIRED)

set(obs-ndi_SOURCES 
	obs-ndi.cpp
	obs-ndi-source.cpp
	obs-ndi-output.cpp)

set(obs-obs
	ndi_HEADERS-ndi.h)

add_library(obs-ndi MODULE 
	${obs-ndi_SOURCES}
	${obs-ndi_HEADERS})
include_directories( 
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	"${NDISDK_DIR}/Include")
target_link_libraries(obs-ndi 
	libobs
	"${PTHREADS_LIB}"
	"${OBS_FRONTEND_LIB}"
	"${NDISDK_DIR}/Lib/x86/Processing.NDI.Lib.x86.lib")