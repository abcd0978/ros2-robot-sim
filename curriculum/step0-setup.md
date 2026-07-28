# Step 0 — 환경 준비와 첫 빌드

> **목표:** 컨테이너 환경을 소싱하고, mini_mission 패키지를 만들어 첫 colcon build를 성공시킨다.

**선행:** 없음 (도커 컨테이너 `superpx4_ros2` 접속 가능한 상태)
**소요:** 대략 15분

---

## 배우는 것
- 컨테이너 접속과 ROS2 환경 소싱 (`setup.bash`)
- colcon 워크스페이스 구조 (`src/`, `build/`, `install/`, `log/`)
- `ros2 pkg create`로 패키지 스캐폴딩 생성
- `colcon build --symlink-install`의 의미
- `.gitignore`로 빌드 산출물 제외하기

## 만들 것
`/root/ros2-robot-sim`은 이미 워크스페이스 루트로 clone되어 있다. 여기에 `src/mini_mission`
패키지를 스캐폴딩만 생성하고(노드 코드는 아직 없음), 첫 빌드를 성공시킨다. 이 스텝이 끝나면
`colcon build`가 통과하고 `ros2 pkg prefix mini_mission`이 정상 경로를 출력하는 상태가 된다.

## 힌트
컨테이너 접속 및 소싱:
```bash
docker exec -it superpx4_ros2 bash
source /opt/ros/humble/setup.bash
```

매 터미널마다 반복하기 귀찮으면 `~/.bashrc` 맨 아래에 추가:
```bash
echo 'source /opt/ros/humble/setup.bash' >> ~/.bashrc
```

워크스페이스로 이동 후 패키지 생성:
```bash
cd /root/ros2-robot-sim
mkdir -p src
cd src
ros2 pkg create --build-type ament_cmake --license Apache-2.0 \
  --dependencies rclcpp geometry_msgs std_msgs std_srvs -- mini_mission
cd ..
```

`--dependencies`가 값을 여러 개 받기 때문에 패키지 이름 앞에 `--`가 필요하다. 빼면 `mini_mission`이
의존성 목록으로 빨려 들어간다. `--license`를 빼면 라이선스 목록을 나열하는 경고가 뜬다.

`.gitignore` (레포 루트):
```
build/
install/
log/
```

첫 빌드:
```bash
colcon build --symlink-install
source install/setup.bash
```

`--symlink-install`은 설치 산출물을 복사하지 않고 심볼릭 링크로 연결한다. 파이썬 스크립트나
launch 파일, config 파일을 고칠 때 재빌드 없이 바로 반영되어 개발 사이클이 빨라진다. C++
실행파일 자체는 어차피 재빌드해야 하지만, 이 워크스페이스 습관은 처음부터 들이는 게 낫다.

## 검증
```bash
colcon build --symlink-install
source install/setup.bash
ros2 pkg prefix mini_mission
```
기대 결과: `colcon build` 로그 끝에 `Summary: 1 package finished`가 뜨고, `ros2 pkg prefix
mini_mission`이 `/root/ros2-robot-sim/install/mini_mission` 경로를 출력한다.

## 자주 밟는 지뢰
- `ros2: command not found` → `source /opt/ros/humble/setup.bash`를 안 했거나, 새 터미널을
  열고 다시 소싱하지 않음
- `src/` 안이나 워크스페이스 밖에서 `colcon build` 실행 → 에러 없이 조용히 아무것도 안 빌드됨.
  `src`와 나란히 있는 워크스페이스 루트(`/root/ros2-robot-sim`)에서 실행해야 함
- 빌드는 성공했는데 `ros2 run`이 옛날 상태로 동작 → `install/setup.bash` 재소싱 안 함

## 과제
- [ ] `~/.bashrc`에 source 줄을 추가하고 새 터미널을 열어 자동 적용되는지 확인
- [ ] `.gitignore` 작성
- [ ] `src/mini_mission/package.xml`이 생성되어 있는지 확인

---

## 정답 코드

<details>
<summary>펼쳐서 보기 — 직접 구현한 뒤에 확인할 것</summary>

코드가 아니라 명령 순서가 정답이다.

```bash
docker exec -it superpx4_ros2 bash
echo 'source /opt/ros/humble/setup.bash' >> ~/.bashrc
source /opt/ros/humble/setup.bash

cd /root/ros2-robot-sim
mkdir -p src
cd src
ros2 pkg create --build-type ament_cmake --license Apache-2.0 \
  --dependencies rclcpp geometry_msgs std_msgs std_srvs -- mini_mission
cd ..

printf 'build/\ninstall/\nlog/\n' > .gitignore

colcon build --symlink-install
source install/setup.bash
ros2 pkg prefix mini_mission
```

</details>

---
다음: [Step 1 — 노드와 토픽 발행](step1-node-and-topic.md)
