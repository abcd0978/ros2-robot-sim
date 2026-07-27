# Step 8 — 취소 처리

> **목표:** 진행 중인 goal을 클라이언트가 취소하고 서버가 협조적으로 중단하는 전체 경로를 구현한다.

**선행:** [Step 7](step7-action-client.md) 완료
**소요:** 대략 50분

---

## 배우는 것
- 취소가 협조적(cooperative)이라는 것 — 서버가 스스로 중단 시점을 체크해야 한다
- `handle_cancel` / `is_canceling()` / `canceled()`의 관계
- 클라이언트 쪽 `async_cancel_goal`
- Result code 4종의 의미 차이

## 만들 것
robot_sim의 execute 루프에 취소 체크를 넣고, mission_client에 `/abort_mission` 서비스를 추가해 진행 중인 goal을 취소한다. 취소되면 `/robot_status`가 `ABORTED`로 바뀐다.

## 힌트
### 서버 쪽 (robot_sim)
- `handle_cancel`은 대부분의 경우 `CancelResponse::ACCEPT`를 반환하면 된다
- **ACCEPT를 반환하는 것만으로는 아무 일도 일어나지 않는다.** execute 루프 안에서 매 반복마다 `goal_handle->is_canceling()`을 직접 체크해야 한다. true면:
  - Result를 채우고 `goal_handle->canceled(result)`를 호출
  - 루프를 즉시 탈출
  - `/robot_status`를 `ABORTED`로 발행
- `canceled()`를 호출하지 않고 그냥 `return`하면 클라이언트는 결과를 영영 받지 못하고 대기한다

### 클라이언트 쪽 (mission_client)
- 취소하려면 goal handle이 필요하므로, `goal_response_callback`에서 받은 handle을 멤버 변수(예: `current_goal_handle_`)로 저장해둔다
- 취소 호출: `client_->async_cancel_goal(current_goal_handle_)`
- `/abort_mission` 서비스 콜백에서: 현재 goal handle이 있으면 cancel 요청 → `current_index_`를 리셋하고 진행 중 플래그를 내림

### Result code 비교
| code | 누가 결정 | 의미 |
|---|---|---|
| `SUCCEEDED` | 서버 | 정상 완료 |
| `ABORTED` | 서버 | 서버가 스스로 실패라고 판단(예: 장애물, 타임아웃) |
| `CANCELED` | 클라이언트 요청 → 서버가 반영 | 클라이언트가 취소를 요청했고 서버가 협조함 |
| `UNKNOWN` | - | 비정상 상태, 거의 발생하지 않아야 함 |

`abort()`는 서버가 스스로 포기하는 것(예: 장애물에 부딪힘), `canceled()`는 클라이언트가 취소를 요청했을 때 서버가 그에 응하는 것 — 주도권이 다르다.

- 이동 스레드(handle_accepted에서 detach한 스레드)가 로봇 위치 같은 멤버 변수를 건드리는 동안 다른 콜백(예: `/odom` publish 타이머)도 같은 변수를 읽는다. 이 프로젝트 규모에서는 무시해도 괜찮지만, 실무에서는 `std::atomic`이나 mutex로 보호해야 하는 지점이다

## 검증
미션 실행 중:
```bash
ros2 service call /abort_mission std_srvs/srv/Trigger {}
```
기대 결과: `/odom`이 그 자리에서 즉시 멈추고, `/robot_status`가 `ABORTED`로 바뀌며, result code가 `CANCELED`로 찍힌다.

CLI로 직접:
```bash
ros2 action send_goal /navigate_to mini_mission_interfaces/action/NavigateTo "{x: 5.0, y: 5.0}" --feedback
```
전송 중 Ctrl-C를 누르면 CLI가 취소 요청을 보내는지, 서버 로그에서 취소가 반영되는지 확인한다.

## 자주 밟는 지뢰
- `is_canceling()` 체크를 execute 루프에 넣지 않음 — 가장 흔한 실수. ACCEPT는 반환했는데 로봇이 안 멈춘다
- `canceled()`를 호출하지 않고 `return`만 해서 클라이언트가 result_callback을 영영 못 받음
- `current_goal_handle_`을 저장하지 않아서 `/abort_mission`이 취소할 대상이 없음
- 취소 후 `current_index_`를 리셋하지 않으면 다음 `/start_mission`이 중간 지점부터 시작함

## 과제
- [ ] 취소 시 `elapsed_sec`에 취소된 시점까지의 시간이 들어가는지 확인
- [ ] `/abort_mission`을 미션이 없을 때 호출하면 어떤 응답이 오는지 정의하고 구현

---
다음: [Step 9 — launch 파일과 통합](step9-launch-and-integration.md)
