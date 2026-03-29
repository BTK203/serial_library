#include "serial_library/serial_library.hpp"

#if defined(USE_LINUX) && defined(USE_ROS)

using namespace std::placeholders;
using namespace std::chrono_literals;

namespace serial_library
{
    
    RosTransceiver::RosTransceiver(const rclcpp::Node::SharedPtr& node, const std::string& ns, size_t maxQSz, bool isBridge)
    : ns(ns),
      maxQueueSize(maxQSz),
      isBridge(isBridge),
      n(node) { }

    bool RosTransceiver::init()
    {
        rx = n->create_subscription<std_msgs::msg::ByteMultiArray>(
            ns + (isBridge ? "/tx" : "/rx"), 10, std::bind(&RosTransceiver::rxCb, this, _1));
        
        tx = n->create_publisher<std_msgs::msg::ByteMultiArray>(
            ns + (isBridge ? "/rx" : "/tx"), 10);

        return true;
    }

    void RosTransceiver::send(const char *data, size_t numData)
    {
        std_msgs::msg::ByteMultiArray msg;
        msg.data.assign(data, data + numData);
        tx->publish(msg);
    }

    size_t RosTransceiver::recv(char *data, size_t numData)
    {
        std::vector<char> latestMsg = {};
        while(msgQ.size() > 0)
        {
            std::vector<char> msg = msgQ.front();
            latestMsg.insert(latestMsg.end(), msg.begin(), msg.end());
            msgQ.pop_front();
        }

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
        std::vector<char> buf(msg->data.begin(), msg->data.end());
        if(msgQ.size() > maxQueueSize)
        {
            msgQ.pop_front();
            RCLCPP_WARN_THROTTLE(n->get_logger(), *n->get_clock(), 1000, 
                "Discarded message because queue length of %zu was exceeded", msgQ.size());
        }

        msgQ.push_back(buf);
    }
}

#endif
