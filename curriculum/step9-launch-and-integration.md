# Step 9 — launch 파일과 통합

> **목표:** 두 노드를 launch 파일 하나로 띄우고, 시스템 전체가 설계대로 맞물려 동작하는지 통합 검증한다.

**선행:** [Step 8](step8-cancel-and-abort.md) 완료
**소요:** 대략 40분

---

## 배우는 것
- Python launch 파일로 여러 노드와 파라미터를 한 번에 구성하기
- `DeclareLaunchArgument`로 커맨드라인에서 값을 주입하는 방법
- launch 디렉토리를 install 트리에 포함시키는 CMake 설정
- 6개 인터페이스가 전부 살아있는지 체계적으로 확인하는 방법

## 만들 것
`launch/mission.launch.py`를 작성해 robot_sim과 mission_client를 파라미터와 함께 동시에 띄운다.

## launch 파일
`src/mini_mission/launch/mission.launch.py`:
```python
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    max_speed_arg = DeclareLaunchArgument('max_speed', default_value='1.0')

    robot_sim = Node(
        package='mini_mission',
        executable='robot_sim',
        name='robot_sim',
        output='screen',
        parameters=[{
            'max_speed': LaunchConfiguration('max_speed'),
            'pos_tolerance': 0.05,
            'publish_rate': 10.0,
        }],
    )

    mission_client = Node(
        package='mini_mission',
        executable='mission_client',
        name='mission_client',
        output='screen',
        parameters=[{
            'waypoints': [1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0],
        }],
    )

    return LaunchDescription([max_speed_arg, robot_sim, mission_client])
```

`CMakeLists.txt`에 추가:
```cmake
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
```

## 힌트
- 실행: `ros2 launch mini_mission mission.launch.py`
- 인자 오버라이드: `ros2 launch mini_mission mission.launch.py max_speed:=2.0`
- `output='screen'`이 없으면 `RCLCPP_INFO` 로그가 터미널에 안 보인다

## 검증
```bash
colcon build --symlink-install --packages-select mini_mission
source install/setup.bash
ros2 launch mini_mission mission.launch.py
```
새 터미널에서 6개 인터페이스가 전부 살아있는지 확인:

| 확인 대상 | 명령 | 기대 결과 |
|---|---|---|
| 노드 2개 | `ros2 node list` | `/robot_sim`, `/mission_client` |
| 토픽 2개 | `ros2 topic list` | `/odom`, `/robot_status` |
| 서비스 3개 | `ros2 service list` | `/reset_pose`, `/start_mission`, `/abort_mission` |
| 액션 1개 | `ros2 action list` | `/navigate_to` |
| 파라미터 | `ros2 param list` | robot_sim에 3개, mission_client에 `waypoints` |

`rqt_graph`로 전체 그림을 볼 수도 있다(컨테이너에 `/tmp/.X11-unix`가 마운트되어 X11이 연결되지만, `DISPLAY` 환경변수가 안 잡혀 있으면 먼저 확인/설정이 필요할 수 있다).

### 시나리오 테스트
1. `ros2 launch mini_mission mission.launch.py max_speed:=2.0`
2. `ros2 service call /start_mission std_srvs/srv/Trigger {}` → 미션 시작
3. 중간에 `ros2 service call /abort_mission std_srvs/srv/Trigger {}` → 즉시 멈추고 `/robot_status`가 `ABORTED`
4. `ros2 service call /reset_pose std_srvs/srv/Trigger {}` → 원점 복귀
5. 다시 `/start_mission` → 이번엔 끝까지 완주, `/robot_status`가 `REACHED`로 끝남

## 자주 밟는 지뢰
- launch 디렉토리를 CMakeLists의 `install`에 추가하지 않으면 `ros2 launch`가 파일을 못 찾는다
- `--symlink-install`로 빌드했더라도 launch 디렉토리를 새로 추가한 직후에는 최소 한 번 재빌드가 필요하다
- launch 파일에서 파라미터 타입이 노드가 기대하는 타입과 다르면(정수를 줬는데 double을 기대) 파라미터 검증에서 걸리거나 노드가 죽는다 — `1.0`처럼 소수점을 명시한다
- `output='screen'`을 빼먹으면 로그가 안 보여서 노드가 죽었는지 살았는지 헷갈린다

## 여기까지
`robot_sim`과 `mission_client` 두 노드로 토픽(BEST_EFFORT/TRANSIENT_LOCAL 두 QoS), 서비스 3개, 커스텀 액션 1개, 파라미터를 전부 갖춘 시스템을 완성했다. 웨이포인트 미션을 지시하고, 실시간으로 상태를 관찰하고, 도중에 취소할 수 있는 최소 로봇 미션 시스템이 launch 파일 하나로 재현 가능한 상태다.

## 과제
- [ ] `pos_tolerance`, `publish_rate`도 `DeclareLaunchArgument`로 노출해보기
- [ ] launch 파일에서 인라인 딕셔너리 대신 별도 YAML 파라미터 파일을 로드하는 방식으로 바꿔보기

---

## 정답 코드 — 전체 완성본

<details>
<summary>펼쳐서 보기 — Step 1~9 를 모두 반영한 최종 소스</summary>

**`src/mini_mission/src/robot_sim.cpp`**
```cpp
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

#include "geometry_msgs/msg/pose2_d.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "rcl_interfaces/msg/set_parameters_result.hpp"

#include "mini_mission_interfaces/action/navigate_to.hpp"

using namespace std::chrono_literals;

class RobotSim : public rclcpp::Node
{
public:
  using NavigateTo = mini_mission_interfaces::action::NavigateTo;
  using GoalHandle = rclcpp_action::ServerGoalHandle<NavigateTo>;

  RobotSim()
  : rclcpp::Node("robot_sim")
  {
    declare_parameter<double>("max_speed", 1.0);
    declare_parameter<double>("pos_tolerance", 0.05);
    declare_parameter<double>("publish_rate", 10.0);

    max_speed_ = get_parameter("max_speed").as_double();
    pos_tolerance_ = get_parameter("pos_tolerance").as_double();
    publish_rate_ = get_parameter("publish_rate").as_double();

    odom_pub_ = create_publisher<geometry_msgs::msg::Pose2D>(
      "odom", rclcpp::SensorDataQoS());

    status_pub_ = create_publisher<std_msgs::msg::String>(
      "robot_status", rclcpp::QoS(1).reliable().transient_local());

    set_status("IDLE");

    reset_srv_ = create_service<std_srvs::srv::Trigger>(
      "reset_pose",
      std::bind(&RobotSim::on_reset_pose, this, std::placeholders::_1, std::placeholders::_2));

    action_server_ = rclcpp_action::create_server<NavigateTo>(
      this,
      "navigate_to",
      std::bind(&RobotSim::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&RobotSim::handle_cancel, this, std::placeholders::_1),
      std::bind(&RobotSim::handle_accepted, this, std::placeholders::_1));

    param_cb_ = add_on_set_parameters_callback(
      std::bind(&RobotSim::on_param_change, this, std::placeholders::_1));

    reset_timer();
  }

private:
  // ---- state ----
  double x_{0.0}, y_{0.0}, theta_{0.0};
  std::string status_{""};

  double max_speed_{1.0};
  double pos_tolerance_{0.05};
  double publish_rate_{10.0};

  rclcpp::Publisher<geometry_msgs::msg::Pose2D>::SharedPtr odom_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_srv_;
  rclcpp_action::Server<NavigateTo>::SharedPtr action_server_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_cb_;

  void set_status(const std::string & s)
  {
    if (s != status_) {
      status_ = s;
      std_msgs::msg::String msg;
      msg.data = status_;
      status_pub_->publish(msg);
      RCLCPP_INFO(get_logger(), "status -> %s", status_.c_str());
    }
  }

  void reset_timer()
  {
    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / publish_rate_),
      std::bind(&RobotSim::on_timer, this));
  }

  void on_timer()
  {
    geometry_msgs::msg::Pose2D msg;
    msg.x = x_;
    msg.y = y_;
    msg.theta = theta_;
    odom_pub_->publish(msg);
  }

  void on_reset_pose(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
    std::shared_ptr<std_srvs::srv::Trigger::Response> res)
  {
    (void)req;
    if (x_ == 0.0 && y_ == 0.0 && theta_ == 0.0) {
      res->success = true;
      res->message = "already at origin";
    } else {
      x_ = 0.0;
      y_ = 0.0;
      theta_ = 0.0;
      res->success = true;
      res->message = "pose reset to origin";
    }
    RCLCPP_INFO(get_logger(), "reset_pose: %s", res->message.c_str());
  }

  rcl_interfaces::msg::SetParametersResult on_param_change(
    const std::vector<rclcpp::Parameter> & params)
  {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;

    bool rate_changed = false;

    for (const auto & p : params) {
      if (p.get_name() == "max_speed") {
        if (p.as_double() <= 0.0) {
          result.successful = false;
          result.reason = "max_speed must be > 0.0";
          return result;
        }
      } else if (p.get_name() == "pos_tolerance") {
        if (p.as_double() < 0.0) {
          result.successful = false;
          result.reason = "pos_tolerance must be >= 0.0";
          return result;
        }
      } else if (p.get_name() == "publish_rate") {
        if (p.as_double() <= 0.0) {
          result.successful = false;
          result.reason = "publish_rate must be > 0.0";
          return result;
        }
      }
    }

    for (const auto & p : params) {
      if (p.get_name() == "max_speed") {
        max_speed_ = p.as_double();
      } else if (p.get_name() == "pos_tolerance") {
        pos_tolerance_ = p.as_double();
      } else if (p.get_name() == "publish_rate") {
        publish_rate_ = p.as_double();
        rate_changed = true;
      }
    }

    if (rate_changed) {
      timer_->cancel();
      reset_timer();
    }

    return result;
  }

  // ---- action server ----
  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const NavigateTo::Goal> goal)
  {
    (void)uuid;
    if (std::abs(goal->x) > 1000.0 || std::abs(goal->y) > 1000.0) {
      RCLCPP_WARN(get_logger(), "rejecting goal (%.2f, %.2f): out of bounds", goal->x, goal->y);
      return rclcpp_action::GoalResponse::REJECT;
    }
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandle> goal_handle)
  {
    (void)goal_handle;
    RCLCPP_INFO(get_logger(), "cancel request received");
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandle> goal_handle)
  {
    std::thread{std::bind(&RobotSim::execute, this, goal_handle)}.detach();
  }

  void execute(const std::shared_ptr<GoalHandle> goal_handle)
  {
    const auto start = now();
    const auto goal = goal_handle->get_goal();
    auto result = std::make_shared<NavigateTo::Result>();
    auto feedback = std::make_shared<NavigateTo::Feedback>();

    while (rclcpp::ok()) {
      if (goal_handle->is_canceling()) {
        result->elapsed_sec = (now() - start).seconds();
        result->reached = false;
        set_status("ABORTED");
        goal_handle->canceled(result);
        RCLCPP_INFO(get_logger(), "goal canceled");
        return;
      }

      double dx = goal->x - x_;
      double dy = goal->y - y_;
      double dist = std::hypot(dx, dy);

      if (dist <= pos_tolerance_) {
        x_ = goal->x;
        y_ = goal->y;
        set_status("REACHED");
        result->elapsed_sec = (now() - start).seconds();
        result->reached = true;
        goal_handle->succeed(result);
        RCLCPP_INFO(get_logger(), "goal reached in %.2fs", result->elapsed_sec);
        return;
      }

      set_status("MOVING");
      double dt = 1.0 / publish_rate_;
      double step = max_speed_ * dt;
      if (step >= dist) {
        x_ = goal->x;
        y_ = goal->y;
      } else {
        x_ += dx / dist * step;
        y_ += dy / dist * step;
      }

      feedback->distance_remaining = dist;
      goal_handle->publish_feedback(feedback);

      std::this_thread::sleep_for(std::chrono::duration<double>(dt));
    }
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RobotSim>());
  rclcpp::shutdown();
  return 0;
}
```

**`src/mini_mission/src/mission_client.cpp`**
```cpp
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

#include "geometry_msgs/msg/pose2_d.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"

#include "mini_mission_interfaces/action/navigate_to.hpp"

class MissionClient : public rclcpp::Node
{
public:
  using NavigateTo = mini_mission_interfaces::action::NavigateTo;
  using GoalHandle = rclcpp_action::ClientGoalHandle<NavigateTo>;

  MissionClient()
  : rclcpp::Node("mission_client")
  {
    odom_sub_ = create_subscription<geometry_msgs::msg::Pose2D>(
      "odom", rclcpp::SensorDataQoS(),
      std::bind(&MissionClient::on_odom, this, std::placeholders::_1));

    status_sub_ = create_subscription<std_msgs::msg::String>(
      "robot_status", rclcpp::QoS(1).reliable().transient_local(),
      std::bind(&MissionClient::on_status, this, std::placeholders::_1));

    declare_parameter<std::vector<double>>(
      "waypoints", {1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0});
    waypoints_ = get_parameter("waypoints").as_double_array();

    nav_client_ = rclcpp_action::create_client<NavigateTo>(this, "navigate_to");

    start_srv_ = create_service<std_srvs::srv::Trigger>(
      "start_mission",
      std::bind(&MissionClient::on_start, this, std::placeholders::_1, std::placeholders::_2));

    abort_srv_ = create_service<std_srvs::srv::Trigger>(
      "abort_mission",
      std::bind(&MissionClient::on_abort, this, std::placeholders::_1, std::placeholders::_2));
  }

private:
  rclcpp::Subscription<geometry_msgs::msg::Pose2D>::SharedPtr odom_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr abort_srv_;
  rclcpp_action::Client<NavigateTo>::SharedPtr nav_client_;

  std::vector<double> waypoints_;
  size_t wp_index_{0};
  bool mission_active_{false};
  rclcpp::Time mission_start_;
  GoalHandle::SharedPtr current_goal_;

  void on_odom(const geometry_msgs::msg::Pose2D::SharedPtr msg)
  {
    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 1000,
      "odom: x=%.2f y=%.2f theta=%.2f", msg->x, msg->y, msg->theta);
  }

  void on_status(const std_msgs::msg::String::SharedPtr msg)
  {
    RCLCPP_INFO(get_logger(), "status: %s", msg->data.c_str());
  }

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

  void on_abort(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
    std::shared_ptr<std_srvs::srv::Trigger::Response> res)
  {
    (void)req;
    if (!mission_active_ || !current_goal_) {
      res->success = false;
      res->message = "no mission in progress";
      return;
    }

    nav_client_->async_cancel_goal(current_goal_);
    current_goal_.reset();
    mission_active_ = false;
    wp_index_ = 0;
    res->success = true;
    res->message = "mission aborted";
  }

  void send_next_waypoint()
  {
    if (wp_index_ * 2 >= waypoints_.size()) {
      mission_active_ = false;
      current_goal_.reset();
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
    current_goal_ = goal_handle;
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
        current_goal_.reset();
        wp_index_++;
        send_next_waypoint();
        break;
      case rclcpp_action::ResultCode::ABORTED:
        RCLCPP_ERROR(get_logger(), "goal aborted");
        mission_active_ = false;
        current_goal_.reset();
        break;
      case rclcpp_action::ResultCode::CANCELED:
        RCLCPP_WARN(get_logger(), "mission canceled");
        mission_active_ = false;
        current_goal_.reset();
        wp_index_ = 0;
        break;
      default:
        RCLCPP_ERROR(get_logger(), "unknown result code");
        mission_active_ = false;
        current_goal_.reset();
        break;
    }
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MissionClient>());
  rclcpp::shutdown();
  return 0;
}
```

**`src/mini_mission/launch/mission.launch.py`**
```python
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    max_speed_arg = DeclareLaunchArgument('max_speed', default_value='1.0')

    robot_sim = Node(
        package='mini_mission',
        executable='robot_sim',
        name='robot_sim',
        output='screen',
        parameters=[{
            'max_speed': LaunchConfiguration('max_speed'),
            'pos_tolerance': 0.05,
            'publish_rate': 10.0,
        }],
    )

    mission_client = Node(
        package='mini_mission',
        executable='mission_client',
        name='mission_client',
        output='screen',
        parameters=[{
            'waypoints': [1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0],
        }],
    )

    return LaunchDescription([max_speed_arg, robot_sim, mission_client])
```

**`src/mini_mission/CMakeLists.txt`**
```cmake
cmake_minimum_required(VERSION 3.8)
project(mini_mission)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(mini_mission_interfaces REQUIRED)

add_executable(robot_sim src/robot_sim.cpp)
ament_target_dependencies(robot_sim
  rclcpp
  rclcpp_action
  geometry_msgs
  std_msgs
  std_srvs
  mini_mission_interfaces
)

add_executable(mission_client src/mission_client.cpp)
ament_target_dependencies(mission_client
  rclcpp
  rclcpp_action
  geometry_msgs
  std_msgs
  std_srvs
  mini_mission_interfaces
)

install(TARGETS robot_sim mission_client
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
```

**`src/mini_mission_interfaces/`**
Step 6 의 정답 코드와 동일하다.

> 이 소스는 컨테이너 안 별도 워크스페이스에서 `colcon build --symlink-install` 클린 빌드 후, Step 9 의 시나리오 테스트(미션 완주 → 중도 abort → reset → 재시작)와 파라미터 거부/수락, 범위 밖 goal 거부까지 전부 실제로 돌려서 확인한 것이다.

</details>

---
다음: [Step 10 — 확장 과제](step10-extensions.md)
