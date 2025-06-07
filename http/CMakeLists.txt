
add_library(HttpLib 
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpParser.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpResponse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpClient.cpp
)


target_include_directories(HttpLib PUBLIC .)

target_include_directories(HttpLib
    PUBLIC 
    ${CMAKE_SOURCE_DIR}/net/include
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/timer
    ${CMAKE_SOURCE_DIR}/logger
)

# 如果 HttpLib 需要依赖 NetLib 和 CommonLib
target_link_libraries(HttpLib PRIVATE NetLib CommonLib TimerLib LoggerLib)

