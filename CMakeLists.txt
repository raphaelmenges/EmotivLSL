cmake_minimum_required(VERSION 2.8)

project(EmotivLSL)
set(APPNAME EmotivLSL)

# Include directory
include_directories(.)

# Collect files
file(GLOB SOURCES
	"*.cpp"
	"*.c")

file(GLOB HEADERS
	"*.h")

# Collect all code
set(ALL_CODE
	${SOURCES}
	${HEADERS})

# LabStreamingLayer
set(LIBLSL_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/liblsl")
include_directories("${LIBLSL_DIRECTORY}/include")
set(LIBLSL_LIBRARIES "${LIBLSL_DIRECTORY}/lib-vs2015_x86_release/liblsl32.lib")

# Emotiv SDK
set(EMOTIV_SDK_PATH "C:/Program Files (x86)/Emotiv SDK Premium Edition v3.3.3/EDK" CACHE PATH "Path to Emotiv SDK Premium Edition.")
include_directories("${EMOTIV_SDK_PATH}/Header files")
set(EMOTIV_SDK_LIBRARIES "${EMOTIV_SDK_PATH}/x86/edk.lib")

# Creation of executeable
add_executable(${APPNAME} ${HEADERS} ${SOURCES})

# Linking of libraries
target_link_libraries(
	${APPNAME}
	${LIBLSL_LIBRARIES}
	${EMOTIV_SDK_LIBRARIES})

# Copy DLL for Emotiv to execution folder
add_custom_command(TARGET ${APPNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EMOTIV_SDK_PATH}/x86/edk.dll"
		${CMAKE_CURRENT_BINARY_DIR})

# Copy LabStreamingLayer library to output folder
add_custom_command(TARGET ${APPNAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${LIBLSL_DIRECTORY}/lib-vs2015_x86_release/liblsl32.dll"
		${CMAKE_CURRENT_BINARY_DIR})