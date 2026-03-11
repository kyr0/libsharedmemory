add_executable(example_stream stream.cpp)
target_link_libraries(example_stream PRIVATE lsm)
set_property(TARGET example_stream PROPERTY CXX_STANDARD 20)

add_executable(example_queue queue.cpp)
target_link_libraries(example_queue PRIVATE lsm)
set_property(TARGET example_queue PROPERTY CXX_STANDARD 20)

# C example: the wrapper (lsm_c.cpp) is compiled as C++ and linked with the
# pure-C main (raw.c). CMake handles mixed C/C++ linking automatically.
add_library(lsm_c_wrapper STATIC lsm_c.cpp)
target_link_libraries(lsm_c_wrapper PRIVATE lsm)
set_property(TARGET lsm_c_wrapper PROPERTY CXX_STANDARD 20)
target_include_directories(lsm_c_wrapper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(example_raw raw.c)
target_link_libraries(example_raw PRIVATE lsm_c_wrapper)
