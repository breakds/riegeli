add_library(status STATIC)
target_sources(status
    PUBLIC errno_mapping.h status.h
    PRIVATE errno_mapping.cc status.cc)
target_link_libraries(status
    PRIVATE absl::status absl::strings)
