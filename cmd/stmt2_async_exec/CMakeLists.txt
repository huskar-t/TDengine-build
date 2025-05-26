add_executable(stmt2_async_exec_test main.c)
target_link_libraries(stmt2_async_exec_test PRIVATE tools taos)
target_include_directories(stmt2_async_exec_test PRIVATE ../../tools/include)