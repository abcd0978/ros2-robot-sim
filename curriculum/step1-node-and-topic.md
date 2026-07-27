# Step 1 — 노드와 토픽 발행

> **목표:** robot_sim 노드를 만들어 고정된 위치를 `/odom`에 10Hz로 publish한다.

**선행:** [Step 0](step0-setup.md) 완료
**소요:** 대략 30분

---

## 배우는 것
- `rclcpp::Node` 상속으로 노드 클래스 만들기
- `create_publisher`, `create_wall_timer`
- `rclcpp::init` / `spin` / `shutdown` 라이프사이클
- `RCLCPP_INFO` 로깅
- CMakeLists.txt에 실행파일 등록하기

## 만들 것
`src/mini_mission/src/robot_sim.cpp`를 새로 작성한다. `RobotSim` 클래스는 `rclcpp::Node`를
상속하고, 위치를 나타내는 멤버 `x_`, `y_`, `theta_`를 들고 있다 (이 멤버들은 뒤 스텝에서 계속
쓰인다). 이 단계에서 로봇은 아직 움직이지 않는다 — `(0, 0, 0)` 고정값을 10Hz로 `/odom`에
내보내는 것부터 시작한다.

CMakeLists.txt에 `robot_sim` 실행파일을 추가한다.

## 힌트
클래스 골격:
```cpp
class RobotSim : public rclcpp::Node
```
멤버 변수:
```cpp
double x_{0.0}, y_{0.0}, theta_{0.0};
```

퍼블리셔 생성 (토픽 이름, 큐 사이즈):
```cpp
create_publisher<geometry_msgs::msg::Pose2D>("odom", 10);
```

타이머 생성 (주기 100ms = 10Hz), 콜백 안에서 현재 `x_, y_, theta_`를 `Pose2D`에 담아
publish한다:
```cpp
create_wall_timer(std::chrono::milliseconds(100), 콜백);
```

`main` 함수는 다음 순서로 호출한다: `rclcpp::init(argc, argv)` → `std::make_shared<RobotSim>()`
→ `rclcpp::spin(node)` → `rclcpp::shutdown()`.

로깅 예:
```cpp
RCLCPP_INFO(this->get_logger(), "robot_sim started");
```

CMakeLists.txt에 추가 (보일러플레이트, 전체 인용 가능):
```
add_executable(robot_sim src/robot_sim.cpp)
ament_target_dependencies(robot_sim rclcpp geometry_msgs)
install(TARGETS robot_sim DESTINATION lib/${PROJECT_NAME})
```

## 검증
터미널 A:
```bash
ros2 run mini_mission robot_sim
```
터미널 B:
```bash
ros2 topic list
ros2 topic hz /odom
ros2 topic echo /odom
```
기대 결과: `ros2 topic list`에 `/odom`이 보이고, `ros2 topic hz /odom`이 약 10Hz로 수렴하며,
`ros2 topic echo /odom`은 `x: 0.0 y: 0.0 theta: 0.0`을 반복 출력한다.

## 자주 밟는 지뢰
- `install(TARGETS ...)`를 CMakeLists에서 빠뜨림 → 빌드는 되는데 `ros2 run`이
  "executable not found" 에러
- `create_publisher`에 넘긴 이름 `"odom"`은 노드 네임스페이스 기준 상대 경로다. 지금은
  네임스페이스가 없어 결과적으로 `/odom`과 같지만, 나중에 네임스페이스를 붙이면 달라진다는
  점을 기억해둔다
- `rclcpp::spin(node)`을 호출하지 않으면 타이머 콜백이 아예 돌지 않는다
- 새 터미널을 열 때마다 `source install/setup.bash`가 필요하다

## 과제
- [ ] 생성자에서 `x_, y_, theta_`를 0이 아닌 값으로 초기화하고 echo로 확인
- [ ] 타이머 주기를 200ms로 바꿔 `ros2 topic hz`가 5로 바뀌는지 확인 후 100ms로 복구

---
다음: [Step 2 — 구독자와 QoS 첫 만남](step2-subscriber-and-qos.md)
