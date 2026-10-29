# Chord Machine: plays the Syntakt SY CHORD machine with scale-aware chord pads
file(GLOB_RECURSE CHORDMACHINE_SOURCES "*.cpp" "*.c")
file(GLOB_RECURSE CHORDMACHINE_HEADERS "*.h")

add_library(ChordMachineAPP ${CHORDMACHINE_SOURCES} ${CHORDMACHINE_HEADERS})

target_include_directories(ChordMachineAPP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Note # Header-only reuse: Scales.h, ScaleVisualizer.h
)

target_link_libraries(ChordMachineAPP PUBLIC
    MatrixOSInterface
    MatrixOSAPPInterface
)

RegisterApplicationClass(ChordMachine)
