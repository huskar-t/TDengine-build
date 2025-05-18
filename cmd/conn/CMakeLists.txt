add_executable(test_conn main.c)
target_link_libraries(test_conn PRIVATE tools taos)
target_include_directories(test_conn PRIVATE ../../tools/include)