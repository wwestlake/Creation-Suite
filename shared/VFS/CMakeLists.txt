# FatFs (third_party/fatfs) is vendored, unmodified except for ffconf.h's
# configuration switches -- see third_party/fatfs/LICENSE.txt (BSD-1-clause
# style, MIT-compatible). ff.c/ffsystem.c/ffunicode.c are C files; our own
# disk I/O shim and the app-facing SuiteVolume wrapper are C++.
add_library(creation_suite_vfs STATIC
    third_party/fatfs/ff.c
    third_party/fatfs/ffsystem.c
    third_party/fatfs/ffunicode.c
    src/FatFsDiskIo.cpp
    src/SparseFile.cpp
    src/SuiteVolume.cpp
)

target_include_directories(creation_suite_vfs
    PUBLIC
        include
    PRIVATE
        third_party/fatfs
        src
)

target_link_libraries(creation_suite_vfs PUBLIC juce::juce_core)
target_compile_features(creation_suite_vfs PUBLIC cxx_std_20)

add_executable(creation_suite_vfs_smoke
    tests/VfsSmoke.cpp
)

target_link_libraries(creation_suite_vfs_smoke
    PRIVATE
        creation_suite_vfs
)

add_test(
    NAME creation_suite_vfs_smoke
    COMMAND creation_suite_vfs_smoke
)

add_executable(creation_suite_vfs_largefile_smoke
    tests/VfsLargeFileSmoke.cpp
)

target_link_libraries(creation_suite_vfs_largefile_smoke
    PRIVATE
        creation_suite_vfs
)

add_test(
    NAME creation_suite_vfs_largefile_smoke
    COMMAND creation_suite_vfs_largefile_smoke
)
