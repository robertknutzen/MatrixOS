# Breakbeat: grid controller for the slompler breakbeat track
file(GLOB_RECURSE BREAKBEAT_SOURCES "*.cpp" "*.c")
file(GLOB_RECURSE BREAKBEAT_HEADERS "*.h")

add_library(BreakbeatAPP ${BREAKBEAT_SOURCES} ${BREAKBEAT_HEADERS})

target_include_directories(BreakbeatAPP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(BreakbeatAPP PUBLIC
    MatrixOSInterface
    MatrixOSAPPInterface
)

# Register the application class
RegisterApplicationClass(Breakbeat)
