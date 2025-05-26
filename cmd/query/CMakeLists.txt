add_executable(query_test main.c)
target_link_libraries(query_test PRIVATE tools taos)
target_include_directories(query_test PRIVATE ../../tools/include)