#include "serial_library/testing.hpp"

int main(int argc, char **argv)
{
    #if defined(USE_ROS)
    rclcpp::init(argc, argv);
    #endif

    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    #if defined(USE_ROS)
    rclcpp::shutdown();
    #endif

    return result;
}