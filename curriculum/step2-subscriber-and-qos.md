# Step 2 — 구독자와 QoS 첫 만남

> **목표:** mission_client 노드를 만들어 `/odom`을 구독하고, 양쪽 QoS를 SensorDataQoS로 맞춘다.

**선행:** [Step 1](step1-node-and-topic.md) 완료
**소요:** 대략 25분

---

## 배우는 것
- `create_subscription` 콜백 등록 (람다 또는 `std::bind` + `std::placeholders::_1`)
- QoS를 숫자로 넘길 때의 실체: `10`은 사실 `KEEP_LAST(10) + RELIABLE`
- `rclcpp::SensorDataQoS()` = `BEST_EFFORT + KEEP_LAST(5)`
- publisher와 subscriber의 QoS가 맞아야 연결된다는 첫 감

## 만들 것
`src/mini_mission/src/mission_client.cpp`를 새로 생성한다. 지금은 `/odom`을 구독해서 콜백에서
로그만 찍는 껍데기 노드다. 그리고 최종 설계에 맞춰 `robot_sim`의 `/odom` publisher QoS를
`10`에서 `rclcpp::SensorDataQoS()`로 바꾸고, `mission_client`의 subscription도 같은 QoS로
맞춘다.

CMakeLists.txt에 `mission_client` 실행파일을 추가한다.

## 힌트
구독 생성 (토픽 이름, QoS, 콜백):
```cpp
create_subscription<geometry_msgs::msg::Pose2D>("odom", rclcpp::SensorDataQoS(), 콜백);
```

콜백 시그니처:
```cpp
void odom_callback(const geometry_msgs::msg::Pose2D::SharedPtr msg);
```

람다로 등록할 경우 `this`를 캡처해야 멤버 함수/로거에 접근 가능:
```cpp
[this](const geometry_msgs::msg::Pose2D::SharedPtr msg) { /* ... */ }
```

Step 1에서 만든 `robot_sim`의 publisher 선언도 QoS 인자만 `10` → `rclcpp::SensorDataQoS()`로
교체한다.

CMakeLists.txt 추가분 (전체 인용 가능):
```
add_executable(mission_client src/mission_client.cpp)
ament_target_dependencies(mission_client rclcpp geometry_msgs)
install(TARGETS mission_client DESTINATION lib/${PROJECT_NAME})
```

## 검증
터미널 A:
```bash
ros2 run mini_mission robot_sim
```
터미널 B:
```bash
ros2 run mini_mission mission_client
```
터미널 C:
```bash
ros2 topic info /odom -v
```
기대 결과: 터미널 B에 odom 콜백 로그가 약 10Hz로 찍힌다. 터미널 C에서 Publisher count 1,
Subscription count 1이 보이고, QoS profile에 `RELIABILITY: BEST_EFFORT`,
`DURABILITY: VOLATILE`, `HISTORY (Depth): KEEP_LAST (5)`가 양쪽 모두 표시된다.

## 자주 밟는 지뢰
- 람다에서 `this`를 캡처하지 않으면 멤버 변수/로거 접근 시 컴파일 에러
- `SharedPtr`과 `ConstSharedPtr` 혼동 — 콜백 인자 타입을 정확히 맞추지 않으면 컴파일 에러
- 새 executable을 CMakeLists.txt에 추가하는 것을 잊으면 `ros2 run mini_mission
  mission_client`가 실행파일을 못 찾음
- publisher만 SensorDataQoS로 바꾸고 subscriber는 그대로 두면(또는 반대) QoS 요건이 서로
  달라 재빌드 후 이상 동작 — Step 4에서 왜 그런지 자세히 다룬다

## 과제
- [ ] 콜백 안에서 `RCLCPP_INFO`로 x, y, theta를 출력
- [ ] `ros2 topic info /odom -v` 출력의 QoS profile 필드를 하나씩 읽어보기

---
다음: [Step 3 — 파라미터](step3-parameters.md)
