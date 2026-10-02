cmake_minimum_required(VERSION 3.16)
project(chaos_emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -O2)

add_executable(chaos_emulator
    src/main.cpp
    src/net/TunInterface.cpp
    src/net/PacketParser.cpp
    src/chaos/LossModel.cpp
    src/chaos/DelayJitterModel.cpp
    src/chaos/TokenBucket.cpp
    src/scheduler/DelayQueue.cpp
    src/metrics/MetricsCollector.cpp
    src/metrics/SystemMonitor.cpp
    src/report/ReportGenerator.cpp
    src/engine/ChaosEngine.cpp
)

target_include_directories(chaos_emulator PRIVATE src)
target_link_libraries(chaos_emulator PRIVATE pthread)

# Optional: unit tests (requires GoogleTest/Catch2, added in a later milestone)
# enable_testing()
# add_subdirectory(tests)
