add_executable(stmt2_bind_test main.c)
target_link_libraries(stmt2_bind_test PRIVATE tools taosnative)
target_include_directories(stmt2_bind_test PRIVATE ../../tools/include)