# CMake References: https://github.com/Lectem/cpp-boilerplate/blob/master/CMakeLists.txt

cmake_minimum_required(VERSION 3.17)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

project("riegeli"
    VERSION 1.0
    LANGUAGES C CXX)

############################
##  Modules and scripts   ##
############################

# Standard CMake modules

include(CTest)                          # Must be called before adding tests but after calling project(). This automatically calls enable_testing() and configures ctest targets when using Make/Ninja
include(CMakeDependentOption)           # This is a really useful scripts that creates options that depends on other options. It can even be used with generator expressions !
include(GNUInstallDirs)                 # This will define the default values for installation directories (all platforms even if named GNU)
include(InstallRequiredSystemLibraries) # Tell CMake that the `install` target needs to install required system libraries (eg: Windows SDK)
include(CMakePackageConfigHelpers)      # Helper to create relocatable packages

# Custom modules and scripts

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake") # Make our cmake scripts available

####################
##  Dependencies  ##
####################

find_package(absl REQUIRED)

###############
##  Options  ##
###############

option(${PROJECT_NAME}_INSTALL
    "Should ${PROJECT_NAME} be added to the install list? Useful if included using add_subdirectory." ON)

###############
##  Targets  ##
###############

# Use the project root to find includes
include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(riegeli)
