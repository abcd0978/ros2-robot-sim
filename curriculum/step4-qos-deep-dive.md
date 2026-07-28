# Step 4 — QoS 파고들기

> **목표:** QoS 호환성 규칙을 실험으로 직접 확인하고, `/robot_status`를 TRANSIENT_LOCAL로 추가한다.

**선행:** [Step 3](step3-parameters.md) 완료
**소요:** 대략 35분

이 프로젝트에서 가장 중요한 스텝이다. QoS 불일치는 컴파일 에러도, 런타임 에러도 없이 그냥
"아무 일도 안 일어나는" 형태로 나타난다. 그 감각을 여기서 만들어둔다.

---

## 배우는 것
- QoS 5축: Reliability, Durability, History+Depth, Deadline, Liveliness (뒤 둘은 이름만 언급)
- 호환성 규칙: publisher가 BEST_EFFORT면 subscriber가 RELIABLE일 때 매칭 자체가 안 된다
  (반대 방향, 즉 publisher RELIABLE + subscriber BEST_EFFORT는 매칭됨). Durability도 같은
  방향의 규칙: publisher가 VOLATILE인데 subscriber가 TRANSIENT_LOCAL을 요구하면 매칭 안 됨
- TRANSIENT_LOCAL = 늦게 붙은 subscriber도 과거 발행값을 받는다 ("latched" 토픽)
- QoS 불일치는 에러 로그를 남기지 않고 조용히 실패한다

## 만들 것
`robot_sim`에 `/robot_status` publisher를 추가한다. 타입은 `std_msgs/msg/String`, QoS는
`rclcpp::QoS(1).reliable().transient_local()`. 상태 문자열은 `IDLE`/`MOVING`/`REACHED`/
`ABORTED` 중 하나가 될 예정이지만, 지금은 생성자에서 `IDLE`을 한 번만 발행한다 (주기 발행이
아니다 — 상태가 바뀔 때만 발행하는 설계이고, 그래서 TRANSIENT_LOCAL이 필요하다: 값을 안
바꾸는데 계속 쏘면 굳이 latched로 만들 이유가 없다).

## 스켈레톤
**아래 스켈레톤은 그대로 붙여넣으면 컴파일된다. 내부 TODO만 채우면 된다.**

기존 파일에 더할 부분만 발췌했다.

`robot_sim.cpp` 에 추가:

```cpp
// ── include 추가 ──
#include <string>
#include "std_msgs/msg/string.hpp"

// ── private: 메서드 추가 ──
  void set_status(const std::string & s)
  {
    // TODO: status_ 와 값이 다를 때만 status_ 갱신 후 status_pub_ 로 발행
  }

// ── private: 멤버 추가 ──
  std::string status_{"IDLE"};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;

// ── 생성자에 추가 ──
    // TODO: status_pub_ = create_publisher<std_msgs::msg::String>(
    //         "robot_status", rclcpp::QoS(1).reliable().transient_local());
    // TODO: set_status("IDLE");
```

`mission_client.cpp` 에 추가:

```cpp
// ── include 추가 ──
#include "std_msgs/msg/string.hpp"

// ── private: 추가 ──
  void on_status(const std_msgs::msg::String::SharedPtr msg)
  {
    // TODO: 상태 변화를 RCLCPP_INFO 로 출력
    (void)msg;
  }

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub_;

// ── 생성자에 추가 ──
    // TODO: status_sub_ 생성. QoS는 퍼블리셔와 동일하게
    //       rclcpp::QoS(1).reliable().transient_local()
```

## 힌트
퍼블리셔 생성 (토픽 이름, QoS):
```cpp
create_publisher<std_msgs::msg::String>("robot_status", rclcpp::QoS(1).reliable().transient_local());
```

메시지 채우기:
```cpp
std_msgs::msg::String status_msg;
status_msg.data = "IDLE";
```

발행은 생성자 안에서 한 번만 호출한다 (타이머 콜백이 아니다).

## 검증

**실험 1 — 늦은 구독자가 과거 값을 받는가**

터미널 A:
```bash
ros2 run mini_mission robot_sim
```
30초 이상 기다린 후 터미널 B:
```bash
ros2 topic echo /robot_status
```
기대 결과: 구독을 시작하자마자 즉시 `data: IDLE`이 뜬다 — 30초 전에 발행된 값인데도 받는다.

같은 터미널 B에서:
```bash
ros2 topic echo /odom
```
기대 결과: `/odom`은 과거 값 재생이 없다. 다음 발행 주기(10Hz라 최대 100ms 대기)까지 기다려야
값이 뜨기 시작한다.

**실험 2 — 일부러 QoS를 깨보기**

터미널 B:
```bash
ros2 topic echo /odom --qos-reliability reliable
```
기대 결과: 아무것도 안 뜬다. echo가 RELIABLE을 요구하는데 `/odom` publisher는 BEST_EFFORT라
매칭이 안 된다.

확인:
```bash
ros2 topic info /odom -v
```
QoS profile에서 publisher의 RELIABILITY가 BEST_EFFORT인지 확인한다.

그다음 `mission_client`의 `/odom` subscription QoS를 `rclcpp::QoS(10).reliable()`로 바꿔서
재빌드하고 실행 → odom 콜백이 전혀 안 불리는 것을 확인한다. 확인했으면 `rclcpp::SensorDataQoS()`로
되돌린다.

## QoS 선택 가이드

| 용도 | Reliability | Durability | 이 프로젝트의 예 |
|---|---|---|---|
| 센서 스트림 (고주기) | BEST_EFFORT | VOLATILE | `/odom` |
| 명령/요청 | RELIABLE | VOLATILE | 서비스 호출 (내부적으로 RELIABLE) |
| 상태/설정값 (저주기, 반드시 도달) | RELIABLE | VOLATILE 또는 TRANSIENT_LOCAL | `/robot_status` |
| latched 토픽 (지도, 스냅샷) | RELIABLE | TRANSIENT_LOCAL | `/robot_status` |

## 자주 밟는 지뢰
- `ros2 topic echo`가 조용한데 에러도 안 남 → 99% QoS 불일치다. `ros2 topic info -v`로
  양쪽 QoS profile을 먼저 비교하는 습관을 들인다
- TRANSIENT_LOCAL은 depth만큼만 보관한다. 여기선 depth 1이라 가장 최근 값 하나만 받는다
- TRANSIENT_LOCAL publisher는 마지막 메시지를 계속 리소스에 들고 있어야 한다. 그래서 고주기
  토픽(`/odom`)에는 붙이지 않는다 — 저주기 상태값에만 쓴다

## 과제
- [ ] 실험 1, 2를 재현하고 결과를 기록해둔다
- [ ] `mission_client`의 `/odom` subscription QoS를 `SensorDataQoS()`로 되돌렸는지
      `ros2 topic info -v`로 재확인
- [ ] `/robot_status`를 echo로 켜놓은 상태에서 `robot_sim`을 재시작해보고, 새 프로세스가 뜨는
      즉시 새 `IDLE` 값이 오는지 관찰 (TRANSIENT_LOCAL은 publisher 프로세스가 살아있는 동안만
      과거 값을 보관한다는 것을 확인)

---

## 정답 코드

<details>
<summary>펼쳐서 보기 — 직접 구현한 뒤에 확인할 것</summary>

**`robot_sim.cpp` 추가분**
```cpp
// ── include 추가 ──
#include <string>
#include "std_msgs/msg/string.hpp"

// ── private: 메서드 추가 ──
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

// ── private: 멤버 추가 ──
  std::string status_{""};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;

// ── 생성자에 추가 ──
    status_pub_ = create_publisher<std_msgs::msg::String>(
      "robot_status", rclcpp::QoS(1).reliable().transient_local());

    set_status("IDLE");
```

**`mission_client.cpp` 추가분**
```cpp
// ── include 추가 ──
#include "std_msgs/msg/string.hpp"

// ── private: 추가 ──
  void on_status(const std_msgs::msg::String::SharedPtr msg)
  {
    RCLCPP_INFO(get_logger(), "status: %s", msg->data.c_str());
  }

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub_;

// ── 생성자에 추가 ──
    status_sub_ = create_subscription<std_msgs::msg::String>(
      "robot_status", rclcpp::QoS(1).reliable().transient_local(),
      std::bind(&MissionClient::on_status, this, std::placeholders::_1));
```

> **스켈레톤과 다른 점 하나.** 스켈레톤에는 `std::string status_{"IDLE"};` 로 되어 있지만 정답은 `""` 로 초기화한다. `"IDLE"` 로 두면 생성자의 `set_status("IDLE")` 이 "값이 안 바뀌었다"고 판단해 발행을 건너뛴다. 그러면 `/robot_status` 에 아무것도 안 실려서 TRANSIENT_LOCAL 실험(실험 1)이 조용히 실패한다. 빈 문자열로 시작해야 첫 전이가 실제 발행으로 이어진다.

</details>

---
다음: [Step 5 — 서비스](step5-services.md)
