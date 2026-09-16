cmake_minimum_required(VERSION 3.20)

project(ZoneJobScheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ZoneJobScheduler
	main.cpp
	Actor.cpp
	JobQueue.cpp
	ReadyQueue.cpp
	Zone.cpp
	ZoneScheduler.cpp
	Worker.cpp
	ThreadPool.cpp
	Producer.cpp
)
