add_executable(stmt2_query_test main.c)
target_link_libraries(stmt2_query_test PRIVATE tools taos)
target_include_directories(stmt2_query_test PRIVATE ../../tools/include)