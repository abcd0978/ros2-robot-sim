# 분석해볼만한 repository

이 커리큘럼(Step 0~10)을 끝낸 뒤 읽을 실무 ROS2 코드베이스. 드론 펌웨어 배경 기준으로 골랐다.

## 권하는 순서

```
1. px4_offboard_study/offboard_controller.py    이미 디스크에 있다 (/root/ros2_ws/src/)
2. px4_ros_com                                  같은 걸 C++ 로
3. nav2_util/simple_action_server.hpp           Step 8 완주 직후에. 내가 짠 것과 대비
4. ros2_control/hardware_interface              하드웨어 경계 설계
```

**3번을 Step 8 이후로 미루는 게 중요하다.** 액션 서버를 손으로 짜본 직후에 읽으면 "내가 한 것을
이들은 이렇게 일반화했구나"가 보인다. 먼저 읽으면 그 대비가 사라진다.

---

## 1. px4_ros_com — 도메인이 겹친다

`https://github.com/PX4/px4_ros_com`

**규모:** 작다. 예제 노드 10개 미만, 하루면 읽는다.

| 볼 것 | 왜 |
|---|---|
| uORB 토픽 구독의 `BEST_EFFORT` 강제 | Step 4 의 QoS 불일치가 실무에서 나타나는 정확한 사례. uXRCE-DDS 브리지가 BEST_EFFORT 로 발행하므로 구독자가 RELIABLE 이면 조용히 아무것도 안 온다 |
| `OffboardControl` 예제의 상태 머신 | 타이머 콜백 안에서 카운터로 모드 전환. `robot_sim::on_timer` 와 구조가 같다 |
| `VehicleCommand` 발행 패턴 | 서비스 대신 토픽으로 명령을 보내는 설계 판단 |

관련해서 컨테이너에 이미 있는 것:
- `/root/ros2_ws/src/px4_msgs` — 인터페이스 전용 패키지. `mini_mission_interfaces` 와 같은 구조, 규모만 훨씬 크다
- `/root/ros2_ws/src/px4_offboard_study/offboard_controller.py` — 파이썬 버전 예제. **여기부터 시작**
- `/root/PX4-Autopilot/src/modules/uxrce_dds_client/` — PX4 쪽 DDS 클라이언트. `dds_topics.yaml` 이 어떤 uORB 토픽을 ROS2 로 내보내는지 정의한다

---

## 2. Nav2 — "규모 있는 것"의 정답

`https://github.com/ros-navigation/navigation2` (브랜치 `humble`)

**규모:** 패키지 30개+, C++ 15만 줄. **전부 읽지 말고 골라 읽는다.**

**왜:** 액션 서버를 대규모로 쓴 표준 사례다. 이 커리큘럼의 `NavigateTo` 가 Nav2 의 `NavigateToPose`
를 축소한 것이고, 설계가 사실상 Nav2 를 따라간 것이다.

읽을 것만:

| 대상 | 배울 것 |
|---|---|
| `nav2_msgs/action/*.action` | 실무 액션 정의. `NavigateToPose`, `ComputePathToPose` |
| `nav2_util/include/nav2_util/simple_action_server.hpp` | **액션 서버 래퍼.** goal 선점(preemption), 취소, 별도 스레드 관리의 일반화. Step 6~8 에서 손으로 짠 것의 라이브러리 버전 |
| `nav2_util/lifecycle_node.hpp` | Lifecycle Node — Step 10 확장 과제의 실물 |
| `nav2_bt_navigator/` | Behavior Tree 로 액션 조합. 액션 클라이언트가 액션 서버를 부르는 계층 구조 |
| `nav2_controller/` | 제어 루프 + pluginlib 플러그인 구조 |

**함정:** `nav2_costmap_2d` 나 `nav2_smac_planner` 로 먼저 들어가면 알고리즘에 파묻힌다.
통신 구조만 보려면 `nav2_util` 과 `nav2_msgs` 가 핵심이다.

---

## 3. ros2_control — 임베디드 배경에 가장 잘 맞는다

`https://github.com/ros-controls/ros2_control` (브랜치 `humble`)

**규모:** 중간. 패키지 8개 정도.

**왜:** 하드웨어와 ROS2 사이의 경계 설계가 주제다. 펌웨어에서 ROS2 로 올라올 때 필요한 지식.

| 대상 | 배울 것 |
|---|---|
| `hardware_interface/` | 실제 장치 추상화. `read()` / `write()` 사이클 |
| 실시간 안전 코드 | 제어 루프에서 힙 할당·락·`std::function` 을 피하는 방법. `std::function` 의 간접 호출·힙 할당 비용이 실제로 문제가 되는 영역 |
| `controller_manager/` | 동적 로딩, 라이프사이클 전이 |

---

## 4. rclcpp 자체

헤더는 이미 컨테이너에 있다 (`/opt/ros/humble/include/rclcpp/rclcpp/`). 구현까지 보려면:

`https://github.com/ros2/rclcpp` (브랜치 `humble`)

| 대상 | 배울 것 |
|---|---|
| `executors/` | SingleThreaded vs MultiThreaded. Step 10 확장의 근거 |
| `callback_group.hpp` | 구독·타이머·서비스를 `weak_ptr` 로 들고 있는 이유 → 핸들을 멤버에 저장해야 하는 규칙의 근원 |
| `node_interfaces/` | Node 가 여러 인터페이스로 쪼개져 있는 이유. `rclcpp_action::create_server` 가 자유 함수인 까닭 |
| `wait_set*.hpp` | DDS 이벤트를 기다리는 방식 |

---

## 그 외 — 목적별

| 프로젝트 | 규모 | 볼 이유 |
|---|---|---|
| `turtlebot3` | 소~중 | 노드 구성이 깔끔하다. 입문용 실물 로봇 코드 |
| `slam_toolbox` | 중 | 무거운 계산을 콜백에서 빼내는 방법 |
| `micro-ROS` | 중 | MCU 에 ROS2 올리기. `/root/Micro-XRCE-DDS-Agent` 가 이미 있다 |
| `Autoware` | 매우 큼 | 참고용. 통독 대상은 아니다 |
