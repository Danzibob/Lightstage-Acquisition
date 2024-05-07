cmake_minimum_required(VERSION 3.10)
project(DomeApp)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Default ArenaSDK path based on the OS
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ARENA_SDK_DEFAULT_PATH "C:/Program Files/LUCID Vision Labs/Arena SDK")
    set(ARENA_SDK_PATH ${ARENA_SDK_DEFAULT_PATH} CACHE PATH "Path to Arena SDK")
    set(GENICAM_LIB_PATH "${ARENA_SDK_PATH}/GenICam/library/CPP/lib/Win64_x64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ARENA_SDK_DEFAULT_PATH "/opt/ArenaSDK/ArenaSDK_Linux_x64")
    set(ARENA_SDK_PATH ${ARENA_SDK_DEFAULT_PATH} CACHE PATH "Path to Arena SDK")
    set(GENICAM_LIB_PATH "${ARENA_SDK_PATH}/GenICam/library/lib/Linux64_x64")
else()
    message(FATAL_ERROR "Unsupported OS")
endif()

# Genicam has a cursed af file structure
set(GENICAM_INCLUDE_PATH "${ARENA_SDK_PATH}/GenICam/library/CPP/include")

# Add the include directories
include_directories(${ARENA_SDK_PATH}/include/Arena)
include_directories(${ARENA_SDK_PATH}/include/Save)
include_directories(${ARENA_SDK_PATH}/include/SaveC)
include_directories(${GENICAM_INCLUDE_PATH})

# Specify the link directories
link_directories(${ARENA_SDK_PATH}/lib64)
link_directories(${GENICAM_LIB_PATH})

# Set RPATH (linux only)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Use the full RPATH during building
    set(CMAKE_BUILD_WITH_INSTALL_RPATH True)

    # Set the RPATH to the installed library path
    set(CMAKE_INSTALL_RPATH "${ARENA_SDK_PATH}/lib64;${GENICAM_LIB_PATH}")
endif()

# Define the executable and its source files
add_executable(DomeApp main.cpp)

# Link the libraries
target_link_libraries(DomeApp
    arena
    save
    savec
    lucidlog
    gentl
    GCBase_gcc54_v3_3_LUCID
    GenApi_gcc54_v3_3_LUCID
    avformat
    avcodec
    avutil
)
