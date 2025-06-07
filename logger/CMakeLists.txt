
add_library(LoggerLib 
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Logger.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/AsyncLogger.cpp    
)

target_include_directories(LoggerLib 
    PUBLIC .)

target_include_directories(LoggerLib 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/timer
)

target_link_libraries(LoggerLib PRIVATE TimerLib)