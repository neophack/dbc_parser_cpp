add_executable(dbc_example parse_dbc.cpp)
target_link_libraries(dbc_example PRIVATE dbc)
target_compile_features(dbc_example PRIVATE cxx_std_11)
