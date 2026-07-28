#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose2_d.hpp"

class MissionClient : public rclcpp::Node
{
public:
  MissionClient()
  : Node("mission_client")
  {
    // TODO: odom_sub_ 생성 (create_subscription)
    odom_sub_ = create_subscription<geometry_msgs::msg::Pose2D>(
      "odom", rclcpp::QoS(rclcpp::SensorDataQoS()), 
      std::bind(&MissionClient::on_odom, this, std::placeholders::_1)
    );
  }

private:
  void on_odom(const geometry_msgs::msg::Pose2D::SharedPtr msg)
  {
    RCLCPP_INFO(get_logger(), "Received odom: x=%f, y=%f, theta=%f", msg->x, msg->y, msg->theta);
    (void)msg;
  }

  rclcpp::Subscription<geometry_msgs::msg::Pose2D>::SharedPtr odom_sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MissionClient>());
  rclcpp::shutdown();
  return 0;
}