cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_PROJECT_NAME all_examples)
project(${CMAKE_PROJECT_NAME} C ASM CXX)

add_subdirectory(erpc)
add_subdirectory(ext/json)

############################################################
# server
# add_executable(server_ex)

# target_sources(server_ex
#     PRIVATE
#         server.cpp
#         generated/example_interface.cpp
#         generated/example_server.cpp
# )

# target_include_directories(server_ex
#     PRIVATE
#         generated
# )

# target_link_libraries(server_ex
#     PRIVATE
#     erpc
# )

############################################################
# client
add_executable(client_ex)

target_sources(client_ex
    PRIVATE
        client.cpp
        generated/c_example_client.cpp
        generated/example_interface.cpp
        generated/example_client.cpp
)

target_include_directories(client_ex
    PRIVATE
        generated
)

target_link_libraries(client_ex
    PRIVATE
    erpc
    nlohmann_json::nlohmann_json
) 
