cmake_minimum_required(VERSION 3.14)

project(HighPerformanceServer)


# .................google test dep....................
# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

#enable_testing()
#.....................................................
add_library(webserver webserver/webserver.cpp)

add_executable(
    ${PROJECT_NAME}
    main.cpp
)

#链接库
#target_link_directories(${PROJECT_NAME} PRIVATE queue)
#target_link_libraries(${PROJECT_NAME} GTest::gtest_main queue)
#target_include_directories(${PROJECT_NAME} PUBLIC queue)

target_link_directories(${PROJECT_NAME} PRIVATE webserver)
target_link_libraries(${PROJECT_NAME} webserver)
target_include_directories(${PROJECT_NAME} PUBLIC webserver)

target_link_libraries(
  ${PROJECT_NAME}
  webserver
 # GTest::gtest_main
)

#include(GoogleTest)

#gtest_discover_tests(${PROJECT_NAME})