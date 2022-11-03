
project(WebServer)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../utility)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../HTTPConnection)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ThreadPool)

add_library(${PROJECT_NAME} WebServer.cpp)

target_link_libraries(${PROJECT_NAME} utility)
target_link_libraries(${PROJECT_NAME} HTTPConnection)
target_link_libraries(${PROJECT_NAME} pthread)

