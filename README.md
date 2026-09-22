# ZoneJobScheduler

같은 게임 상태를 다루는 Job의 동시 실행을 막기 위해 Zone을 작업 분배 단위로 만든 C++20 멀티스레드 Job Scheduler 데모입니다.

- **같은 Zone:** JobQueue에 등록된 순서대로, 한 번에 한 Worker가 직렬 실행
- **서로 다른 Zone:** 여러 Worker에서 병렬 실행 가능

```text
Producer ── submit(zoneId, Job) ──► ZoneScheduler
                                        │
                                        ▼
                                  Zone별 JobQueue
                                        │ CAS 성공 시 Zone 등록
                                        ▼
                                ReadyQueue (Zone 참조)
                                        │ 대기 Worker 깨우기
                                        ▼
                                 Worker Thread Pool
                                        │
                                        └──► 해당 Zone의 Job 순차 실행
```

## 프로젝트 목적

실무에서는 주로 단일 스레드로 실행되는 게임 로직을 다뤘습니다. 여러 Worker가 게임 상태를 나눠 처리할 때 필요한 작업 분배와 동시성 제어를 직접 구현하고 검증하기 위해 만든 개인 기술 데모입니다.

## 작업 처리 흐름

1. Producer가 `ZoneScheduler::submit()`으로 Job을 제출하면 해당 Zone의 JobQueue에 저장합니다. `m_isScheduled`를 `false`에서 `true`로 바꾼 호출만 Zone을 ReadyQueue에 등록합니다.
2. ReadyQueue에서 Zone을 받은 Worker가 JobQueue의 Job을 하나씩 꺼내 실행합니다. 이미 실행 중인 Zone에 들어온 Job도 같은 Worker가 이어서 처리할 수 있습니다.
3. 큐가 비면 Worker가 실행 상태를 해제하고 다시 확인합니다. 그 사이 들어온 Job이 남아 있으면 CAS에 성공한 쪽이 Zone을 다시 등록합니다.

## 핵심 설계

- **같은 Zone의 중복 실행 방지:** Job을 Worker에 직접 나누면 같은 상태를 두 Worker가 동시에 바꿀 수 있습니다. ReadyQueue에는 Job이 아닌 Zone을 넣고, CAS 기반 `m_isScheduled`로 중복 등록을 막습니다. Worker가 고정된 Zone을 맡는 방식은 아닙니다.
- **큐 보호와 Job 실행의 분리:** JobQueue와 ReadyQueue는 각각 mutex로 큐 접근을 보호합니다. Worker는 JobQueue에서 Job을 꺼낸 뒤 잠금을 풀고 실행하므로, Job 실행 중 큐 mutex를 잡고 있지 않습니다.
- **작업 대기:** ReadyQueue가 비면 Worker는 condition variable에서 기다립니다. Zone 등록 시 `notify_one()`으로 Worker 하나를 깨웁니다.
- **종료 경계:** `submit()`은 Job 저장과 필요한 ReadyQueue 등록이 끝날 때까지 shared lock을 유지합니다. `shutDown()`은 unique lock으로 새 접수를 막은 뒤 대기 Worker를 깨웁니다. Worker는 이미 등록된 Zone과 Job을 처리하고 종료하며, ThreadPool은 Worker들을 join합니다.

## 테스트 및 검증

[`tests/SchedulerTest.cpp`](tests/SchedulerTest.cpp)의 테스트는 다음 조건을 확인하고, 실패하면 실행 파일이 실패 코드로 종료됩니다.

- 여러 Producer가 제출한 **16,000개 Job이 모두 실행**됐는지 확인
- 데미지 Job 25개 실행 후 Zone의 모든 Actor가 예상 HP 75인지 확인
- 같은 Zone의 Job 실행 구간이 겹치지 않는지 확인하고, 순서대로 등록한 Job이 FIFO로 실행되는지 확인
- 서로 다른 Zone의 두 Job이 제한 시간 안에 같은 실행 구간에 진입하는지 확인
- submitter의 첫 Job 접수 후 shutdown을 시작해 **접수 수와 실행 수가 같은지**, 종료 후 제출이 거부되는지 확인
- 명시적 shutdown·join 없이도 소멸자가 Thread를 정리하고 제출된 Job 200개를 실행하는지 확인

## 범위와 한계

이 저장소는 Zone 단위 실행 모델을 검증하는 기술 데모로, 네트워크·DB·게임 콘텐츠나 Cross-Zone 상태 변경은 구현하지 않습니다.

- Worker는 선택한 Zone의 JobQueue가 빌 때까지 처리합니다. Job이 계속 들어오는 Hot Zone은 다른 Zone의 대기 시간을 늘릴 수 있으며, 공정성 제한은 구현하지 않았습니다.
- Job은 예외를 Worker 밖으로 전달하지 않는다는 계약을 전제로 합니다. 처리되지 않은 예외에 대한 복구·격리 정책은 없습니다.
- Zone 간 공유 상태까지 자동으로 보호하지는 않습니다. Zone은 Scheduler가 소유하며, Worker와 Producer보다 오래 살아 있어야 합니다.
- 테스트는 정확성과 동시 실행 규칙을 확인합니다. 처리량·지연 시간이나 Worker 수별 확장성은 측정하지 않았습니다.

## 프로젝트 구조

- [`src/ZoneScheduler.cpp`](src/ZoneScheduler.cpp), [`src/Zone.cpp`](src/Zone.cpp): Job 접수, Zone별 큐와 실행 상태 관리
- [`src/JobQueue.cpp`](src/JobQueue.cpp), [`src/ReadyQueue.cpp`](src/ReadyQueue.cpp): Job 저장과 실행 가능한 Zone 전달
- [`src/Worker.cpp`](src/Worker.cpp), [`src/ThreadPool.cpp`](src/ThreadPool.cpp): Zone의 Job 실행과 Worker Thread 정리
- [`tests/SchedulerTest.cpp`](tests/SchedulerTest.cpp): 실행 규칙과 종료 동작 검증

## Build / Run

CMake 3.20 이상과 C++20을 지원하는 컴파일러가 필요합니다. Windows에서는 Visual Studio Developer PowerShell에서 다음과 같이 빌드하고 테스트 출력을 볼 수 있습니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug -V
```

`ctest -V`가 실행 파일을 실행하고 테스트 출력을 표시합니다. 실행 파일 위치는 CMake generator에 따라 `build/` 또는 `build/Debug/`입니다.

## Tech Stack

C++20, CMake, MSVC에서 검증했습니다. 작업 분배와 동기화에는 `std::thread`, `std::mutex`, `std::shared_mutex`, `std::condition_variable`, `std::atomic`을 사용합니다.
