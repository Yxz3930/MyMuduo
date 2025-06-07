# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_BUILD_TYPE Debug)

# # 编译选项
# add_compile_options(-Wall -O2)
# # add_compile_options(-Wall)

# set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/output)

# cmake_minimum_required(VERSION 3.10)

# project(MemoryPool)

# include_directories(${CMAKE_SOURCE_DIR}/include)

# file(GLOB src_files ${CMAKE_SOURCE_DIR}/src/*.cpp)

# add_executable(memoryPool_test
#     ${CMAKE_SOURCE_DIR}/test/memoryPool_test.cpp
#     ${src_files}
# )


# ##################################################
# 将这个文件夹中的文件制作称库
# ##################################################
file(GLOB src_files src/*.cpp)
add_library(MemoryPoolLib
    STATIC
    ${src_files}
)
target_include_directories(MemoryPoolLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


# ##################################################
# 这个文件夹中的文件进行编译输出 得到可执行文件
# ##################################################

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/output)

add_compile_options(-Wall -O2)
add_executable(SmartPointer test/SmartPointer.cpp ${src_files})
target_include_directories(SmartPointer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(memoryPool_test test/memoryPool_test.cpp ${src_files})
target_include_directories(memoryPool_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)