cmake_minimum_required(VERSION 3.8)
project(fpga_config)

#################################
#  Build configuration options  #
#################################

# These can be overriden by ccmake, commandline args or using
# set(BUILD_OPTION <new value> CACHE <type> "") in an including project

option(NINA_PI_HAT "Build to use with the Melbourne Instruments NINA Rpi hat" TRUE)
option(DELIA_PI_HAT "Build to use with the Melbourne Instruments DELIA Rpi hat" FALSE)

##################################
#  Perform Cross Compile setup   #
##################################

# Check if a cross-compile build
message("CMAKE_SYSROOT is " ${CMAKE_SYSROOT})
if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
    set(CMAKE_SYSROOT "${CMAKE_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif()

##################################
#  Generate build information    #
##################################

# Get the latest commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M")

####################
#  Main Target     #
####################

set(COMPILATION_UNITS src/main.cpp)

# Enumerate all the headers separately so that CLion can index them
set(EXTRA_CLION_SOURCES )

set(SOURCE_FILES "${COMPILATION_UNITS}" "${EXTRA_CLION_SOURCES}")

add_executable(fpga_config "${COMPILATION_UNITS}")
target_link_libraries(fpga_config PRIVATE pthread)

#########################
#  Include Directories  #
#########################

set(INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/src"
                 "${PROJECT_SOURCE_DIR}/include")

# /usr/local doesn't get added by default in Mac OS X
if (APPLE)
    set(INCLUDE_DIRS "${INCLUDE_DIRS}" /usr/local/include)
endif()

#################################
#  Linked libraries             #
#################################

# Common libraries between all builds, and safe to be included in test targets
set(COMMON_LIBRARIES pthread)

#set(EXTRA_BUILD_LIBRARIES ${EXTRA_BUILD_LIBRARIES} grpc++ asound lo)
target_include_directories(fpga_config PRIVATE ${INCLUDE_DIRS})
target_link_libraries(fpga_config PRIVATE ${EXTRA_BUILD_LIBRARIES} ${COMMON_LIBRARIES})

####################################
#  Compiler Flags and definitions  #
####################################

target_compile_features(fpga_config PRIVATE cxx_std_17)
target_compile_options(fpga_config PRIVATE -Wall -Wextra -Wno-psabi -fno-rtti -ffast-math)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0"))
        target_compile_options(fpga_config PRIVATE -faligned-new)
    endif()
endif()
if (${NINA_PI_HAT})
    target_compile_options(fpga_config PRIVATE -DMELBINST_PI_HAT=0)
endif()
if (${DELIA_PI_HAT})
    target_compile_options(fpga_config PRIVATE -DMELBINST_PI_HAT=1)
endif()

####################
#  Install         #
####################

install(TARGETS fpga_config DESTINATION bin)
