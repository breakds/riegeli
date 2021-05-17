add_executable(hash_demo)
target_sources(hash_demo PRIVATE hash_demo.cc)
target_link_libraries(hash_demo PRIVATE hash)
