cmake_minimum_required(VERSION 3.16)
project(tbshaper VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TBSHAPER_WERROR "Treat warnings as errors" ON)
set(TBSHAPER_SANITIZE "" CACHE STRING "Sanitizer to enable: thread | address | undefined | (empty)")

add_compile_options(-Wall -Wextra -Wpedantic)
if(TBSHAPER_WERROR)
    add_compile_options(-Werror)
endif()
if(TBSHAPER_SANITIZE)
    add_compile_options(-fsanitize=${TBSHAPER_SANITIZE} -g -O1 -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${TBSHAPER_SANITIZE})
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NFQ REQUIRED IMPORTED_TARGET libnetfilter_queue libnfnetlink)

# Everything that does not need a live NFQUEUE socket; shared with the tests.
add_library(tbcore STATIC
    src/token_bucket.cpp
    src/packet.cpp
    src/policer.cpp
    src/shaper.cpp
    src/statistics.cpp
    src/config.cpp
)
target_include_directories(tbcore PUBLIC include)
target_link_libraries(tbcore PUBLIC Threads::Threads PkgConfig::NFQ)

add_executable(tbshaper src/main.cpp src/nfqueue_capture.cpp)
target_link_libraries(tbshaper PRIVATE tbcore)

enable_testing()
add_subdirectory(tests)
