set(source_files test.cc)
add_executable(lsm_test ${source_files})
target_link_libraries(lsm_test PUBLIC lsm)
target_include_directories(lsm_test PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET lsm_test PROPERTY CXX_STANDARD 11)

if(MSVC)
    target_compile_definitions(lsm_test PRIVATE TYPE_SAFE_TEST_NO_STATIC_ASSERT)
elseif(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
    target_compile_definitions(lsm_test PRIVATE TYPE_SAFE_TEST_NO_STATIC_ASSERT)
endif()

add_test(NAME test COMMAND lsm_test)