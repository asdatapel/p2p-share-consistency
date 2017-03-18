cmake_minimum_required(VERSION 3.6)
project(p2p_share)

set(CMAKE_CXX_STANDARD 11)


include_directories(ext/SFML/include)

link_directories(ext/SFML/lib)

set(SOURCE_FILES main.cpp Client.cpp File.cpp)

add_executable(p2p_Client ${SOURCE_FILES})
target_link_libraries(p2p_Client sfml-system.a)
target_link_libraries(p2p_Client sfml-network.a)
target_compile_definitions(p2p_Client PRIVATE)
