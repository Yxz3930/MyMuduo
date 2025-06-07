# 创建 CommonLib 静态库
add_library(CommonLib STATIC ${CMAKE_CURRENT_SOURCE_DIR}/myutils.cpp)

# 指定 CommonLib 头文件目录
target_include_directories(CommonLib PUBLIC .)
target_include_directories(CommonLib 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/logger
)

target_link_libraries(CommonLib PRIVATE LoggerLib)
