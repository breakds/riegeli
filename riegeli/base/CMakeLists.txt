add_library(status STATIC)
target_sources(status
    PUBLIC
    $<INSTALL_INTERFACE:errno_mapping.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/errno_mapping.h>
    $<INSTALL_INTERFACE:status.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/status.h>
    PRIVATE errno_mapping.cc status.cc)
target_link_libraries(status
    PRIVATE absl::status absl::strings absl::cord)


add_library(base STATIC)
target_sources(base
    PUBLIC
    $<INSTALL_INTERFACE:base.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/base.h>

    $<INSTALL_INTERFACE:dependency.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/dependency.h>

    $<INSTALL_INTERFACE:memory.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/memory.h>

    $<INSTALL_INTERFACE:object.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/object.h>

    $<INSTALL_INTERFACE:port.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/port.h>

    $<INSTALL_INTERFACE:reset.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/reset.h>

    $<INSTALL_INTERFACE:stable_dependency.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/stable_dependency.h>

    PRIVATE base.cc memory.cc object.cc)
target_link_libraries(base
    PRIVATE absl::core_headers absl::base
    absl::status absl::strings absl::type_traits absl::cord)


add_library(intrusive_ref_count INTERFACE)
target_sources(intrusive_ref_count INTERFACE intrusive_ref_count.h)
    

add_library(memory_estimator STATIC)
target_sources(memory_estimator
    PUBLIC
    $<INSTALL_INTERFACE:memory_estimator.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/memory_estimator.h>
    PRIVATE memory_estimator.cc)
target_link_libraries(memory_estimator
    PRIVATE absl::container)


add_library(chain STATIC)
target_sources(chain
    PUBLIC
    $<INSTALL_INTERFACE:chain.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/chain.h>
    PRIVATE memory_estimator.cc)
target_link_libraries(chain
    PUBLIC base intrusive_ref_count memory_estimator
    PRIVATE absl::base absl::container absl::meta absl::strings)


add_library(options_parser STATIC)
target_sources(options_parser
    PUBLIC
    $<INSTALL_INTERFACE:options_parser.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/options_parser.h>
    PRIVATE options_parser.cc)
target_link_libraries(options_parser
    PUBLIC base 
    PRIVATE absl::base absl::status absl::strings)


add_library(parallelism STATIC)
target_sources(parallelism
    PUBLIC
    $<INSTALL_INTERFACE:parallelism.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/parallelism.h>
    PRIVATE parallelism.cc)
target_link_libraries(parallelism
    PUBLIC base 
    PRIVATE absl::base absl::time absl::synchronization)


add_library(buffer STATIC)
target_sources(buffer
    PUBLIC
    $<INSTALL_INTERFACE:buffer.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/buffer.h>
    PRIVATE buffer.cc)
target_link_libraries(parallelism
    PUBLIC base 
    PRIVATE absl::strings absl::cord)
