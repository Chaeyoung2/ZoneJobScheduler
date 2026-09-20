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
├─ src/            # 클래스 구현과 실행 검증 코드
├─ CMakeLists.txt  # 빌드 대상과 테스트 정의
└─ README.md
```

## 사용한 동시성 요소

- `std::thread`
- `std::mutex`
- `std::condition_variable`
- `std::atomic<bool>`과 CAS
- RAII 기반 lock 관리
- `std::unique_ptr` 기반 소유권 관리

## 현재 검증된 항목

- 여러 Producer가 제출한 16,000개 Job이 모두 실행되는지 확인
- 같은 Zone의 Job이 동시에 실행되지 않는지 확인
- 같은 Zone의 Job이 제출 순서대로 실행되는지 확인
- 서로 다른 Zone의 Job 실행 구간이 실제로 겹치는지 확인
- ReadyQueue shutdown 후 대기 중인 Worker가 종료되는지 확인
- 모든 Worker Thread가 `join()`을 통해 정상 종료되는지 확인

현재 테스트 출력 예시:

```text
Expected jobs: 16000
Executed jobs: 16000
Same-zone overlap detected: false
Same-zone FIFO preserved: true
Different-zone overlap detected: true
```

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
Worker 수와 workload에 따른 차이는 추가 실험 후보로 남겨 두었습니다.

## 추가 실험 후보

핵심 정확성 검증은 완료했습니다. 다음 실험은 Scheduler의 확장성 한계를 측정하기 위한 선택 항목입니다.

1. Worker 수에 따른 처리량 비교
2. Balanced workload와 Hot Zone workload 비교
3. 한 Zone에 Job이 집중될 때 발생하는 직렬 처리 병목 측정
