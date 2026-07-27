# Step 10 — 확장 과제

> 여기서부터는 정답이 없다. 관심 가는 것만 골라서 진행한다.

**선행:** [Step 9](step9-launch-and-integration.md) 완료

---

각 항목: 무엇을 하는지, 왜 배울 가치가 있는지, 시작점이 되는 API/명령.

## 쉬움

**rqt_graph / rqt_console로 관찰**
실행 중인 노드-토픽-서비스 연결을 그래프로, 로그를 필터링해서 본다. 텍스트로만 보던 시스템 구조를 시각적으로 확인하는 습관을 들인다.
시작점: `rqt_graph`, `rqt_console`

**bag 기록/재생**
`/odom`, `/robot_status`를 기록해뒀다가 나중에 재생한다. 재현 가능한 디버깅과 오프라인 분석의 기본기다.
시작점: `ros2 bag record /odom /robot_status`, `ros2 bag play <bag>`

**네임스페이스와 리매핑으로 로봇 2대**
같은 노드를 네임스페이스를 바꿔 두 번 실행해 로봇 2대를 동시에 시뮬레이션한다. 멀티 로봇 시스템의 가장 기본적인 구성법이다.
시작점: `ros2 run mini_mission robot_sim --ros-args -r __ns:=/robot1`, 토픽 리매핑은 `-r odom:=robot1/odom`

**시스템 진단 명령**
전체 ROS2 환경이 정상인지, 특정 노드의 상세 정보(구독/발행/서비스 목록)를 확인한다.
시작점: `ros2 doctor`, `ros2 node info /robot_sim`

## 중간

**MultiThreadedExecutor + CallbackGroup**
Step 5에서 예고했던 문제: 액션이 실행되는 동안 `/reset_pose` 서비스가 응답하는지 확인해본다. 기본 `SingleThreadedExecutor`에서는 콜백들이 하나의 스레드를 놓고 경쟁한다(단, execute를 별도 스레드로 넘겼다면 그 스레드는 별개이니 직접 관찰해서 확인한다). 막히는 지점이 있다면 `MutuallyExclusiveCallbackGroup`/`ReentrantCallbackGroup`으로 분리해 해결한다. 실무 ROS2 노드에서 반드시 마주치는 문제다.
시작점: `rclcpp::executors::MultiThreadedExecutor`, `rclcpp::CallbackGroupType`

**장애물 추가**
특정 좌표 근처를 지나면 액션을 `abort()`로 실패시킨다. 서버가 스스로 실패를 판단하고 알리는 경로를 처음으로 실습하게 된다.
시작점: execute 루프의 거리 계산 지점에 장애물 좌표와의 거리 체크 추가

**tf2 도입**
`/odom`을 `nav_msgs/msg/Odometry`로 바꾸고 TF를 broadcast해서 `rviz2`로 로봇 위치를 시각화한다. ROS2 좌표계 관리의 표준 도구를 처음 만져보는 단계다.
시작점: `tf2_ros::TransformBroadcaster`, `geometry_msgs/msg/TransformStamped`

**theta를 실제로 쓰기**
지금까지는 직선 이동만 했다. 목표 방향으로 먼저 회전한 뒤 직진하는 2단계 이동으로 바꿔 `theta`를 의미 있게 만든다.
시작점: `std::atan2(dy, dx)`로 목표 방향 계산

## 어려움

**Lifecycle Node로 robot_sim 전환**
`unconfigured → inactive → active` 상태 전이를 갖는 노드로 재구성한다. 상태 관리가 명시적으로 필요한 실무 노드(특히 하드웨어 드라이버)의 표준 패턴이다.
시작점: `rclcpp_lifecycle::LifecycleNode`, `ros2 lifecycle set /robot_sim configure`

**rclcpp_components로 컴포넌트화**
두 노드를 각각의 프로세스 대신 한 프로세스에 컴포넌트로 로드해 intra-process 통신을 실습한다. 통신 오버헤드가 어떻게 줄어드는지 직접 비교할 수 있다.
시작점: `rclcpp_components::register_node_macro`, `ros2 component load`

**노드 단위 테스트**
`launch_testing` 또는 `gtest`로 robot_sim의 이동 로직이나 mission_client의 순차 실행 로직을 자동 검증한다. 지금까지는 전부 수동으로 CLI를 두드려 확인했다.
시작점: `launch_testing`, `ament_add_gtest`

**QoS Deadline / Liveliness**
퍼블리셔가 죽거나 발행 주기가 늦어지는 것을 구독자가 이벤트 콜백으로 감지하게 만든다. Step 4에서 다룬 QoS 정책 중 실제로 쓰이는 빈도는 낮지만 장애 감지에 유용한 축이다.
시작점: `rclcpp::QoS::deadline`, `rclcpp::QoS::liveliness`

**DDS 구현체 바꿔보기**
`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`로 바꿔 같은 시스템이 다른 DDS 위에서 동일하게 동작하는지 확인한다. ROS2가 DDS를 추상화한 정도를 체감하는 실습이다.
시작점: `export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` 후 재실행 및 비교

## 이 커리큘럼이 다루지 않은 것
| 주제 | 언제 필요한가 |
|---|---|
| Nav2 | 실제 경로 계획, 장애물 회피가 필요한 자율주행 로봇 |
| MoveIt | 로봇 팔 등 매니퓰레이터의 모션 플래닝 |
| Gazebo/Ignition | 물리 시뮬레이션(충돌, 중력, 센서 노이즈)이 필요할 때 |
| micro-ROS | 마이크로컨트롤러급 저사양 장치에 ROS2를 올릴 때 |
| SROS2(보안) | 인증되지 않은 노드의 참여를 막아야 하는 배포 환경 |
| 실시간 실행기 | 마이크로초 단위 지연 보장이 필요한 제어 루프 |

---
이 문서로 커리큘럼이 끝난다. 필요하면 [Step 0](step0-setup.md)부터 다시 훑어보거나, 위 항목 중 하나를 골라 바로 시작한다.
