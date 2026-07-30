#include <chrono>   // std::chrono::milliseconds — 타이머 주기를 숫자가 아닌 '타입 붙은 시간'으로 넘기려고
#include <memory>   // std::make_shared / shared_ptr — rclcpp 는 노드·퍼블리셔·타이머를 전부 shared_ptr 로 다룬다
#include <vector>
#include "rcl_interfaces/msg/set_parameters_result.hpp"

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose2_d.hpp"
#include <string>
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"

#include "mini_mission/robot_status.hpp"
#include "mini_mission/topic.hpp"
#include "mini_mission/param.hpp"
#include "mini_mission/service.hpp"
#include "mini_mission/action.hpp"

#include <cmath>
#include <thread>
#include "rclcpp_action/rclcpp_action.hpp"
#include "mini_mission_interfaces/action/navigate_to.hpp"

using NavigateTo = mini_mission_interfaces::action::NavigateTo;
using GoalHandle = rclcpp_action::ServerGoalHandle<NavigateTo>;

constexpr auto & mock_arg_1 = std::placeholders::_1;
constexpr auto & mock_arg_2 = std::placeholders::_2;

class RobotSim : public rclcpp::Node
{
public:
  RobotSim()
  : Node("robot_sim")
  {
    // 퍼블리셔 생성
    odom_pub_ = create_publisher<geometry_msgs::msg::Pose2D>(
      mini_mission::topic::ODOM, 
      rclcpp::QoS(rclcpp::SensorDataQoS())
    );
    status_pub_ = create_publisher<std_msgs::msg::String>(
      mini_mission::topic::ROBOT_STATUS, 
      rclcpp::QoS(1).reliable().transient_local()
    );
    set_status(mini_mission::robot_status::IDLE);

    // 파라미터 선언, 콜백 연결
    declare_parameter<double>(mini_mission::param::MAX_SPEED, 1.0);
    declare_parameter<double>(mini_mission::param::POS_TOLERANCE, 0.05);
    declare_parameter<double>(mini_mission::param::PUBLISH_RATE, 10.0);
    max_speed_     = get_parameter(mini_mission::param::MAX_SPEED).as_double();
    pos_tolerance_ = get_parameter(mini_mission::param::POS_TOLERANCE).as_double();
    publish_rate_  = get_parameter(mini_mission::param::PUBLISH_RATE).as_double();
    reset_timer();
    param_cb_ = add_on_set_parameters_callback(
            std::bind(&RobotSim::on_param_change, this, mock_arg_1));

    // 서비스 생성
    reset_srv_ = create_service<std_srvs::srv::Trigger>(
      mini_mission::service::RESET_POSE,
      std::bind(&RobotSim::on_reset, this, mock_arg_1, mock_arg_2)
    );

    //액션서버 생성
    nav_server_ = rclcpp_action::create_server<NavigateTo>(
      this, 
      mini_mission::action::NAVIGATE_TO,
      std::bind(&RobotSim::handle_goal, this, mock_arg_1, mock_arg_2),
      std::bind(&RobotSim::handle_cancel, this, mock_arg_1),
      std::bind(&RobotSim::handle_accepted, this, mock_arg_1)
    );

    // 로봇 노드 시작
    RCLCPP_INFO(get_logger(), "robot_sim started");
  }

private:
  // status 설정 및 퍼블리시
  void set_status(const std::string & s)
  {
    if (this->status_ != s) {
      this->status_ = s;
      std_msgs::msg::String msg;
      msg.data = s;
      status_pub_->publish(msg);
    }
  }

  //파라미터 변경 콜백
  rcl_interfaces::msg::SetParametersResult
   on_param_change(const std::vector<rclcpp::Parameter> & params)
  {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;
    
    for (const auto & param : params) {
      std::string paramName = param.get_name();
      auto paramType = param.get_type();
      auto paramValue = param.get_value<rclcpp::ParameterValue>();

      // max_speed, pos_tolerance, publish_rate 파라미터 valid check
      if (paramName == mini_mission::param::MAX_SPEED) {
        if (paramType != rclcpp::ParameterType::PARAMETER_DOUBLE) {
          result.successful = false;
          result.reason = "max_speed must be a double";
          RCLCPP_WARN(get_logger(), "max_speed must be a double");
          break;
        } else if (paramValue.get<double>() <= 0.0) {
          result.successful = false;
          result.reason = "max_speed must be positive";
          RCLCPP_WARN(get_logger(), "max_speed must be positive");
          break;
        }
        max_speed_ = param.as_double();
      } else if (paramName == mini_mission::param::POS_TOLERANCE) {
        if (paramType != rclcpp::ParameterType::PARAMETER_DOUBLE) {
          result.successful = false;
          result.reason = "pos_tolerance must be a double";
          RCLCPP_WARN(get_logger(), "pos_tolerance must be a double");
          break;
        } else if (paramValue.get<double>() <= 0.0) {
          result.successful = false;
          result.reason = "pos_tolerance must be positive";
          RCLCPP_WARN(get_logger(), "pos_tolerance must be positive");
          break;
        }
        pos_tolerance_ = param.as_double();
      } else if (paramName == mini_mission::param::PUBLISH_RATE) {
         if (paramType != rclcpp::ParameterType::PARAMETER_DOUBLE) {
          result.successful = false;
          result.reason = "publish_rate must be a double";
          RCLCPP_WARN(get_logger(), "pos_tolerance must be a double");
          break;
        } else if (paramValue.get<double>() <= 0.0) {
          result.successful = false;
          result.reason = "publish_rate must be positive";
          RCLCPP_WARN(get_logger(), "publish_rate must be positive");
          break;
        }
        publish_rate_ = param.as_double();
        reset_timer();
      }
    }
    return result;
  }
  // reset_pose 서비스 콜백
  void on_reset(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
    std::shared_ptr<std_srvs::srv::Trigger::Response> res)
  {
    (void)req;
    const bool already = (x_ == 0.0 && y_ == 0.0 && theta_ == 0.0);
    this->x_ = 0.0;
    this->y_ = 0.0;
    this->theta_ = 0.0;
    res->success = true;
    res->message = "Robot position reset to (0, 0, 0)";
    if (already) {
      res->message += "(already at origin)";
    }
    RCLCPP_INFO(get_logger(), "%s", res->message.c_str());   // 런타임 문자열을 포맷 자리에 넣으면 UB
  }

  // publish_rate_ 변경 시 타이머 재설정
  void reset_timer()
  {
    timer_ = create_wall_timer(
    std::chrono::duration<double>(1.0 / publish_rate_),
    std::bind(&RobotSim::on_timer, this));
  }
  // tick 마다 오도메트리 퍼블리시
  void on_timer()
  {
    geometry_msgs::msg::Pose2D msg;
    msg.x = x_;
    msg.y = y_;
    msg.theta = theta_;
    odom_pub_->publish(msg);
  }

  /**
   * 액션 서버 콜백
   */

  // 
  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const NavigateTo::Goal> goal)
  {
    // TODO: goal 좌표 검증. 거부하려면 GoalResponse::REJECT
    (void)uuid; (void)goal;
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(
    const std::shared_ptr<GoalHandle> goal_handle)
  {
    (void)goal_handle;
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandle> goal_handle)
  {
    std::thread{std::bind(&RobotSim::execute, this, goal_handle)}.detach();
  }

  // 현재 위치에서 목표까지 남은 거리
  double distance_to(double gx, double gy) const { return std::hypot(gx - x_, gy - y_); }

  // 목표 방향으로 step 만큼 전진. 한 스텝에 지나칠 거리면 목표에 스냅(오버슈트 방지)
  void step_toward(double gx, double gy, double step)
  {
    const double dx = gx - x_, dy = gy - y_;
    const double dist = std::hypot(dx, dy);
    if (step >= dist) { x_ = gx; y_ = gy; return; }
    x_ += dx / dist * step;               // dx/dist 가 단위 방향벡터
    y_ += dy / dist * step;
  }

  // Result 객체 생성 — succeed/canceled 양쪽이 같은 필드를 채우므로 여기 모은다(Step 8 에서 재사용)
  std::shared_ptr<NavigateTo::Result> make_result(const rclcpp::Time & start, bool reached)
  {
    auto result = std::make_shared<NavigateTo::Result>();
    result->elapsed_sec = (now() - start).seconds();
    result->reached = reached;
    return result;
  }

  // handle_accepted 가 띄운 별도 스레드에서 돈다. executor 스레드가 아니므로 여기서는 블로킹해도 된다.
  void execute(const std::shared_ptr<GoalHandle> goal_handle)
  {
    const auto start = now();
    const auto goal = goal_handle->get_goal();
    auto feedback = std::make_shared<NavigateTo::Feedback>();

    while (rclcpp::ok()) {
      const double dist = distance_to(goal->x, goal->y);

      // 도착 — 목표에 스냅하고 결과를 통보한 뒤 종료
      if (dist <= pos_tolerance_) {
        x_ = goal->x;
        y_ = goal->y;
        set_status(mini_mission::robot_status::REACHED);
        auto result = make_result(start, true);
        goal_handle->succeed(result);      // 이걸 안 부르면 클라이언트가 결과를 영영 못 받는다
        RCLCPP_INFO(get_logger(), "goal reached in %.2fs", result->elapsed_sec);
        return;
      }

      set_status(mini_mission::robot_status::MOVING);

      // dt·step 은 루프 안에서 계산한다 — 실행 중 ros2 param set 이 반영되어야 하므로
      const double dt = 1.0 / publish_rate_;
      step_toward(goal->x, goal->y, max_speed_ * dt);

      feedback->distance_remaining = dist;
      goal_handle->publish_feedback(feedback);

      std::this_thread::sleep_for(std::chrono::duration<double>(dt));
    }
    // rclcpp::ok() 가 false — 종료 중이므로 그냥 빠져나온다
  }


  // 로봇의 위치 상태. 
  double x_{0.0}, y_{0.0}, theta_{0.0};

  // 로봇의 상태
  std::string status_{mini_mission::robot_status::NONE};

  //파라미터
  double max_speed_{1.0};
  double pos_tolerance_{0.05};
  double publish_rate_{10.0};
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_cb_;

  //퍼블리셔
  rclcpp::Publisher<geometry_msgs::msg::Pose2D>::SharedPtr odom_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;

  //타이머
  rclcpp::TimerBase::SharedPtr timer_;

  //서비스
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_srv_;

  // 액션 서버
  rclcpp_action::Server<NavigateTo>::SharedPtr nav_server_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  // 기본은 SingleThreadedExecutor 라 콜백이 한 스레드에서 하나씩 순차 처리된다. Step 6 에서 액션 실행을
  // 별도 스레드로 넘겨야 하는 이유가 이것 — 안 그러면 이동 루프가 executor 를 붙잡아 전부 멈춘다.
  rclcpp::spin(std::make_shared<RobotSim>());

  rclcpp::shutdown();
  return 0;
}
