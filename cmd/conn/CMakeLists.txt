add_executable(conn_test main.c)
target_link_libraries(conn_test PRIVATE tools taosnative)
target_include_directories(conn_test PRIVATE ../../tools/include)