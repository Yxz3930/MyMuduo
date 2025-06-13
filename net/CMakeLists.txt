file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(NetLib STATIC ${SOURCES})



# 指定包含目录（正确的应该使用 target_include_directories）
# 包含当前模块的头文件路径 这个目的是为了正确添加头文件 通过 #include "xxx.h" 不需要带路径名直接就是各头文件的名称
target_include_directories(NetLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(NetLib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/http
    ${CMAKE_SOURCE_DIR}/timer
    ${CMAKE_SOURCE_DIR}/logger
    ${CMAKE_SOURCE_DIR}/memoryPool/include
)


# 如果 NetLib 需要依赖其他库（比如 HttpLib 或 CommonLib）
target_link_libraries(NetLib PRIVATE CommonLib TimerLib LoggerLib MemoryPoolLib)
