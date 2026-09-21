# ZoneJobScheduler

C++20으로 구현한 Zone 단위 멀티스레드 Job Scheduler 기술 데모입니다.

같은 Zone의 게임 상태는 한 번에 하나의 Worker만 변경하고, 
서로 다른 Zone은 여러 Worker가 병렬로 처리할 수 있는 실행 모델을 검증합니다.

## 구현 목적

일반적인 Thread Pool에서 Job을 곧바로 분배하면 여러 Worker가 같은 게임 상태를 동시에 변경할 수 있습니다. 
예를 들어, 두 Job이 같은 Actor의 HP를 동시에 읽고 수정하면 갱신 손실이 발생할 수 있습니다.

이 프로젝트는 lock을 Actor 마다 추가하는 대신 
Zone을 상태 정합성 단위로 정하고, 다음 규칙을 보장하는 것을 목표로 합니다.

- 같은 Zone에 속한 Job은 순차적으로 실행한다.
- 서로 다른 Zone에 속한 Job은 병렬로 실행할 수 있다.

## 실행 구조

```text
Producer
   |
   v
ZoneScheduler::submit(zoneId, job)
   |
   v
Zone별 JobQueue
   |
   | m_isScheduled CAS (false -> true)
   v
전역 ReadyQueue<Zone>
   |
   v
Worker Thread Pool
   |
   v
Zone의 Job을 순차 실행
```

ReadyQueue에는 Job이 아니라 실행 가능한 Zone이 들어갑니다. 
Zone의 `m_isScheduled`가 `true`이면,
해당 Zone이 ReadyQueue에 있거나 Worker에 의해 처리 중이라는 뜻입니다. 
`false -> true` 전환에 성공한 주체만 Zone을 ReadyQueue에 등록하므로 
같은 Zone이 여러 Worker에 동시에 배정되는 것을 방지합니다.

Worker가 Zone의 현재 Job을 모두 처리하면 `m_isScheduled`를 `false`로 바꾸고 JobQueue를 다시 확인합니다.
그 사이 새 Job이 들어왔다면 CAS를 통해 Zone을 다시 ReadyQueue에 등록하여 Job 유실을 방지합니다.

## 주요 구성 요소

| 구성 요소 | 역할 |
| --- | --- |
| `ZoneScheduler` | Zone 소유 및 Job 제출 경로 관리 |
| `Zone` | 순차 실행과 상태 정합성을 보장하는 단위 |
| `JobQueue` | Zone별 Job을 보호하는 thread-safe queue |
| `ReadyQueue` | 실행 가능한 Zone을 Worker에 전달하는 대기 queue |
| `Worker` | ReadyQueue에서 Zone을 가져와 해당 Zone의 Job 실행 |
| `ThreadPool` | 고정된 수의 Worker Thread 관리 |
| `Producer` | 테스트 Job 생성 및 Scheduler 제출 |
| `Actor` | Job이 실제로 변경하는 최소 게임 상태 객체 |

## 프로젝트 구조

```text
ZoneJobScheduler/
├─ include/        # 클래스 선언과 Job 타입
├─ src/            # 클래스 구현과 실행 진입점
├─ tests/          # Scheduler 실행 검증 코드
├─ CMakeLists.txt  # 빌드 대상과 테스트 정의
└─ README.md
```

## 사용한 동시성 요소

- `std::thread`
- `std::mutex`
- `std::condition_variable`
- `std::atomic<bool>`과 CAS
- `std::shared_mutex`로 Job 접수와 shutdown의 경계 보호
- C++20 `std::atomic::wait()`로 테스트 Thread 시작 대기
- RAII 기반 lock 관리
- `std::unique_ptr` 기반 소유권 관리

## 종료와 Job 접수 규약

`ZoneScheduler::submit()`은 Job을 접수하면 `true`, shutdown 이후라 접수하지 않으면
`false`를 반환합니다. `submit()`은 JobQueue 저장과 필요한 ReadyQueue 등록을
마칠 때까지 lifecycle shared lock을 유지합니다. `shutDown()`은 unique lock으로
진행 중인 제출이 끝나기를 기다린 뒤 새 Job 접수를 막고 ReadyQueue를 종료합니다.

ReadyQueue는 이미 등록된 Zone을 계속 Worker에 전달합니다. Worker는 접수된 Job을
처리한 뒤 종료하고, `ThreadPool::join()`은 모든 Worker의 종료를 기다립니다.
`ThreadPool` 소멸자도 Scheduler shutdown과 Worker join을 수행하므로 호출자가
명시적으로 종료 함수를 호출하지 않아도 Thread를 정리합니다. 이때 Scheduler는
ThreadPool과 Producer보다 오래 살아 있어야 합니다.

## 현재 검증된 항목

- 여러 Producer가 제출한 16,000개 Job이 모두 실행되는지 확인
- Job 실행 후 모든 Actor의 HP가 예상값과 같은지 확인
- 같은 Zone의 Job이 동시에 실행되지 않는지 확인
- 같은 Zone의 Job이 제출 순서대로 실행되는지 확인
- 서로 다른 Zone의 Job 두 개가 제한 시간 안에 같은 실행 구간에 진입하는지 확인
- 제출 스레드의 첫 Job 접수를 확인한 뒤 shutdown을 시작해 접수된 Job이 모두 실행되는지 확인
- 제출 스레드가 shutdown을 관찰하고 이후 제출을 중단하는지 확인
- 명시적인 shutdown·join 없이도 Producer와 ThreadPool 소멸자가 Thread를 정리하는지 확인

현재 테스트 출력 예시:

```text
Expected jobs: 16000
Executed jobs: 16000
Same-zone overlap detected: false
All actors have expected HP: true
Same-zone FIFO preserved: true
Different-zone overlap detected: true
Accepted jobs during shutdown: 108
Executed jobs during shutdown: 108
Submitter observed shutdown: true
Submit after shutdown rejected: true
Automatic cleanup executed jobs: 200 / 200
Submit after automatic cleanup rejected: true
```

shutdown 테스트의 접수 수에는 시작 전에 넣은 Job 100개와 제출 스레드가 접수한
Job이 모두 포함됩니다. 제출 스레드가 적어도 하나를 접수한 뒤 shutdown을 시작하며,
접수된 수와 실행된 수가 같은지, 종료 후 제출이 거부되는지 확인합니다. 접수 수는
실행 시점마다 달라질 수 있고, 두 스레드가 정확히 같은 순간 잠금을 놓고 경쟁했음을
보장하는 테스트는 아닙니다.

## 빌드 환경

- C++20
- CMake 3.20 이상
- Visual Studio 2022 이상 / MSVC

Visual Studio의 Developer PowerShell에서 다음과 같이 빌드하고 테스트할 수 있습니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

실행 파일 위치는 사용한 CMake generator에 따라 `build/` 또는 `build/Debug/`입니다.

## 범위와 한계

이 저장소는 MMORPG 서버 전체가 아니라 Zone 기반 실행 모델을 검증하는 작은 기술 데모입니다. 
다음 기능은 의도적으로 포함하지 않습니다.

- 네트워크 및 IOCP
- DB와 영속성
- 로그인, 인벤토리, 스킬 등의 게임 콘텐츠
- Cross-Zone 상태 변경
- Lock-free queue 및 Memory Pool

한 Zone에 Job이 집중되면 해당 Zone 내부는 순차 실행되므로
Worker 수를 늘려도 처리량이 선형으로 증가하지 않습니다.
Worker가 선택한 Zone의 JobQueue를 빌 때까지 처리하기 때문에, 계속 Job이
들어오는 Hot Zone은 다른 Zone의 대기 시간을 늘릴 수 있습니다.
Worker 수와 workload에 따른 차이는 추가 실험 후보로 남겨 두었습니다.

이 프로젝트는 정확성 검증에 초점을 맞췄으며 처리량이나 지연 시간의 성능
수치는 측정하지 않았습니다.

## 추가 실험 후보

핵심 정확성 검증은 완료했습니다. 다음 실험은 Scheduler의 확장성 한계를 측정하기 위한 선택 항목입니다.

1. Worker 수에 따른 처리량 비교
2. Balanced workload와 Hot Zone workload 비교
3. 한 Zone에 Job이 집중될 때 발생하는 직렬 처리 병목 측정

## Job 실행 계약

Worker는 제출된 Job이 예외를 밖으로 전달하지 않는다고 가정합니다.

Job 실행 도중 처리되지 않은 예외가 발생하면 `std::thread`의 진입 함수
밖으로 예외가 전달되어 `std::terminate()`가 호출될 수 있습니다.

실제 게임 서버에서는 예외 발생 시 상태가 일부만 변경됐을 가능성이 있으므로,
예외를 무조건 무시하지 않고 로깅, Zone 격리, 프로세스 종료 등의 정책을
서버 운영 방식에 맞게 결정해야 합니다.
