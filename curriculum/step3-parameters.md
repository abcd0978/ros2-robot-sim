# Step 3 — 파라미터

> **목표:** robot_sim에 max_speed/pos_tolerance/publish_rate 파라미터를 선언하고, 검증 콜백으로 잘못된 값을 거부한다.

**선행:** [Step 2](step2-subscriber-and-qos.md) 완료
**소요:** 대략 30분

---

## 배우는 것
- `declare_parameter<T>`, `get_parameter().as_T()`
- `add_on_set_parameters_callback`과 `SetParametersResult`
- 파라미터 값 검증을 콜백 안에서 거부하는 패턴
- 타이머는 주기 변경 API가 없어서 재생성이 필요하다는 것
- `ros2 param` CLI와 YAML 파라미터 파일

## 만들 것
`robot_sim`에 `max_speed`(double, 기본 1.0), `pos_tolerance`(double, 기본 0.05),
`publish_rate`(double, 기본 10.0) 세 파라미터를 선언한다. `add_on_set_parameters_callback`으로
검증 콜백을 등록해서 `max_speed <= 0` 또는 `pos_tolerance < 0`이면 거부한다. `publish_rate`가
바뀌면 실제로 발행 주기가 바뀌어야 한다.

`rclcpp::TimerBase`에는 주기를 바꾸는 API가 없다. 기존 타이머를 취소하고 새 주기로 다시
`create_wall_timer`를 호출해서 멤버 타이머 핸들을 교체해야 한다. 이 재생성 로직을
`on_set_parameters_callback` 안에서 처리한다.

## 스켈레톤
**아래 스켈레톤은 그대로 붙여넣으면 컴파일된다. 내부 TODO만 채우면 된다.**

기존 파일에 더할 부분만 발췌했다.

`robot_sim.cpp` 에 **추가**한다.

```cpp
// ── include 추가 ──
#include <vector>
#include "rcl_interfaces/msg/set_parameters_result.hpp"

// ── private: 메서드 추가 ──
  rcl_interfaces::msg::SetParametersResult
  on_param_change(const std::vector<rclcpp::Parameter> & params)
  {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;
    // TODO: params 를 순회하며 get_name() 으로 분기
    // TODO: 검증 통과하면 멤버 변수에 반영, 실패하면
    //       result.successful = false; result.reason = "...";
    return result;
  }

// ── private: 멤버 추가 ──
  double max_speed_{1.0};
  double pos_tolerance_{0.05};
  double publish_rate_{10.0};
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_cb_;

// ── 생성자에 추가 ──
    // TODO: declare_parameter<double>("max_speed", 1.0);  (3개)
    // TODO: param_cb_ = add_on_set_parameters_callback(
    //         std::bind(&RobotSim::on_param_change, this, std::placeholders::_1));
```

`param_cb_` 의 반환 핸들을 멤버로 잡아두지 않으면 콜백이 즉시 해제된다. 반드시 보관해야 한다.

## 힌트
파라미터 선언 (3개, 타입과 기본값 인자):
```cpp
declare_parameter<double>("max_speed", 1.0);
```

값 읽기:
```cpp
get_parameter("max_speed").as_double();
```

검증 콜백 시그니처와 등록:
```cpp
rcl_interfaces::msg::SetParametersResult on_param_change(const std::vector<rclcpp::Parameter> & params);
```
```cpp
add_on_set_parameters_callback(std::bind(&RobotSim::on_param_change, this, std::placeholders::_1));
```

`SetParametersResult`는 `successful`(bool), `reason`(string) 두 필드를 채워서 반환한다. 거부할
땐 `successful = false`로 두고 `reason`에 이유를 담는다.

`publish_rate` 파라미터 파일 예시 (`params.yaml`, 전체 인용 가능):
```yaml
robot_sim:
  ros__parameters:
    max_speed: 2.0
    publish_rate: 5.0
```

파일로 띄우기:
```bash
ros2 run mini_mission robot_sim --ros-args --params-file params.yaml
```

## 검증
```bash
ros2 param set /robot_sim publish_rate 2.0
ros2 topic hz /odom
ros2 param set /robot_sim max_speed -1.0
ros2 param get /robot_sim max_speed
```
기대 결과: `publish_rate`를 2.0으로 바꾸면 `ros2 topic hz /odom`이 약 2로 수렴한다.
`max_speed`를 음수로 set하면 "Setting parameter failed" 계열 거부 메시지가 뜨고, 이어서
`ros2 param get`은 여전히 이전 값(1.0 또는 직전 값)을 보여준다.

## 자주 밟는 지뢰
- `declare_parameter` 없이 `get_parameter`를 호출하면
  `rclcpp::exceptions::ParameterNotDeclaredException` 발생
- 콜백에서 `result.successful = true`만 반환하고 실제 멤버 변수(`max_speed_` 등)를 갱신하지
  않으면, `ros2 param get`은 새 값을 보여주는데 실제 동작은 예전 값 그대로인 불일치가 생긴다
- 정수 리터럴을 double 파라미터에 set하면 타입 에러 — `ros2 param set /robot_sim max_speed
  1.0`처럼 소수점을 명시한다

## 과제
- [ ] `ros2 param describe /robot_sim max_speed`로 타입과 설명 확인
- [ ] `ros2 param dump /robot_sim > params.yaml`로 저장 후 다시 `--params-file`로 로드
- [ ] `pos_tolerance`에 음수를 set해서 거부되는지 확인

---
다음: [Step 4 — QoS 파고들기](step4-qos-deep-dive.md)
