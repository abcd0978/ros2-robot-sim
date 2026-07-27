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
colcon build --packages-select mini_mission
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
다음: [Step 10 — 확장 과제](step10-extensions.md)
