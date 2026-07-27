# Step 5 — 서비스

> **목표:** 요청/응답 통신 패턴을 이해하고 robot_sim에 `/reset_pose` 서비스를 추가한다.

**선행:** [Step 4](step4-qos-deep-dive.md) 완료
**소요:** 대략 30분

---

## 배우는 것
- 토픽과 서비스의 근본적인 차이: 브로드캐스트 vs 1:1 요청/응답
- `create_service`로 서비스 서버 만들기
- `std_srvs/srv/Trigger`의 구조와 언제 쓰는지
- 서비스 콜백의 동기적 특성과 블로킹 위험

## 만들 것
robot_sim에 `/reset_pose` (`std_srvs/srv/Trigger`) 서비스 서버를 추가한다. 호출되면 로봇의 (x, y, theta)를 (0, 0, 0)으로 되돌린다. mission_client는 이번 스텝에서 건드리지 않는다.

### 토픽 vs 서비스
| | 토픽 | 서비스 |
|---|---|---|
| 통신 방향 | 1:N 브로드캐스트 | 1:1 요청/응답 |
| 동기성 | 발행자는 구독자를 모름 | 호출자가 응답을 기다림(콜백 기반이면 비동기 대기도 가능) |
| 언제 쓰나 | 연속적인 상태/센서 스트림 | "지금 한 번 실행하고 결과를 알려줘" |
| 이 프로젝트 예 | `/odom`, `/robot_status` | `/reset_pose`, `/start_mission` |

판단 기준: 데이터가 시간에 따라 계속 흐르면 토픽, 한 번의 요청에 한 번의 답이면 서비스.

## 스켈레톤
**아래 스켈레톤은 그대로 붙여넣으면 컴파일된다. 내부 TODO만 채우면 된다.**

기존 파일에 더할 부분만 발췌했다.

`robot_sim.cpp` 에 추가:

```cpp
// ── include 추가 ──
#include "std_srvs/srv/trigger.hpp"

// ── private: 메서드 추가 ──
  void on_reset(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
    std::shared_ptr<std_srvs::srv::Trigger::Response> res)
  {
    // TODO: x_, y_, theta_ 를 0 으로 되돌린다
    // TODO: res->success 와 res->message 를 채운다
    (void)req;
  }

// ── private: 멤버 추가 ──
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_srv_;

// ── 생성자에 추가 ──
    // TODO: reset_srv_ = create_service<std_srvs::srv::Trigger>(
    //         "reset_pose",
    //         std::bind(&RobotSim::on_reset, this,
    //                   std::placeholders::_1, std::placeholders::_2));
```

`Trigger` 는 Request 가 비어 있다. 그래서 `req` 는 쓰지 않고 `(void)req;` 로 미사용 경고만 막는다.

## 힌트
- `rclcpp::Node::create_service<std_srvs::srv::Trigger>("reset_pose", 콜백)`
- 콜백 시그니처:
  ```cpp
  void reset_pose_callback(const std::shared_ptr<std_srvs::srv::Trigger::Request>, std::shared_ptr<std_srvs::srv::Trigger::Response>);
  ```
- `Trigger::Request`는 필드가 없다 — "그냥 실행해"용
- `Trigger::Response`는 `bool success`, `string message` 두 필드뿐
- 왜 Trigger인가: 인자가 필요 없는 요청에 커스텀 srv를 만들 이유가 없다. 기성 타입으로 표현되면 기성 타입을 쓴다
- 인자가 필요해지면 `std_srvs/srv/SetBool`을 먼저 고려하고, 그것도 부족할 때만 커스텀 srv를 만든다
- `package.xml`에 `<depend>std_srvs</depend>` 추가
- `CMakeLists.txt`의 의존 목록에도 `std_srvs` 추가

## 검증
터미널 A:
```bash
source /opt/ros/humble/setup.bash
ros2 run mini_mission robot_sim
```
터미널 B:
```bash
source /opt/ros/humble/setup.bash
ros2 service list
ros2 service type /reset_pose
ros2 service call /reset_pose std_srvs/srv/Trigger {}
```
터미널 C:
```bash
source /opt/ros/humble/setup.bash
ros2 topic echo /odom
```
기대 결과: `/reset_pose`가 서비스 목록에 보이고 타입은 `std_srvs/srv/Trigger`. 서비스 호출 시 `success: true` 응답이 오고, `/odom`의 x/y/theta가 0으로 리셋된다.

## 자주 밟는 지뢰
- `ros2 service call`에서 `{}`를 빼먹으면 인자 파싱 에러가 난다. Trigger도 빈 중괄호는 필요하다
- 서비스 콜백 안에서 오래 블로킹하면 노드 전체가 멈춘다(기본 `SingleThreadedExecutor`는 콜백을 하나씩 순서대로 처리한다) — Step 10에서 `MultiThreadedExecutor`로 다룬다
- 같은 노드 안에서 자기 서비스를 동기 호출(`spin_until_future_complete`)하면 데드락에 빠진다. 응답을 기다리는 스레드와 응답을 만들 스레드가 같기 때문

## 과제
- [ ] `/reset_pose` 호출 시 콘솔에 로그를 남기기(`RCLCPP_INFO`)
- [ ] 이미 원점에 있을 때 호출하면 `message`에 다른 문구를 넣어보기

---
다음: [Step 6 — 커스텀 액션과 액션 서버](step6-custom-action-server.md)
