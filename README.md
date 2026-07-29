# ros2-robot-sim

가상 2D 로봇에게 웨이포인트 미션을 지시하고, 진행 상황을 실시간으로 보고, 도중에 취소할 수 있는
최소 ROS2 시스템. rclcpp(C++)로 직접 구현하며 배우는 학습용 워크스페이스다. 하드웨어는 없다.

토픽 · 서비스 · 액션 · 파라미터 · QoS 다섯 축을 전부 자연스럽게 요구하는 최소 크기의 시스템이다.

## 구성

```
mission_client                                  robot_sim
     ├── action client ──── /navigate_to ──────►│ action server
     ├── sub /odom ◄─────── BEST_EFFORT ────────┤ pub (publish_rate Hz)
     ├── sub /robot_status ◄─ TRANSIENT_LOCAL ──┤ pub (상태 변화 시에만)
     │                                          │
srv: /start_mission                        srv: /reset_pose
     /abort_mission
param: waypoints                         param: max_speed, pos_tolerance, publish_rate
```

## 환경

- 도커 컨테이너 `superpx4_ros2` / ROS2 Humble / Ubuntu 22.04
- `ros2` 가 기본 PATH 에 없다 — 셸마다 `source /opt/ros/humble/setup.bash` 필요

## 빌드와 실행

```bash
cd ~/ros2-robot-sim
source /opt/ros/humble/setup.bash

colcon build --symlink-install
source install/setup.bash            # 새 터미널마다 필요

ros2 run mini_mission robot_sim      # 터미널 A
ros2 run mini_mission mission_client # 터미널 B
ros2 launch mini_mission mission.launch.py   # 또는 한 번에
```

인터페이스를 고쳤을 때는 그쪽을 먼저 빌드하고 소싱한다.

```bash
colcon build --packages-select mini_mission_interfaces
source install/setup.bash
colcon build --packages-select mini_mission
```

## 조작

```bash
ros2 service call /start_mission std_srvs/srv/Trigger {}
ros2 service call /abort_mission std_srvs/srv/Trigger {}
ros2 service call /reset_pose    std_srvs/srv/Trigger {}

ros2 topic echo /odom
ros2 topic echo /robot_status
ros2 param set /robot_sim max_speed 2.0
ros2 action send_goal /navigate_to mini_mission_interfaces/action/NavigateTo "{x: 1.0, y: 0.0}" --feedback
```

## 디렉토리

```
curriculum/                  학습 문서 (step0 ~ step10)
src/
├── mini_mission/            노드 2개
│   ├── src/robot_sim.cpp
│   ├── src/mission_client.cpp
│   ├── include/mini_mission/    토픽·서비스·파라미터·상태 문자열 상수
│   └── launch/mission.launch.py
└── mini_mission_interfaces/ NavigateTo.action 정의만. 노드 코드 없음
build/ install/ log/         colcon 산출물 (git 제외)
```

인터페이스를 별도 패키지로 분리한 이유는 계약과 구현을 나누기 위해서다. 이 액션을 쓰려는 쪽이
노드 구현 전체에 의존할 필요가 없다.

## 커리큘럼

한 스텝씩 진행하고, 각 문서의 "검증"이 통과해야 다음으로 넘어간다. 각 문서 하단에 정답 코드가
접힌 채로 들어 있다 — 직접 구현한 뒤에 펼쳐서 대조한다.

[curriculum/README.md](curriculum/README.md) 에 전체 목차와 설계 근거가 있다.

| 스텝 | 내용 |
|---|---|
| [0](curriculum/step0-setup.md) | colcon 워크스페이스, 패키지 생성, 첫 빌드 |
| [1](curriculum/step1-node-and-topic.md) | Node, publisher, 타이머 |
| [2](curriculum/step2-subscriber-and-qos.md) | subscription, SensorDataQoS |
| [3](curriculum/step3-parameters.md) | 파라미터 선언과 검증 콜백 |
| [4](curriculum/step4-qos-deep-dive.md) | QoS 호환성, TRANSIENT_LOCAL |
| [5](curriculum/step5-services.md) | 서비스 서버, Trigger |
| [6](curriculum/step6-custom-action-server.md) | .action 정의, 액션 서버 |
| [7](curriculum/step7-action-client.md) | 액션 클라이언트, 미션 실행 |
| [8](curriculum/step8-cancel-and-abort.md) | 취소 처리 |
| [9](curriculum/step9-launch-and-integration.md) | launch 파일, 통합 검증 |
| [10](curriculum/step10-extensions.md) | 확장 과제 |
