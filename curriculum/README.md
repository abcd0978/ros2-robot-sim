# ros2-robot-sim 학습 커리큘럼

가상 2D 로봇에게 웨이포인트 미션을 지시하고, 진행 상황을 실시간으로 모니터링하고,
도중에 취소할 수 있는 동작하는 최소 ROS2 시스템을 rclcpp(C++)로 직접 구현하는 커리큘럼이다.
하드웨어는 없다. 전부 소프트웨어 시뮬레이션이다.

**궁극적 목표:** `mission_client`가 사각형 웨이포인트를 순서대로 `robot_sim`에 액션 goal로
보내고, 로봇이 이동하는 동안 odom/status를 구독해 진행 상황을 확인하며, 필요하면 서비스
호출로 미션을 중단시킬 수 있는 2노드 시스템.

## 왜 이 시스템인가

ROS2의 통신 축은 크게 다섯 가지다: 토픽, 서비스, 액션, 파라미터, QoS. 이 다섯을 전부
자연스럽게 요구하면서도 억지로 끼워맞추지 않는 최소 크기 시스템이 바로 "미션을 수행하는
이동 로봇"이다.

- 위치는 고주기로 흘러야 하니 **토픽**이 필요하다
- 로봇 초기화처럼 즉시 응답이 필요한 단발 요청엔 **서비스**가 필요하다
- 목표 지점까지 이동은 시간이 걸리고 중간 진행률과 취소가 필요하니 **액션**이 필요하다
- 최고 속도나 허용 오차 같은 값은 실행 중 조정 가능해야 하니 **파라미터**가 필요하다
- 센서 스트림과 상태값은 요구되는 전달 보장이 다르니 **QoS**를 구분해야 한다

이 다섯이 전부 필요한 시스템 중 이보다 작은 것은 찾기 어렵다.

## 최종 시스템 구성

```
   mission_client                                  robot_sim
        │                                              │
        ├── action client ──── /navigate_to ──────────►│ action server
        ├── sub /odom ◄─────── BEST_EFFORT ────────────┤ pub 10Hz
        ├── sub /robot_status ◄─ TRANSIENT_LOCAL ──────┤ pub on change
        │                                              │
   srv: /start_mission                            srv: /reset_pose
        /abort_mission
   param: waypoints                               param: max_speed
                                                         pos_tolerance
                                                         publish_rate
```

### 노드 1 — `robot_sim` (가상 로봇 본체)

자기 위치(x, y, theta)를 들고 있다가 액션 goal이 오면 그쪽으로 등속 이동한다.

- publisher `/odom` (`geometry_msgs/msg/Pose2D`) — publish_rate Hz로 주기 발행
- publisher `/robot_status` (`std_msgs/msg/String`) — 상태가 바뀔 때만 발행. 값은
  `IDLE`/`MOVING`/`REACHED`/`ABORTED`
- service server `/reset_pose` (`std_srvs/srv/Trigger`) — 위치를 원점(0,0,0)으로
- action server `/navigate_to` (`mini_mission_interfaces/action/NavigateTo`)
- parameters: `max_speed`(m/s, 기본 1.0), `pos_tolerance`(m, 기본 0.05), `publish_rate`(Hz, 기본 10.0)

### 노드 2 — `mission_client` (미션 실행기)

웨이포인트 리스트를 순서대로 하나씩 `/navigate_to`에 goal로 보낸다.

- action client `/navigate_to`
- subscriber `/odom`, `/robot_status`
- service server `/start_mission` (`std_srvs/srv/Trigger`) — 미션 시작
- service server `/abort_mission` (`std_srvs/srv/Trigger`) — 진행 중인 goal을 cancel
- parameter `waypoints` (double array, `[x1,y1, x2,y2, ...]`, 기본 `[1.0,0.0, 1.0,1.0, 0.0,1.0, 0.0,0.0]`)

## 인터페이스 목록

| 종류 | 이름 | 타입 | 제공자 |
|---|---|---|---|
| Topic | `/odom` | `geometry_msgs/msg/Pose2D` | robot_sim |
| Topic | `/robot_status` | `std_msgs/msg/String` | robot_sim |
| Service | `/reset_pose` | `std_srvs/srv/Trigger` | robot_sim |
| Service | `/start_mission` | `std_srvs/srv/Trigger` | mission_client |
| Service | `/abort_mission` | `std_srvs/srv/Trigger` | mission_client |
| Action | `/navigate_to` | `mini_mission_interfaces/action/NavigateTo` | robot_sim |

커스텀 인터페이스는 액션 하나뿐이다. 나머지는 전부 기성 타입(std_srvs, geometry_msgs,
std_msgs)을 쓴다. 필요 없는 커스텀 메시지는 만들지 않는다.

## QoS 설계

| 토픽 | Reliability | Durability | History | 이유 |
|---|---|---|---|---|
| `/odom` | BEST_EFFORT | VOLATILE | KEEP_LAST(1) | 고주기 센서 스트림. 놓친 옛날 위치는 가치 없음 |
| `/robot_status` | RELIABLE | TRANSIENT_LOCAL | KEEP_LAST(1) | 상태는 반드시 도달해야 하고, 늦게 접속한 노드도 현재 상태를 알아야 함 |

이 두 토픽을 같은 프로젝트 안에 나란히 두는 것 자체가 학습 목적이다. `ros2 topic echo`를
나중에 켰을 때 `/robot_status`는 즉시 값이 뜨고 `/odom`은 안 뜨는 것을 직접 눈으로 확인한다
(Step 4).

## 디렉토리 구조

```
ros2-robot-sim/                 # 레포 루트이자 colcon 워크스페이스
├── curriculum/                 # 이 커리큘럼 문서들
├── src/
│   ├── mini_mission_interfaces/    # Step 6에서 생성. 액션 정의만
│   │   ├── action/NavigateTo.action
│   │   ├── CMakeLists.txt
│   │   └── package.xml
│   └── mini_mission/               # Step 0에서 생성. 노드 2개
│       ├── src/robot_sim.cpp
│       ├── src/mission_client.cpp
│       ├── launch/mission.launch.py
│       ├── CMakeLists.txt
│       └── package.xml
├── .gitignore                  # build/ install/ log/
├── build/  install/  log/      # colcon 산출물, git에 안 올림
└── README.md
```

## 환경

- 도커 컨테이너: `superpx4_ros2` (호스트에서 `docker exec -it superpx4_ros2 bash`)
- ROS2 Humble, Ubuntu 22.04, gcc 11.4, cmake 3.22
- 레포 위치: 컨테이너 안 `/root/ros2-robot-sim`
- `ros2` 명령이 기본 PATH에 없다. 셸을 열 때마다 `source /opt/ros/humble/setup.bash` 필요
  (Step 0에서 `~/.bashrc` 등록)
- `RMW_IMPLEMENTATION` 미설정 → 기본 `rmw_fastrtps_cpp`, `ROS_DOMAIN_ID=0`
- `rclcpp`, `rclcpp_action`, `std_srvs`, `std_msgs`, `geometry_msgs`,
  `rosidl_default_generators` 전부 설치되어 있다. 추가 설치 불필요
- 기존 `/root/ros2_ws`에는 무거운 `px4_msgs`가 있으니 거기서 빌드하지 말 것. 이 프로젝트는
  `/root/ros2-robot-sim`에서 독립적으로 빌드한다

## 전체 스텝 목차

| 스텝 | 문서 | 새로 배우는 것 |
|---|---|---|
| 0 | [환경 준비와 첫 빌드](step0-setup.md) | colcon 워크스페이스, 패키지 생성, 첫 빌드 |
| 1 | [노드와 토픽 발행](step1-node-and-topic.md) | rclcpp::Node, publisher, 타이머 |
| 2 | [구독자와 QoS 첫 만남](step2-subscriber-and-qos.md) | subscription, SensorDataQoS |
| 3 | [파라미터](step3-parameters.md) | declare_parameter, 파라미터 검증 콜백 |
| 4 | [QoS 파고들기](step4-qos-deep-dive.md) | Reliability/Durability 호환성, TRANSIENT_LOCAL |
| 5 | [서비스](step5-services.md) | service server/client, Trigger |
| 6 | [커스텀 액션과 액션 서버](step6-custom-action-server.md) | .action 정의, action server |
| 7 | [액션 클라이언트와 미션 실행](step7-action-client.md) | action client, goal/feedback/result |
| 8 | [취소 처리](step8-cancel-and-abort.md) | cancel goal, abort 흐름 |
| 9 | [launch 파일과 통합](step9-launch-and-integration.md) | launch.py, 전체 시스템 통합 |
| 10 | [확장 과제](step10-extensions.md) | 자유 확장 |

## 학습 규칙

- 한 스텝씩 진행한다. 이전 스텝의 "검증"이 통과하지 않으면 다음 스텝으로 넘어가지 않는다
- 코드는 반드시 직접 친다. 커리큘럼 문서에 완성된 코드가 없는 것은 의도된 설계다 — 힌트는
  API 이름과 시그니처까지만 제공한다
- 막히면 "자주 밟는 지뢰" 절부터 확인한다
- 각 스텝 문서의 `## 스켈레톤` 절에 컴파일되는 뼈대 코드가 있다. 선언부는 주어지고, 내부 TODO만 직접 채운다

## 자주 쓰는 명령어 치트시트

```bash
# 빌드
colcon build --symlink-install
source install/setup.bash

# 노드 실행
ros2 run mini_mission robot_sim
ros2 run mini_mission mission_client

# 조사
ros2 node list
ros2 topic list
ros2 topic echo /odom
ros2 topic hz /odom
ros2 topic info /odom -v
ros2 service list
ros2 service call /reset_pose std_srvs/srv/Trigger
ros2 action list
ros2 action send_goal /navigate_to mini_mission_interfaces/action/NavigateTo "{x: 1.0, y: 0.0}"
ros2 param list
ros2 param get /robot_sim max_speed
ros2 param set /robot_sim max_speed 2.0
```
