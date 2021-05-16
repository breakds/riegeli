add_library(message_serialize SHARED)
target_sources(message_serialize
    PUBLIC
    $<INSTALL_INTERFACE:message_serialize.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/message_serialize.h>
    PRIVATE message_serialize.cc)
target_link_libraries(message_serialize
    PUBLIC base chain writer chain_writer cord_writer string_writer
    PRIVATE absl::core_headers absl::status absl::strings absl::cord protobuf::libprotobuf_lite)
