# SC1000 Controller Application
file(GLOB_RECURSE SC1000_SOURCES "*.cpp" "*.c")
file(GLOB_RECURSE SC1000_HEADERS "*.h")

add_library(SC1000APP ${SC1000_SOURCES} ${SC1000_HEADERS})

target_include_directories(SC1000APP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SC1000APP PUBLIC
    MatrixOSInterface
    MatrixOSAPPInterface
)

# Register the application class
RegisterApplicationClass(SC1000)
