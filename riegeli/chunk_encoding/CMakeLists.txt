add_library(hash SHARED)
target_sources(hash
    PUBLIC
    $<INSTALL_INTERFACE:hash.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/hash.h>
    PRIVATE hash.cc)
target_link_libraries(hash
    PUBLIC chain
    PRIVATE absl::inlined_vector absl::strings absl::optional
    # highwayhash)
