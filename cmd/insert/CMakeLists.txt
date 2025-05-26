add_executable(insert_test main.c)
target_link_libraries(insert_test PRIVATE tools taosnative)
target_include_directories(insert_test PRIVATE ../../tools/include)