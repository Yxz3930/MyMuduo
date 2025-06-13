file(GLOB source_file *.cpp)

add_library(HttpLib 
    STATIC
    ${source_file}
)


target_include_directories(HttpLib PUBLIC .)
target_link_libraries(HttpLib PRIVATE NetLib)



