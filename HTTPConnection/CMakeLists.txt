project(HTTPConnection)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../utility)

add_library(${PROJECT_NAME} HTTPConnection.cpp)

target_link_libraries(${PROJECT_NAME} utility)