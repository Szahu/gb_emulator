cmake_minimum_required(VERSION 3.22)

project(gb_emulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)

# Add source files
file(GLOB_RECURSE SRC_FILES 
	${CMAKE_SOURCE_DIR}/src/*.c
	${CMAKE_SOURCE_DIR}/src/*.cpp)
	
# Add header files
file(GLOB_RECURSE HDR_FILES 
	${CMAKE_SOURCE_DIR}/src/*.h
	${CMAKE_SOURCE_DIR}/src/*.hpp)

# SDL
set(SDL_TEST OFF)
add_subdirectory(${VENDOR_DIR}/SDL)

# Define the executable
add_executable(${PROJECT_NAME} ${SRC_FILES} ${HDR_FILES})
include_directories(${VENDOR_DIR}/SDL/include)

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_DEFINITIONS "PROJECT_DIR=\"${CMAKE_SOURCE_DIR}\"")
target_link_libraries(${PROJECT_NAME} SDL3::SDL3)