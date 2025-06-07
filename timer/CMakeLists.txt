
add_library(TimerLib 
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/TimeStamp.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/Timer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TimeQueue.cpp
)

target_include_directories(TimerLib PUBLIC .)

target_include_directories(TimerLib PUBLIC 
    ${CMAKE_SOURCE_DIR}/net/include    
    ${CMAKE_SOURCE_DIR}/logger
)

target_link_libraries(TimerLib PRIVATE NetLib LoggerLib)