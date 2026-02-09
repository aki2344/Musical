# toolchain-aarch64.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# multiarch の探索先を優先
set(CMAKE_FIND_ROOT_PATH
    ../
    /usr/aarch64-linux-gnu
    /usr/lib/aarch64-linux-gnu
    /usr/include/aarch64-linux-gnu
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 共有ライブラリ作るならPIC推奨
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

