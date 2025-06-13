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