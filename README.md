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

## 성능 측정

두 가지를 측정합니다. Worker 수에 따른 **처리량**과, Job이 몰린 Zone(Hot Zone)이 있을 때 다른 Zone이 겪는 **대기 지연**입니다.

### Worker 수별 처리량

[`bench/SchedulerBenchmark.cpp`](bench/SchedulerBenchmark.cpp)는 Worker 수에 따른 처리량을 측정합니다. 바꾸는 변수는 Worker 수(1/2/4/8) 하나이며, 나머지 조건은 고정합니다.

- Zone 8개, 제출 Thread 4개가 Job을 Zone에 번갈아 제출합니다. Zone 수를 최대 Worker 수 이상으로 두어, 같은 Zone 직렬 규칙 때문에 Worker가 노는 상황을 배제했습니다.
- Job 종류는 두 가지입니다. **빈 Job**은 Job 하나당 스케줄러 비용을, **약 1µs 계산 Job**은 게임 로직이 있을 때의 확장성을 봅니다.
- ThreadPool 생성은 측정 구간에서 제외하고, 제출 시작부터 모든 Job 실행과 Worker 종료(`join`)까지를 `steady_clock`으로 잽니다.
- Release 빌드에서 warm-up 1회를 버리고 5회 측정한 중앙값을 사용합니다. 실행된 Job 수는 측정 구간 밖에서 확인합니다.

#### 결과

MSVC Release, 16 논리 스레드 환경에서 측정했습니다.

| Workers | Job | Jobs | Median(ms) | Jobs/s | ns/job |
|---|---|---|---|---|---|
| 1 | empty | 1,000,000 | 80.3 | 12,455,906 | 80.3 |
| 2 | empty | 1,000,000 | 105.1 | 9,517,763 | 105.1 |
| 4 | empty | 1,000,000 | 136.7 | 7,315,733 | 136.7 |
| 8 | empty | 1,000,000 | 200.5 | 4,987,865 | 200.5 |
| 1 | ~1µs | 200,000 | 184.7 | 1,082,727 | 923.6 |
| 2 | ~1µs | 200,000 | 93.2 | 2,146,704 | 465.8 |
| 4 | ~1µs | 200,000 | 47.1 | 4,243,077 | 235.7 |
| 8 | ~1µs | 200,000 | 25.4 | 7,885,005 | 126.8 |

- **약 1µs Job:** Worker 1 → 8에서 처리량이 7.3배 증가해 거의 선형으로 확장됩니다. Job 비용이 µs 단위이면 스케줄러 비용이 전체 시간에서 작은 비중을 차지합니다.
- **빈 Job:** Worker가 늘수록 Job당 시간이 80ns → 200ns로 늘어납니다. Job이 비어 있으면 큐가 바로 비워져 Zone 해제·재등록·Worker 깨우기가 자주 반복되고, 여러 Thread가 같은 JobQueue mutex와 `m_isScheduled`, ReadyQueue를 두고 경합하는 비용이 드러나는 것으로 추정합니다. 이 원인은 프로파일러로 확인하지 않았습니다.

#### 측정 시 주의한 점

- **측정 장치의 경합 제거:** 실행 수와 계산 결과는 Zone별 카운터에 기록합니다. 같은 Zone의 Job은 동시에 실행되지 않으므로 atomic이 필요 없습니다. 공유 atomic 하나를 쓰면 스케줄러가 아니라 그 atomic의 경합을 측정하게 됩니다.
- **False sharing 방지:** Zone별 카운터를 `alignas(64)`로 캐시 라인마다 분리했습니다. 정렬하지 않으면 서로 다른 Zone을 처리하는 Worker들이 같은 캐시 라인을 번갈아 점유해 측정값이 왜곡됩니다.
- **계산 제거 방지:** 계산 결과를 누적해 마지막에 checksum으로 출력합니다. 결과를 사용하지 않으면 Release 최적화가 계산 루프를 제거해 1µs Job이 빈 Job이 됩니다.
- **보정과 측정의 코드 일치:** 1µs에 해당하는 반복 횟수를 처음에는 상수 반복 횟수로 보정했더니, 실제 Job이 약 2µs 걸렸습니다(보정 2591회/µs, 실측 2008ns). 어셈블리를 확인한 결과, 반복 횟수가 컴파일 시점에 알려진 보정 코드에서는 MSVC가 두 반복을 `v·a² + (a·c + c)` 한 번으로 합쳐 절반만 반복했고, 반복 횟수가 실행 중에 정해지는 실제 Job에서는 그렇지 않았습니다. 보정 반복 횟수를 `volatile` 변수로 읽어 두 경로가 같은 기계어로 컴파일되게 했고, 이후 보정 1172회/µs에 실측 약 0.92µs로 맞았습니다.

### Hot Zone 지연

Worker는 선택한 Zone의 JobQueue가 빌 때까지 처리합니다([`src/Worker.cpp`](src/Worker.cpp)의 `tryPop` 루프). Job이 계속 들어와 큐가 비지 않는 Zone은 그 Worker를 놓아주지 않습니다. [`bench/HotZoneBenchmark.cpp`](bench/HotZoneBenchmark.cpp)는 이때 **다른 Zone의 Job이 제출부터 실행 시작까지 얼마나 기다리는지**를 측정합니다. 바꾸는 변수는 Hot Zone 유무 하나이며, Worker 1/2/4/8마다 비교합니다.

- Zone 8개 중 Zone 0이 Hot Zone입니다. 별도 Thread가 처리되지 않은 Job을 약 1,000개로 유지하며 계속 제출해 큐가 비지 않게 합니다. 제한 없이 제출하면 큐가 끝없이 커지므로 미처리 수를 제한했습니다.
- Zone 1~7은 100µs마다 Zone별로 약 1µs Job을 하나씩 제출합니다. Worker 하나 기준 약 7%의 가벼운 부하로, 두 경우 모두 같습니다. 따라서 지연이 늘면 원인은 Hot Zone입니다.
- 제출 직전 시각을 Job에 담고, Job **실행이 시작될 때** 차이를 기록합니다. Job 자신의 계산 시간은 포함하지 않습니다.
- 측정은 Hot Zone이 Worker를 잡은 뒤에 시작합니다. 한 번에 200ms를 측정하고, warm-up 1회 후 5회의 p50/p99/max 각각의 중앙값을 사용합니다.

#### 결과

MSVC Release, 16 논리 스레드 환경에서 측정했습니다. Zone 1~7의 Job은 매 설정 14,000개가 모두 실행됐습니다.

| Workers | Hot Zone | Hot Zone Jobs | p50(µs) | p99(µs) | max(µs) |
|---|---|---|---|---|---|
| 1 | off | 0 | 5.0 | 16.3 | 48.1 |
| 1 | on | 218,232 | 107,358.8 | 205,758.0 | 211,941.1 |
| 2 | off | 0 | 3.3 | 6.1 | 24.8 |
| 2 | on | 217,743 | 4.9 | 14.8 | 146.9 |
| 4 | off | 0 | 2.4 | 3.9 | 12.1 |
| 4 | on | 218,923 | 2.7 | 4.6 | 15.1 |
| 8 | off | 0 | 2.4 | 4.1 | 8.8 |
| 8 | on | 217,232 | 2.4 | 3.9 | 8.8 |

- **Worker 1:** Hot Zone이 유일한 Worker를 계속 붙잡아, 다른 Zone은 측정하는 200ms 동안 실행되지 못했습니다(p50 5µs → 107ms). max 약 212ms는 측정 시간에 묶인 값이며, Hot Zone 부하가 계속되는 한 대기도 계속 늘어납니다.
- **Worker 2:** Hot Zone이 Worker 하나를 붙잡고 남은 1명이 Zone 1~7을 처리합니다. 그래서 p99가 14.8µs로, Worker 1·Hot Zone 없음(16.3µs)과 비슷합니다.
- **Worker 4·8:** 남은 Worker로 가벼운 부하를 충분히 감당해 차이가 거의 없습니다.
- 정리하면, Hot Zone 하나는 Worker 하나를 계속 점유합니다. 다른 Zone이 영향을 받는지는 **남은 Worker 수(전체 − Hot Zone 수)가 나머지 부하를 감당하는지**에 달려 있습니다.
- max는 한 번의 OS 스케줄링 지연에도 크게 흔들리므로(예: Worker 2·Hot Zone on에서 p99 14.8µs, max 146.9µs) 비교에는 p50/p99를 사용합니다.

#### 측정 시 주의한 점

- **측정 장치의 경합 제거:** 지연 기록은 Zone별 vector에 저장합니다. 같은 Zone의 Job은 동시에 실행되지 않으므로 잠금이 필요 없습니다. 측정 중 재할당이 지연에 섞이지 않도록 미리 `reserve`하고, Zone별 기록은 `alignas(64)`로 캐시 라인을 분리했습니다.
- **atomic이 필요한 곳만 atomic:** Hot Zone의 실행 수는 측정 중에 제출 Thread가 읽으므로 atomic입니다. 나머지 기록은 Worker 종료(`join`) 후에만 읽습니다.
- **µs 단위 제출 간격:** Windows의 `sleep_for`는 ms 단위로 깨어나 100µs 간격을 맞출 수 없으므로 시각을 확인하며 기다립니다(spin-wait). 간격은 시작 시각 기준 절대 시각으로 계산해 오차가 누적되지 않게 했습니다.

## 범위와 한계

이 저장소는 Zone 단위 실행 모델을 검증하는 기술 데모로, 네트워크·DB·게임 콘텐츠나 Cross-Zone 상태 변경은 구현하지 않습니다.

- Worker는 선택한 Zone의 JobQueue가 빌 때까지 처리합니다. Job이 계속 들어오는 Hot Zone은 Worker 하나를 계속 점유하며, Worker가 1개일 때 다른 Zone이 측정 내내 실행되지 못했습니다(위 Hot Zone 지연 측정). 공정성 제한은 구현하지 않았습니다.
- Job은 예외를 Worker 밖으로 전달하지 않는다는 계약을 전제로 합니다. 처리되지 않은 예외에 대한 복구·격리 정책은 없습니다.
- Zone 간 공유 상태까지 자동으로 보호하지는 않습니다. Zone은 Scheduler가 소유하며, Worker와 Producer보다 오래 살아 있어야 합니다.
- 테스트는 정확성과 동시 실행 규칙을 확인합니다. 성능은 Worker 수별 처리량과 Hot Zone 1개·가벼운 부하에서의 대기 지연을 측정했습니다. Hot Zone이 여러 개이거나 다른 Zone의 부하가 무거운 경우, CPU·메모리 사용량은 아직 측정하지 않았습니다.

## 프로젝트 구조

- [`src/ZoneScheduler.cpp`](src/ZoneScheduler.cpp), [`src/Zone.cpp`](src/Zone.cpp): Job 접수, Zone별 큐와 실행 상태 관리
- [`src/JobQueue.cpp`](src/JobQueue.cpp), [`src/ReadyQueue.cpp`](src/ReadyQueue.cpp): Job 저장과 실행 가능한 Zone 전달
- [`src/Worker.cpp`](src/Worker.cpp), [`src/ThreadPool.cpp`](src/ThreadPool.cpp): Zone의 Job 실행과 Worker Thread 정리
- [`tests/SchedulerTest.cpp`](tests/SchedulerTest.cpp): 실행 규칙과 종료 동작 검증
- [`bench/SchedulerBenchmark.cpp`](bench/SchedulerBenchmark.cpp): Worker 수별 처리량 측정
- [`bench/HotZoneBenchmark.cpp`](bench/HotZoneBenchmark.cpp): Hot Zone 유무에 따른 다른 Zone의 대기 지연 측정

CMake 타깃은 스케줄러 정적 라이브러리 `ZoneJobSchedulerCore`와, 이를 링크하는 정확성 테스트 `ZoneJobScheduler`, 성능 측정 `SchedulerBenchmark`·`HotZoneBenchmark`로 나뉩니다. 측정은 실행 시간이 길고 통과/실패를 판정하지 않으므로 CTest에 등록하지 않았습니다.

## Build / Run

CMake 3.20 이상과 C++20을 지원하는 컴파일러가 필요합니다. Windows에서는 Visual Studio Developer PowerShell에서 다음과 같이 빌드하고 테스트 출력을 볼 수 있습니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug -V
```

`ctest -V`가 실행 파일을 실행하고 테스트 출력을 표시합니다. 실행 파일 위치는 CMake generator에 따라 `build/` 또는 `build/Debug/`입니다.

성능 측정은 Release 빌드로 실행합니다.

```powershell
cmake --build build --config Release
.\build\Release\SchedulerBenchmark.exe
.\build\Release\HotZoneBenchmark.exe
```

## Tech Stack

C++20, CMake, MSVC에서 검증했습니다. 작업 분배와 동기화에는 `std::thread`, `std::mutex`, `std::shared_mutex`, `std::condition_variable`, `std::atomic`을 사용합니다.
