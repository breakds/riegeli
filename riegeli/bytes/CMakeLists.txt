add_library(writer STATIC)
target_sources(writer
    PUBLIC
    $<INSTALL_INTERFACE:writer.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/writer.h>
    PRIVATE writer.cc)
target_link_libraries(writer
    PUBLIC base chain status
    PRIVATE absl::status absl::base absl::strings)


add_library(chain_writer STATIC)
target_sources(chain_writer
    PUBLIC
    $<INSTALL_INTERFACE:chain_writer.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/chain_writer.h>
    PRIVATE chain_writer.cc)
target_link_libraries(chain_writer
    PUBLIC base chain writer
    PRIVATE absl::base absl::strings absl::cord absl::span)


add_library(cord_writer STATIC)
target_sources(cord_writer
    PUBLIC
    $<INSTALL_INTERFACE:cord_writer.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/cord_writer.h>
    PRIVATE cord_writer.cc)
target_link_libraries(cord_writer
    PUBLIC writer base buffer chain
    PRIVATE absl::core_headers absl::strings absl::cord basl::optional)


add_library(string_writer STATIC)
target_sources(string_writer
    PUBLIC
    $<INSTALL_INTERFACE:string_writer.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/string_writer.h>
    PRIVATE string_writer.cc)
target_link_libraries(string_writer
    PUBLIC writer base chain
    PRIVATE absl::core_headers absl::strings absl::cord basl::optional)
