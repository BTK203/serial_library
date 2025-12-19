#include "serial_library/serial_library.hpp"

#if defined(USE_LINUX) && USE_ROS

using namespace std::placeholders;

namespace serial_library
{
    
    RosTransceiver::RosTransceiver(const rclcpp::Node::SharedPtr& node, const std::string& ns)
    : ns(ns),
      n(node) { }

    bool RosTransceiver::init()
    {
        rx = n->create_subscription<std_msgs::msg::ByteMultiArray>(
            ns + "/rx", 10, std::bind(&RosTransceiver::rxCb, this, _1));
        
        tx = n->create_publisher<std_msgs::msg::ByteMultiArray>(
            ns + "/tx", 10);

        return true;
    }

    void RosTransceiver::send(const char *data, size_t numData) const
    {
        std_msgs::msg::ByteMultiArray msg;
        msg.data.assign(data, data + numData);
        tx->publish(msg);
    }

    size_t RosTransceiver::recv(char *data, size_t numData) const
    {
        size_t nbytes = latestMsg.size();
        if(nbytes > numData)
        {
            nbytes = numData;
            RCLCPP_WARN(n->get_logger(), "Truncated msg to %zu bytes", numData);
        }

        memcpy(data, latestMsg.data(), nbytes);
        return nbytes;
    }

    void RosTransceiver::deinit(void)
    { }


    void RosTransceiver::rxCb(std_msgs::msg::ByteMultiArray::ConstSharedPtr msg)
    {
        latestMsg.assign(msg->data.begin(), msg->data.end());
    }
}

#endif
