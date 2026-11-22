#include <rclcpp/node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/subscription.hpp>
#include <visualization_msgs/msg/detail/marker__struct.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
 class RosNode: public rclcpp::Node{
     private:
         rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr lidar_pub_;
         rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr lidar_sub_;
         rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr cv_pub_;
         rclcpp::Subscription<visualization_msgs::msg::MarkerArray>::SharedPtr cv_sub_;
     public:
         void publishLidar(void* data);
         void publishFusion(void* data);
         void FusionCallback(void* data);
         void LidarCallback(void* data);
};
