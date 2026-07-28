// robot_sim — 가상 2D 로봇 본체. Step 1 시점: 아직 움직이지 않고 고정 위치만 10Hz 로 발행한다.
//
// 이 파일의 설계 축 하나: "상태는 멤버가 들고, 타이머는 현재 상태를 내보내기만 한다."
// 그래서 Step 6 에서 액션 서버가 x_, y_ 를 실제로 움직이기 시작해도 on_timer 는 한 줄도 안 고친다.

#include <chrono>   // std::chrono::milliseconds — 타이머 주기를 숫자가 아닌 '타입 붙은 시간'으로 넘기려고
#include <memory>   // std::make_shared / shared_ptr — rclcpp 는 노드·퍼블리셔·타이머를 전부 shared_ptr 로 다룬다

#include "rclcpp/rclcpp.hpp"
// 메시지 타입은 Pose2D(CamelCase)인데 헤더는 pose2_d.hpp(snake_case)다. rosidl 이 대문자 앞에서
// 끊어 변환하기 때문에 'pose2d' 가 아니라 'pose2_d' 가 된다. 여기서 자주 막힌다.
#include "geometry_msgs/msg/pose2_d.hpp"

// rclcpp::Node 를 상속하는 게 ROS2 C++ 의 표준 패턴이다. 상속하면 create_publisher / get_logger 를
// this-> 없이 바로 쓸 수 있고, 나중에 rclcpp_components 로 컴포넌트화할 때도 이 형태를 요구한다.
class RobotSim : public rclcpp::Node
{
public:
  RobotSim()
  : Node("robot_sim")   // 부모 생성자 호출은 반드시 초기화 리스트에서. 이 문자열이 곧 `ros2 node list` 의 /robot_sim
  {
    // "odom" 은 앞에 / 가 없는 상대 경로다. 지금은 네임스페이스가 없어 결과적으로 /odom 이지만,
    //   --ros-args -r __ns:=/robot1  을 붙이면 /robot1/odom 으로 따라 바뀐다.
    //   "/odom" 이라고 절대 경로로 쓰면 네임스페이스를 무시해서 리매핑이 안 먹는다.
    // 두 번째 인자 10 은 큐 사이즈가 아니라 QoS 다. rclcpp::QoS(10) 으로 암묵 변환되며
    //   = KEEP_LAST(10) + RELIABLE + VOLATILE. Step 2 에서 SensorDataQoS() 로 바꾼다.
    // 반환 핸들을 멤버에 저장하는 게 필수. 지역 변수로 받으면 생성자 끝에서 참조 카운트가 0이 되어
    //   퍼블리셔가 소멸하고, 에러 없이 조용히 아무것도 발행되지 않는다.
    odom_pub_ = create_publisher<geometry_msgs::msg::Pose2D>("odom", rclcpp::QoS(rclcpp::SensorDataQoS()));
    // wall timer = 벽시계(실제 경과) 시간 기준. ROS 시뮬레이션 시간(/clock, use_sim_time)을 따르지 않는다.
    // std::bind(&RobotSim::on_timer, this) — 멤버 함수는 '어느 객체의' 것인지가 필요해서 그냥 넘길 수 없다.
    //   this 를 묶어 인자 없는 호출 가능 객체로 만든다. [this]() { on_timer(); } 람다도 완전히 동일하다.
    // 여기서도 timer_ 에 저장이 필수 — 안 하면 타이머가 즉시 소멸해 콜백이 한 번도 안 불린다.
    timer_ = create_wall_timer(
      std::chrono::milliseconds(100),   // 100ms = 10Hz
      std::bind(&RobotSim::on_timer, this));

    // 함수가 아니라 매크로다. 로그 레벨이 꺼져 있으면 인자 계산 자체를 건너뛰기 위해서.
    // get_logger() 는 노드 이름이 붙은 로거 → [INFO] [robot_sim]: ... 로 찍히고 /rosout 으로도 나간다.
    RCLCPP_INFO(get_logger(), "robot_sim started");
  }

private:
  void on_timer()
  {
    // ROS2 메시지는 평범한 C++ 구조체다. 스택에 만들고 필드는 기본값(0.0)으로 초기화된다.
    geometry_msgs::msg::Pose2D msg;
    msg.x = x_;
    msg.y = y_;
    msg.theta = theta_;
    // publish 는 논블로킹이고 구독자가 하나도 없어도 그냥 성공한다.
    // 그래서 "발행은 되는데 아무도 못 받는" QoS 불일치가 조용히 넘어간다 — Step 4 의 주제.
    odom_pub_->publish(msg);
  }

  // 로봇의 위치 상태. 지금은 아무도 안 건드려서 계속 0 이고, Step 6 의 액션 서버가 실제로 움직인다.
  // 밑줄 접미사는 private 멤버라는 관례 — execute() 에서 지역변수 x 와 멤버 x_ 를 안 헷갈리게 해준다.
  double x_{0.0}, y_{0.0}, theta_{0.0};

  // Publisher<T>::SharedPtr 은 std::shared_ptr<rclcpp::Publisher<T>> 의 별칭. rclcpp 대부분의 클래스가 갖고 있다.
  rclcpp::Publisher<geometry_msgs::msg::Pose2D>::SharedPtr odom_pub_;
  // Timer 가 아니라 TimerBase 다. create_wall_timer 의 실제 반환 타입은 콜백 타입에 따라 달라지는
  // 템플릿이라, 콜백 타입에 무관한 기반 클래스로 받아야 타입이 맞는다.
  //
  // 선언 순서 주의: C++ 멤버는 선언 순서대로 초기화되고 역순으로 소멸한다. timer_ 가 뒤에 있으니
  // 소멸은 timer_ → odom_pub_ 순 → 타이머가 먼저 죽어서 콜백이 이미 소멸한 퍼블리셔를 건드릴 일이 없다.
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  // 어떤 노드보다 먼저 호출한다. argc/argv 를 넘기는 이유는 --ros-args -p max_speed:=2.0 같은
  // ROS 인자를 여기서 파싱하기 때문이다.
  rclcpp::init(argc, argv);

  // make_shared 여야 한다. rclcpp 내부가 shared_from_this() 를 쓰기 때문에,
  //   RobotSim node;  처럼 스택에 만들면 런타임에 bad_weak_ptr 예외가 난다.
  //
  // spin() 은 여기서 블로킹하며 이벤트 루프를 돈다(타이머·구독·서비스 콜백 처리). Ctrl-C 에 리턴한다.
  // 이걸 안 부르면 노드는 생기지만 콜백이 하나도 안 돈다 — node list 엔 뜨는데 토픽은 안 나가는 상태.
  //
  // 기본은 SingleThreadedExecutor 라 콜백이 한 스레드에서 하나씩 순차 처리된다. Step 6 에서 액션 실행을
  // 별도 스레드로 넘겨야 하는 이유가 이것 — 안 그러면 이동 루프가 executor 를 붙잡아 전부 멈춘다.
  rclcpp::spin(std::make_shared<RobotSim>());

  rclcpp::shutdown();
  return 0;
}
