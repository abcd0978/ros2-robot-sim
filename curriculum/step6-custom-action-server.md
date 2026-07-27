# Step 6 — 커스텀 액션과 액션 서버

> **목표:** `NavigateTo` 액션 인터페이스를 정의하고 robot_sim에 액션 서버를 구현해 로봇을 실제로 목표 지점까지 이동시킨다.

**선행:** [Step 5](step5-services.md) 완료
**소요:** 대략 90분

---

## 배우는 것
- 토픽/서비스/액션 3자의 역할 분담
- 액션 인터페이스(.action) 정의 문법과 인터페이스 패키지를 별도로 분리하는 이유
- 액션 하나가 서비스 2개 + 토픽 2개로 구현된다는 사실
- `rclcpp_action::create_server`의 goal/cancel/accepted 3개 콜백 역할
- 액션 실행을 별도 스레드로 넘겨야 하는 이유

## 만들 것
`mini_mission_interfaces` 패키지를 새로 만들어 `NavigateTo.action`을 정의하고, `mini_mission` 패키지의 robot_sim에 `/navigate_to` 액션 서버를 구현한다. 이 스텝이 끝나면 CLI로 goal을 보내 로봇을 실제로 이동시킬 수 있다.

### 왜 액션인가
| | 토픽 | 서비스 | 액션 |
|---|---|---|---|
| 오래 걸리는 작업 | - | 부적합(호출자가 블로킹) | 적합 |
| 중간 진행상황(feedback) | 가능하지만 억지 | 불가 | 내장 |
| 취소 가능 | 불가 | 불가 | 내장 |
| 이 프로젝트 예 | `/odom` | `/reset_pose` | `/navigate_to` |

로봇 이동은 몇 초씩 걸리고, 도중 거리를 알고 싶고, 도중에 멈추고 싶다 — 셋 다 필요하므로 액션이다.

---

## 6-A. `mini_mission_interfaces` 패키지

인터페이스 패키지는 노드 패키지(`mini_mission`)와 반드시 분리한다. 같은 패키지에 두면 인터페이스를 쓰는 노드와 인터페이스 자체가 서로 얽혀, 인터페이스를 고칠 때마다 노드 코드까지 재빌드되는 순환 의존 구조가 된다.

```bash
cd ~/ros2-robot-sim/src
source /opt/ros/humble/setup.bash
ros2 pkg create --build-type ament_cmake --license Apache-2.0 mini_mission_interfaces
```

여기엔 `--dependencies`가 없으므로 `--` 구분자도 필요 없다.

`src/mini_mission_interfaces/action/NavigateTo.action`:
```
# Goal
float64 x
float64 y
---
# Result
float64 elapsed_sec
bool reached
---
# Feedback
float64 distance_remaining
```
`---`로 구분된 3파트가 각각 Goal / Result / Feedback이다.

`CMakeLists.txt`에 추가 (반드시 `ament_package()` 호출보다 앞에 와야 한다):
```cmake
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "action/NavigateTo.action"
)
```

`package.xml`에 추가:
```xml
<buildtool_depend>rosidl_default_generators</buildtool_depend>
<exec_depend>rosidl_default_runtime</exec_depend>
<member_of_group>rosidl_interface_packages</member_of_group>
```

빌드 후 확인:
```bash
cd ~/ros2-robot-sim
colcon build --packages-select mini_mission_interfaces
source install/setup.bash
ros2 interface show mini_mission_interfaces/action/NavigateTo
```

액션 하나는 실제로 서비스 2~3개(`send_goal`, `cancel_goal`, `get_result`) + 토픽 2개(`feedback`, `status`)로 구현된다. 서버를 띄운 뒤 확인:
```bash
ros2 topic list --include-hidden-topics | grep navigate_to
```
`/navigate_to/_action/feedback`, `/navigate_to/_action/status` 등이 보인다.

---

## 6-B. robot_sim에 액션 서버 추가

`mini_mission`의 `package.xml`/`CMakeLists.txt`에 `rclcpp_action`, `mini_mission_interfaces` 의존을 추가한다.

## 스켈레톤
**아래 스켈레톤은 그대로 붙여넣으면 컴파일된다. 내부 TODO만 채우면 된다.**

기존 파일에 더할 부분만 발췌했다.

`robot_sim.cpp` 에 추가:

```cpp
// ── include 추가 ──
#include <cmath>
#include <thread>
#include "rclcpp_action/rclcpp_action.hpp"
#include "mini_mission_interfaces/action/navigate_to.hpp"

// ── public: 맨 위에 타입 별칭 추가 ──
  using NavigateTo = mini_mission_interfaces::action::NavigateTo;
  using GoalHandle = rclcpp_action::ServerGoalHandle<NavigateTo>;

// ── private: 메서드 추가 ──
  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const NavigateTo::Goal> goal)
  {
    // TODO: goal 좌표 검증. 거부하려면 GoalResponse::REJECT
    (void)uuid; (void)goal;
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(
    const std::shared_ptr<GoalHandle> goal_handle)
  {
    (void)goal_handle;
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandle> goal_handle)
  {
    // TODO: std::thread{std::bind(&RobotSim::execute, this, goal_handle)}.detach();
    //       여기서 execute 를 직접 부르면 노드 전체가 멈춘다
    (void)goal_handle;
  }

  void execute(const std::shared_ptr<GoalHandle> goal_handle)
  {
    // TODO: 루프
    //   1) 목표까지 남은 거리 계산
    //   2) pos_tolerance_ 이내면 결과 채우고 goal_handle->succeed(result) 후 종료
    //   3) 아니면 max_speed_ * dt 만큼 전진, set_status("MOVING")
    //   4) goal_handle->publish_feedback(feedback)
    //   5) publish_rate_ 주기로 sleep 후 반복
    (void)goal_handle;
  }

// ── private: 멤버 추가 ──
  rclcpp_action::Server<NavigateTo>::SharedPtr nav_server_;

// ── 생성자에 추가 ──
    // TODO: nav_server_ = rclcpp_action::create_server<NavigateTo>(
    //         this, "navigate_to",
    //         std::bind(&RobotSim::handle_goal, this,
    //                   std::placeholders::_1, std::placeholders::_2),
    //         std::bind(&RobotSim::handle_cancel, this, std::placeholders::_1),
    //         std::bind(&RobotSim::handle_accepted, this, std::placeholders::_1));
```

`goal_handle->get_goal()` 로 Goal 에, `std::make_shared<NavigateTo::Result>()` / `::Feedback` 로 결과·피드백 객체에 접근한다.

## 힌트
- 콜백 3개의 역할:
  - `handle_goal` — goal을 받을지 말지 결정. `GoalResponse::ACCEPT_AND_EXECUTE` 또는 `REJECT`
  - `handle_cancel` — 취소 요청을 받을지 결정. `CancelResponse::ACCEPT` 또는 `REJECT`
  - `handle_accepted` — 수락된 goal을 실제로 굴리기 시작. **반드시 별도 스레드로 넘긴다.** 여기서 직접 실행하면 executor 스레드가 막혀 구독·서비스가 전부 멈춘다
- execute 로직(프로즈로 직접 구현):
  - 목표 (x, y)까지의 남은 거리를 계산한다
  - 거리가 `pos_tolerance` 이내면 성공 처리
  - 아니면 `max_speed * dt`만큼 목표 방향으로 전진시키고, `/robot_status`를 `MOVING`으로, feedback으로 `distance_remaining`을 발행한 뒤 `publish_rate`에 맞춰 sleep, 다시 거리 계산부터 반복
  - 도착하면 `/robot_status`를 `REACHED`로 바꾸고 Result의 `elapsed_sec`, `reached`를 채운다
- 관련 API: `goal_handle->publish_feedback(feedback)`, `->succeed(result)`, `->abort(result)`
- goal 데이터는 `goal_handle->get_goal()`로 접근한다

## 검증
```bash
ros2 action list
ros2 action info /navigate_to -t
ros2 action send_goal /navigate_to mini_mission_interfaces/action/NavigateTo "{x: 2.0, y: 1.0}" --feedback
```
기대 결과: feedback이 주기적으로 출력되며 `distance_remaining`이 줄어들고, 다른 터미널의 `ros2 topic echo /odom`에서 로봇이 실제로 (2.0, 1.0) 방향으로 이동하는 것이 보인다. 도착하면 goal이 SUCCEEDED로 끝난다.

## 자주 밟는 지뢰
- `handle_accepted`에서 바로 execute를 실행해서 노드 전체가 멈춤(가장 흔함)
- 인터페이스 패키지를 빌드한 뒤 `source install/setup.bash`를 다시 안 해서 헤더를 못 찾음
- `.action` 파일을 `CMakeLists.txt`의 `rosidl_generate_interfaces`에 등록하지 않음
- `rosidl_generate_interfaces` 호출을 `ament_package()` 뒤에 둠 — 반드시 앞에 와야 함
- goal 좌표를 `{x: 2, y: 1}`처럼 정수로 보내면 타입 에러. `float64`이므로 `2.0`, `1.0`으로 보내야 함

## 과제
- [ ] `handle_goal`에서 좌표 절대값이 비정상적으로 크면(예: 1000 초과) REJECT 하도록 검증 추가
- [ ] `elapsed_sec`이 실제 걸린 시간과 맞는지 확인

---
다음: [Step 7 — 액션 클라이언트와 미션 실행](step7-action-client.md)
