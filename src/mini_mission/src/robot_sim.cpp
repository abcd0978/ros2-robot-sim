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

#include <cmath>
#include <thread>
#include "rclcpp_action/rclcpp_action.hpp"
#include "mini_mission_interfaces/action/navigate_to.hpp"

using NavigateTo = mini_mission_interfaces::action::NavigateTo;
using GoalHandle = rclcpp_action::ServerGoalHandle<NavigateTo>;

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
            std::bind(&RobotSim::on_param_change, this, std::placeholders::_1));

    // 서비스 생성
    reset_srv_ = create_service<std_srvs::srv::Trigger>(
      mini_mission::service::RESET_POSE,
      std::bind(&RobotSim::on_reset, this, std::placeholders::_1, std::placeholders::_2)
    );

    //액션서버 생성
    nav_server_ = rclcpp_action::create_server<NavigateTo>(
      this, "navigate_to",
      std::bind(&RobotSim::handle_goal, this,
        std::placeholders::_1, std::placeholders::_2),
      std::bind(&RobotSim::handle_cancel, this, std::placeholders::_1),
      std::bind(&RobotSim::handle_accepted, this, std::placeholders::_1)
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
    RCLCPP_INFO(get_logger(), res->message.c_str());
  }

  void reset_timer()
  {
    timer_ = create_wall_timer(
    std::chrono::duration<double>(1.0 / publish_rate_),
    std::bind(&RobotSim::on_timer, this));
  }

  void on_timer()
  {
    geometry_msgs::msg::Pose2D msg;
    msg.x = x_;
    msg.y = y_;
    msg.theta = theta_;
    odom_pub_->publish(msg);
  }

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
    // TODO: std::thread{std::bind(&RobotSim::execute, this, goal_handle)}.detach();
    //       여기서 execute 를 직접 부르면 노드 전체가 멈춘다
    (void)goal_handle;
  }

  void execute(const std::shared_ptr<GoalHandle> goal_handle)
  {
    // TODO: 루프
    //   1) 목표까지 남은 거리 계산
    //   2) pos_tolerance_ 이내면 결과 채우고 goal_handle->succeed(result) 후 종료
    //   3) 아니면 max_speed_ * dt 만큼 전진, set_status("MOVING")
    //   4) goal_handle->publish_feedback(feedback)
    //   5) publish_rate_ 주기로 sleep 후 반복
    (void)goal_handle;
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
