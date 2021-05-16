add_library(status STATIC)
target_sources(status
    PUBLIC
    $<INSTALL_INTERFACE:errno_mapping.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/errno_mapping.h>
    $<INSTALL_INTERFACE:status.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/status.h>
    PRIVATE errno_mapping.cc status.cc)
target_link_libraries(status
    PRIVATE absl::status absl::strings)
