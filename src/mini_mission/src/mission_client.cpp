#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose2_d.hpp"
#include "std_msgs/msg/string.hpp"

#include "mini_mission/robot_status.hpp"
#include "mini_mission/topic.hpp"

constexpr auto & mock_arg_1 = std::placeholders::_1;
constexpr auto & mock_arg_2 = std::placeholders::_2;

class MissionClient : public rclcpp::Node
{
public:
  MissionClient()
  : Node("mission_client")
  {
    // subscription 생성
    odom_sub_ = create_subscription<geometry_msgs::msg::Pose2D>(
      mini_mission::topic::ODOM, 
      rclcpp::QoS(rclcpp::SensorDataQoS()), 
      std::bind(&MissionClient::on_odom, this, mock_arg_1)
    );
    status_sub_ = create_subscription<std_msgs::msg::String>(
      mini_mission::topic::ROBOT_STATUS, 
      rclcpp::QoS(1).reliable().transient_local(),
      std::bind(&MissionClient::on_status, this, mock_arg_1)
    );
  }

private:
  // 오도메트리 수신객체 & 콜백
  rclcpp::Subscription<geometry_msgs::msg::Pose2D>::SharedPtr odom_sub_;
  void on_odom(const geometry_msgs::msg::Pose2D::SharedPtr msg)
  {
    RCLCPP_INFO(get_logger(), "Received odom: x=%f, y=%f, theta=%f", msg->x, msg->y, msg->theta);
    (void)msg;
  }

  // status 수신객체 & 콜백
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub_;
  void on_status(const std_msgs::msg::String::SharedPtr msg)
  {
    (void)msg;
    RCLCPP_INFO(get_logger(), "Received status: %s", msg->data.c_str());
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MissionClient>());
  rclcpp::shutdown();
  return 0;
}