include(FindPackageHandleStandardArgs)

if(DEFINED ENV{HIGHWAYHASH_DIR})
    set(HIGHWAYHASH_DIR ENV{HIGHWAYHASH_DIR})
endif()

find_path(
    HIGHWAYHASH_INCLUDE_DIR highwayhash/highwayhash_target.h
    HINTS ${HIGHWAYHASH_DIR})

if(DEFINED ENV{HIGHWAYHASH_DIR)
    set(HIGHWAYHASH_DIR ENV{HIGHWAYHASH_DIR})
endif()

find_library(
    HIGHWAYHASH_LIBRARY NAMES highwayhash
    HINTS ${HIGHWAYHASH_DIR})

find_package_handle_standard_args(
    HighwayHash
    REQUIRED_VARS HIGHWAYHASH_INCLUDE_DIR HIGHWAYHASH_LIBRARY)
