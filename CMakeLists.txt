cmake_minimum_required(VERSION 3.20)

project(ZoneJobScheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ZoneJobScheduler
	src/main.cpp
	tests/SchedulerTest.cpp
	tests/SchedulerTest.h
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

target_include_directories(ZoneJobScheduler PRIVATE include tests)

if(MSVC)
    target_compile_options(ZoneJobScheduler PRIVATE
        /W4
        /permissive-
    )
else()
    target_compile_options(ZoneJobScheduler PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

enable_testing()

add_test(
    NAME ZoneJobSchedulerCorrectness
    COMMAND ZoneJobScheduler
)
