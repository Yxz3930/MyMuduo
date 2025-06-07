
add_library(ChatRoomLib 
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatClient.cpp
)


target_include_directories(ChatRoomLib PUBLIC .)

target_include_directories(ChatRoomLib
    PUBLIC 
    ${CMAKE_SOURCE_DIR}/net/include
    ${CMAKE_SOURCE_DIR}/common
)

# 如果 ChatRoomLib 需要依赖其他库（比如 CommonLib）
target_link_libraries(ChatRoomLib PRIVATE NetLib CommonLib)
