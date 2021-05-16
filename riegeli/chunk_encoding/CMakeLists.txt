add_library(hash SHARED)
target_sources(hash
    PUBLIC
    $<INSTALL_INTERFACE:hash.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/hash.h>
    PRIVATE hash.cc)
target_link_libraries(hash
    PUBLIC chain
    PRIVATE absl::inlined_vector absl::strings absl::optional -lhighwayhash)


add_library(constants INTERFACE)
target_sources(constants
    INTERFACE constants.h)
target_link_libraries(hash INTERFACE base)
    

add_library(chunk SHARED)
target_sources(chunk
    PUBLIC
    $<INSTALL_INTERFACE:chunk.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/chunk.h>
    PRIVATE chunk.cc)
