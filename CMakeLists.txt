cmake_minimum_required(VERSION 3.20)

project(ZoneJobScheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 스케줄러 코어: 테스트·측정·(향후) 서버 실행 파일이 함께 링크한다.
add_library(ZoneJobSchedulerCore STATIC
	src/Actor.cpp
	src/JobQueue.cpp
	src/Producer.cpp
	src/ReadyQueue.cpp
	src/ThreadPool.cpp
	src/Worker.cpp
	src/Zone.cpp
	src/ZoneScheduler.cpp
	include/Actor.h
	include/Job.h
	include/JobQueue.h
	include/Producer.h
	include/ReadyQueue.h
	include/ThreadPool.h
	include/Worker.h
	include/Zone.h
	include/ZoneScheduler.h
)

target_include_directories(ZoneJobSchedulerCore PUBLIC include)

# 정확성 테스트 실행 파일
add_executable(ZoneJobScheduler
	src/main.cpp
	tests/SchedulerTest.cpp
	tests/SchedulerTest.h
)

target_include_directories(ZoneJobScheduler PRIVATE tests)
target_link_libraries(ZoneJobScheduler PRIVATE ZoneJobSchedulerCore)

# 성능 측정 실행 파일 (Release 빌드로 실행)
add_executable(SchedulerBenchmark
	bench/SchedulerBenchmark.cpp
)

target_link_libraries(SchedulerBenchmark PRIVATE ZoneJobSchedulerCore)

foreach(target IN ITEMS ZoneJobSchedulerCore ZoneJobScheduler SchedulerBenchmark)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endforeach()

enable_testing()

add_test(
    NAME ZoneJobSchedulerCorrectness
    COMMAND ZoneJobScheduler
)
