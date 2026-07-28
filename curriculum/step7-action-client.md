# Step 7 — 액션 클라이언트와 미션 실행

> **목표:** mission_client에 액션 클라이언트를 붙여 waypoints 파라미터를 순서대로 실행하는 미션 로직을 완성한다.

**선행:** [Step 6](step6-custom-action-server.md) 완료
**소요:** 대략 60분

---

## 배우는 것
- `rclcpp_action::create_client`와 `SendGoalOptions`의 3개 콜백
- 콜백 체인으로 순차 작업을 표현하는 패턴
- 콜백 안에서 동기 대기를 하면 안 되는 이유
- double array 파라미터를 좌표쌍으로 파싱하기

## 만들 것
mission_client에 `/navigate_to` 액션 클라이언트, `waypoints` 파라미터, `/start_mission` 서비스를 추가한다. 이 스텝이 끝나면 `/start_mission`을 호출했을 때 로봇이 waypoints를 순서대로 전부 방문한다.

## 스켈레톤
**아래 스켈레톤은 그대로 붙여넣으면 컴파일된다. 내부 TODO만 채우면 된다.**

기존 파일에 더할 부분만 발췌했다.

`mission_client.cpp` 에 추가:

```cpp
// ── include 추가 ──
#include <vector>
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "mini_mission_interfaces/action/navigate_to.hpp"

// ── public: 맨 위에 타입 별칭 추가 ──
  using NavigateTo = mini_mission_interfaces::action::NavigateTo;
  using GoalHandle = rclcpp_action::ClientGoalHandle<NavigateTo>;

// ── private: 메서드 추가 ──
  void on_start(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
    std::shared_ptr<std_srvs::srv::Trigger::Response> res)
  {
    // TODO: 이미 미션 중이면 거절. 아니면 wp_index_ = 0 후 send_next_waypoint()
    (void)req; (void)res;
  }

  void send_next_waypoint()
  {
    // TODO: wp_index_ 가 끝까지 갔으면 종료 처리
    // TODO: waypoints_[wp_index_*2], waypoints_[wp_index_*2+1] 로 Goal 구성
    // TODO: SendGoalOptions 에 콜백 3개를 연결하고 nav_client_->async_send_goal(goal, options)
  }

  void on_goal_response(GoalHandle::SharedPtr goal_handle)
  {
    // TODO: nullptr 이면 서버가 goal 을 거절한 것
    (void)goal_handle;
  }

  void on_feedback(
    GoalHandle::SharedPtr goal_handle,
    const std::shared_ptr<const NavigateTo::Feedback> feedback)
  {
    // TODO: feedback->distance_remaining 출력
    (void)goal_handle; (void)feedback;
  }

  void on_result(const GoalHandle::WrappedResult & result)
  {
    // TODO: result.code 가 ResultCode::SUCCEEDED 면
    //       wp_index_++ 후 send_next_waypoint() 로 다음 목표
    (void)result;
  }

// ── private: 멤버 추가 ──
  std::vector<double> waypoints_;
  size_t wp_index_{0};

  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_srv_;
  rclcpp_action::Client<NavigateTo>::SharedPtr nav_client_;

// ── 생성자에 추가 ──
    // TODO: declare_parameter("waypoints", std::vector<double>{1.0,0.0, 1.0,1.0, 0.0,1.0, 0.0,0.0});
    // TODO: nav_client_ = rclcpp_action::create_client<NavigateTo>(this, "navigate_to");
    // TODO: start_srv_ = create_service<std_srvs::srv::Trigger>("start_mission", ...);
```

`SendGoalOptions` 는 `rclcpp_action::Client<NavigateTo>::SendGoalOptions()` 로 만든다. Humble의 `goal_response_callback` 은 `GoalHandle::SharedPtr` 를 직접 받는다 — 예전 배포판의 `std::shared_future` 형태가 아니다.

## 힌트
- 클라이언트 생성: `rclcpp_action::create_client<mini_mission_interfaces::action::NavigateTo>(this, "navigate_to")`
- 서버 대기: `client_->wait_for_action_server(std::chrono::seconds(N))` — 실패 시 false 반환
- goal 전송:
  ```cpp
  auto options = rclcpp_action::Client<NavigateTo>::SendGoalOptions{};
  client_->async_send_goal(goal, options);
  ```
- `SendGoalOptions`의 3개 콜백 필드와 역할:
  - `goal_response_callback` — 서버가 수락/거절했는지. 여기서 goal handle을 받아 멤버로 저장
  - `feedback_callback` — `distance_remaining`이 올 때마다 호출
  - `result_callback` — 최종 결과. 여기서 `result.code`(SUCCEEDED/ABORTED/CANCELED)를 확인
- `waypoints` 파라미터: `declare_parameter<std::vector<double>>("waypoints", {1.0,0.0, 1.0,1.0, 0.0,1.0, 0.0,0.0})`로 선언하고, 읽을 때 2개씩 끊어서 (x, y) 쌍으로 만든다. 개수가 홀수면 에러 처리
- **순차 실행 방법**: `result_callback` 안에서 다음 인덱스의 goal을 전송한다. "현재 몇 번째 웨이포인트인가"를 멤버 변수 하나(예: `current_index_`)로 관리하면, 하나가 끝날 때마다 그 콜백이 다음 것을 트리거하는 자연스러운 체인이 만들어진다 — 별도의 루프나 스레드가 필요 없다
- `/start_mission` 서비스 콜백에서 `current_index_ = 0`으로 초기화하고 첫 goal을 보낸다. 이미 미션이 진행 중이면 요청을 거절하는 가드를 둔다

## 검증
터미널 A:
```bash
source /opt/ros/humble/setup.bash
ros2 run mini_mission robot_sim
```
터미널 B:
```bash
source /opt/ros/humble/setup.bash
ros2 run mini_mission mission_client
```
터미널 C:
```bash
ros2 service call /start_mission std_srvs/srv/Trigger {}
```
터미널 D:
```bash
ros2 topic echo /odom
```
기대 결과: 로봇이 (1,0) → (1,1) → (0,1) → (0,0) 순서로 사각형을 그리며 이동하고, 각 구간에서 `/robot_status`가 MOVING → REACHED를 반복한다.

## 자주 밟는 지뢰
- 액션 서버가 안 떠 있는 상태에서 `async_send_goal`을 호출하면 조용히 아무 일도 안 일어난다. `wait_for_action_server`로 먼저 확인해야 원인을 알 수 있다
- `result_callback`에서 `result.code == rclcpp_action::ResultCode::SUCCEEDED` 확인 없이 무조건 다음 웨이포인트로 넘어가면, 실패한 경우에도 계속 진행해버린다
- `waypoints`를 `std::vector<double>`이 아니라 개별 `double` 파라미터 여러 개로 받으려는 시도 — ROS2 파라미터는 배열 타입을 직접 지원하므로 그럴 필요 없다
- **콜백(`goal_response_callback`, `result_callback` 등) 안에서 `spin_until_future_complete` 같은 동기 대기를 호출하면 데드락이다.** 콜백을 처리 중인 executor 스레드가 자기 자신이 끝나기를 기다리는 셈이기 때문. 그래서 이 스텝은 전부 콜백 체인으로 구성한다

## 과제
- [ ] 미션 진행 중 `/start_mission`을 또 호출하면 `success: false`와 이유가 담긴 메시지가 오는지 확인
- [ ] 웨이포인트를 다 돌면 콘솔에 총 소요 시간을 출력해보기

---

## 정답 코드

<details>
<summary>펼쳐서 보기 — 직접 구현한 뒤에 확인할 것</summary>

```cpp
// ── include 추가 ──
#include <vector>
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "mini_mission_interfaces/action/navigate_to.hpp"

// ── public: 타입 별칭 ──
  using NavigateTo = mini_mission_interfaces::action::NavigateTo;
  using GoalHandle = rclcpp_action::ClientGoalHandle<NavigateTo>;

// ── private: 메서드 추가 ──
  void on_start(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
    std::shared_ptr<std_srvs::srv::Trigger::Response> res)
  {
    (void)req;
    if (mission_active_) {
      res->success = false;
      res->message = "mission already running";
      return;
    }

    waypoints_ = get_parameter("waypoints").as_double_array();
    if (waypoints_.empty() || waypoints_.size() % 2 != 0) {
      res->success = false;
      res->message = "waypoints parameter is empty or has an odd number of values";
      return;
    }

    if (!nav_client_->action_server_is_ready()) {
      res->success = false;
      res->message = "action server not available";
      return;
    }

    wp_index_ = 0;
    mission_active_ = true;
    mission_start_ = now();
    send_next_waypoint();
    res->success = true;
    res->message = "mission started";
  }

  void send_next_waypoint()
  {
    if (wp_index_ * 2 >= waypoints_.size()) {
      mission_active_ = false;
      double elapsed = (now() - mission_start_).seconds();
      RCLCPP_INFO(get_logger(), "mission finished, total elapsed %.2fs", elapsed);
      return;
    }

    auto goal = NavigateTo::Goal();
    goal.x = waypoints_[wp_index_ * 2];
    goal.y = waypoints_[wp_index_ * 2 + 1];

    auto options = rclcpp_action::Client<NavigateTo>::SendGoalOptions();
    options.goal_response_callback =
      std::bind(&MissionClient::on_goal_response, this, std::placeholders::_1);
    options.feedback_callback =
      std::bind(&MissionClient::on_feedback, this, std::placeholders::_1, std::placeholders::_2);
    options.result_callback =
      std::bind(&MissionClient::on_result, this, std::placeholders::_1);

    RCLCPP_INFO(
      get_logger(), "sending waypoint %zu: (%.2f, %.2f)", wp_index_, goal.x, goal.y);
    nav_client_->async_send_goal(goal, options);
  }

  void on_goal_response(GoalHandle::SharedPtr goal_handle)
  {
    if (!goal_handle) {
      RCLCPP_ERROR(get_logger(), "goal rejected");
      mission_active_ = false;
      return;
    }
  }

  void on_feedback(
    GoalHandle::SharedPtr,
    const std::shared_ptr<const NavigateTo::Feedback> feedback)
  {
    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 1000,
      "distance remaining: %.2f", feedback->distance_remaining);
  }

  void on_result(const GoalHandle::WrappedResult & result)
  {
    switch (result.code) {
      case rclcpp_action::ResultCode::SUCCEEDED:
        RCLCPP_INFO(
          get_logger(), "waypoint reached=%d elapsed=%.2fs",
          result.result->reached, result.result->elapsed_sec);
        wp_index_++;
        send_next_waypoint();
        break;
      case rclcpp_action::ResultCode::ABORTED:
        RCLCPP_ERROR(get_logger(), "goal aborted");
        mission_active_ = false;
        break;
      case rclcpp_action::ResultCode::CANCELED:
        RCLCPP_WARN(get_logger(), "mission canceled");
        mission_active_ = false;
        wp_index_ = 0;
        break;
      default:
        RCLCPP_ERROR(get_logger(), "unknown result code");
        mission_active_ = false;
        break;
    }
  }

// ── private: 멤버 추가 ──
  std::vector<double> waypoints_;
  size_t wp_index_{0};
  bool mission_active_{false};
  rclcpp::Time mission_start_;

  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_srv_;
  rclcpp_action::Client<NavigateTo>::SharedPtr nav_client_;

// ── 생성자에 추가 ──
    declare_parameter<std::vector<double>>(
      "waypoints", {1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0});
    waypoints_ = get_parameter("waypoints").as_double_array();

    nav_client_ = rclcpp_action::create_client<NavigateTo>(this, "navigate_to");

    start_srv_ = create_service<std_srvs::srv::Trigger>(
      "start_mission",
      std::bind(&MissionClient::on_start, this, std::placeholders::_1, std::placeholders::_2));
```

> 순차 실행은 `on_result` → `wp_index_++` → `send_next_waypoint()` 콜백 체인 하나로 끝난다. 루프도 스레드도 필요 없다.
>
> `on_start` 에서 `wait_for_action_server()` 대신 `action_server_is_ready()` 를 쓰는 이유: 서비스 콜백은 executor 스레드 위에서 돌기 때문에 여기서 블로킹하면 노드 전체가 그 시간만큼 멈춘다.
>
> `waypoints_` 를 생성자에서 한 번 읽고 끝내지 않고 `on_start` 에서 다시 읽는 이유: 실행 중에 `ros2 param set /mission_client waypoints "[...]"` 로 바꾼 값이 다음 미션에 반영되게 하려는 것이다.

</details>

---
다음: [Step 8 — 취소 처리](step8-cancel-and-abort.md)
