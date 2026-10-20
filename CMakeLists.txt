cmake_minimum_required(VERSION 3.20)

project(ZoneJobScheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZJS_ENABLE_ASAN "Build with AddressSanitizer" OFF)

if(ZJS_ENABLE_ASAN AND MSVC)
	add_compile_options(/fsanitize=address)
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	add_link_options(/INCREMENTAL:NO)
endif()

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

# Hot Zone 지연 측정 실행 파일 (Release 빌드로 실행)
add_executable(HotZoneBenchmark
	bench/HotZoneBenchmark.cpp
)

target_link_libraries(HotZoneBenchmark PRIVATE ZoneJobSchedulerCore)

# 세션 수명 재현 실행 파일 (ASan 빌드로 실행)
add_executable(SessionLifetimeRepro
	tests/SessionLifetimeRepro.cpp
)

target_link_libraries(SessionLifetimeRepro PRIVATE ZoneJobSchedulerCore)

# 세션 수명 해결 확인 실행 파일 (CTest 등록, ASan 빌드에서도 실행)
add_executable(SessionLifetimeTest
	tests/SessionLifetimeTest.cpp
)

target_link_libraries(SessionLifetimeTest PRIVATE ZoneJobSchedulerCore)

# IOCP 네트워크 계층: 스케줄러 코어와 별개 라이브러리 (Zone Job 연결은 과제 4)
add_library(ServerNetwork STATIC
	src/IocpServer.cpp
	src/PacketAssembler.cpp
	src/Session.cpp
	include/IocpServer.h
	include/PacketAssembler.h
	include/Session.h
)

target_include_directories(ServerNetwork PUBLIC include)
target_compile_definitions(ServerNetwork PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ServerNetwork PUBLIC ws2_32)

# IOCP 서버 확인 실행 파일 (CTest 등록)
add_executable(IocpServerTest
	tests/IocpServerTest.cpp
)

target_link_libraries(IocpServerTest PRIVATE ServerNetwork)

# 연결된 채 서버 종료 시 세션 정리 확인 실행 파일 (CTest 등록, ASan 빌드에서도 실행)
add_executable(IocpShutdownTest
	tests/IocpShutdownTest.cpp
)

target_link_libraries(IocpShutdownTest PRIVATE ServerNetwork)

# 패킷 조립 확인 실행 파일 (CTest 등록, 네트워크 없이 실행)
add_executable(PacketAssemblerTest
	tests/PacketAssemblerTest.cpp
)

target_link_libraries(PacketAssemblerTest PRIVATE ServerNetwork)

# 잘못된 패킷을 보낸 세션을 끊는지 확인 실행 파일 (CTest 등록)
add_executable(IocpInvalidPacketTest
	tests/IocpInvalidPacketTest.cpp
)

target_link_libraries(IocpInvalidPacketTest PRIVATE ServerNetwork)

# 패킷 여러 개를 한 번에 보내 send 큐를 확인하는 실행 파일 (CTest 등록)
add_executable(IocpBurstTest
	tests/IocpBurstTest.cpp
)

target_link_libraries(IocpBurstTest PRIVATE ServerNetwork)

foreach(target IN ITEMS ZoneJobSchedulerCore ZoneJobScheduler SchedulerBenchmark HotZoneBenchmark SessionLifetimeRepro 
SessionLifetimeTest ServerNetwork IocpServerTest IocpShutdownTest PacketAssemblerTest IocpInvalidPacketTest
IocpBurstTest)

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

add_test(
    NAME SessionLifetime
    COMMAND SessionLifetimeTest
)

add_test(
    NAME IocpServer
    COMMAND IocpServerTest
)

add_test(
    NAME IocpShutdown
    COMMAND IocpShutdownTest
)

add_test(
    NAME PacketAssembler
    COMMAND PacketAssemblerTest
)

add_test(
    NAME IocpInvalidPacket
    COMMAND IocpInvalidPacketTest
)

add_test(
    NAME IocpBurst
    COMMAND IocpBurstTest
)