aux_source_directory(src TOOLS_SRC)
add_library(tools STATIC ${TOOLS_SRC})
target_include_directories(tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tools PUBLIC pthread)