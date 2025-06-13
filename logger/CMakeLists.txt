file(GLOB SOURCES *.cpp)

add_library(LoggerLib 
    STATIC
    ${SOURCES}  
)

target_include_directories(LoggerLib 
    PUBLIC .)

target_include_directories(LoggerLib 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/timer
)

find_package(fmt REQUIRED)
target_link_libraries(LoggerLib PRIVATE TimerLib fmt::fmt)