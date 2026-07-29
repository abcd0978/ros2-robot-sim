#include <chrono>   // std::chrono::milliseconds — 타이머 주기를 숫자가 아닌 '타입 붙은 시간'으로 넘기려고
#include <memory>   // std::make_shared / shared_ptr — rclcpp 는 노드·퍼블리셔·타이머를 전부 shared_ptr 로 다룬다
#include <vector>
#include "rcl_interfaces/msg/set_parameters_result.hpp"

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose2_d.hpp"

class RobotSim : public rclcpp::Node
{
public:
  RobotSim()
  : Node("robot_sim")
  {
    odom_pub_ = create_publisher<geometry_msgs::msg::Pose2D>("odom", rclcpp::QoS(rclcpp::SensorDataQoS()));
    reset_timer();

    // 파라미터 설정, 콜백 연결
    declare_parameter<double>("max_speed", 1.0);
    declare_parameter<double>("pos_tolerance", 0.05);
    declare_parameter<double>("publish_rate", 10.0);
    // 각 파라미터에 대해 콜백을 등록하는게 아니라, 노드 전체에 대해 콜백을 등록하고, 콜백 안에서 파라미터 이름으로 분기.
    param_cb_ = add_on_set_parameters_callback(
            std::bind(&RobotSim::on_param_change, this, std::placeholders::_1));
      
    RCLCPP_INFO(get_logger(), "robot_sim started");
  }

private:
  rcl_interfaces::msg::SetParametersResult
   on_param_change(const std::vector<rclcpp::Parameter> & params)
  {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;
    
    for (const auto & param : params) {
      std::string paramName = param.get_name();
      auto paramType = param.get_type();
      auto paramValue = param.get_value<rclcpp::ParameterValue>();
      if (paramName == "max_speed") {
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
      } else if (paramName == "pos_tolerance") {
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
      } else if (paramName == "publish_rate") {
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

  void reset_timer()
  {
    timer_ = create_wall_timer(
    std::chrono::duration<double>(1.0 / publish_rate_),   // Hz → 초. 2.0Hz면 0.5초
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

  // 로봇의 위치 상태. 
  double x_{0.0}, y_{0.0}, theta_{0.0};
  //파라미터
  double max_speed_{1.0};
  double pos_tolerance_{0.05};
  double publish_rate_{10.0};
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_cb_;

  rclcpp::Publisher<geometry_msgs::msg::Pose2D>::SharedPtr odom_pub_;
  
  rclcpp::TimerBase::SharedPtr timer_;
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
