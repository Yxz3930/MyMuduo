cmake_minimum_required(VERSION 3.10)

# 设置库输出路径 在project之前设置
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/output)

# 在 CMakeLists.txt 中启用 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MyMuduo)

# 设置编译类型为 Debug
set(CMAKE_BUILD_TYPE Debug)

# 添加下一级的文件夹 注意是文件夹名称
add_subdirectory(${CMAKE_SOURCE_DIR}/common)
# add_subdirectory(${CMAKE_SOURCE_DIR}/http)
add_subdirectory(${CMAKE_SOURCE_DIR}/net)
add_subdirectory(${CMAKE_SOURCE_DIR}/timer)
add_subdirectory(${CMAKE_SOURCE_DIR}/logger)
add_subdirectory(${CMAKE_SOURCE_DIR}/memoryPool)


add_executable(EchoServer testServer.cpp)
target_link_libraries(EchoServer NetLib CommonLib LoggerLib TimerLib)

add_executable(EchoClient testClient.cpp)
target_link_libraries(EchoClient NetLib CommonLib LoggerLib TimerLib)

add_executable(EchoClientNonLoop testClientNonLoop.cpp)
target_link_libraries(EchoClientNonLoop NetLib CommonLib LoggerLib TimerLib)