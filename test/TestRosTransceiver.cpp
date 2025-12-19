#if defined(USE_ROS) // use_ros defined by cmake
#include <rclcpp/rclcpp.hpp>
#include "serial_library/serial_library.hpp"
#include "serial_library/testing.hpp"

using namespace std::placeholders;

class RosTransceiverTest : public ::testing::Test
{
    public:
    void cb(std_msgs::msg::ByteMultiArray::ConstSharedPtr msg)
    {
        lastRecvd.assign(msg->data.begin(), msg->data.end());
    }

    protected:
    void SetUp() override
    {
        lastRecvd.assign({});
    }

    std::vector<char> getLastRecvd()
    {
        std::vector<char> ret = lastRecvd;
        lastRecvd.clear();
        return ret;
    }

    private:
    std::vector<char> lastRecvd;
};


TEST_F(RosTransceiverTest, TestRosTransceiver_Rx)
{
    rclcpp::Node::SharedPtr n = std::make_shared<rclcpp::Node>("test_rx_node");
    serial_library::SerialTransceiver::SharedPtr trans = std::make_shared<serial_library::RosTransceiver>(n, "test");
    ASSERT_TRUE(trans->init());
    rclcpp::Publisher<std_msgs::msg::ByteMultiArray>::SharedPtr pub = n->create_publisher<std_msgs::msg::ByteMultiArray>("test/rx", 10);

    // test initial cond: recv should return nothing after receiving nothing
    char buf[32];
    rclcpp::spin_some(n);
    size_t recvd = trans->recv(buf, sizeof(buf));
    ASSERT_EQ(recvd, 0);

    // test receiving small message
    std_msgs::msg::ByteMultiArray msg;
    char s1[] = "a";
    msg.data.assign(s1, s1 + sizeof(s1));
    pub->publish(msg);
    rclcpp::spin_some(n);
    recvd = trans->recv(buf, sizeof(buf));
    ASSERT_EQ(recvd, sizeof(s1));
    ASSERT_EQ(memcmp(buf, s1, recvd), 0);

    // //test that buffer clears
    // recvd = trans->recv(buf, sizeof(buf));
    // ASSERT_EQ(recvd, 0);

    //test receiving longer message
    char s2[] = "hello world!";
    msg.data.assign(s2, s2 + sizeof(s2));
    pub->publish(msg);
    rclcpp::spin_some(n);
    recvd = trans->recv(buf, sizeof(buf));
    ASSERT_EQ(recvd, sizeof(s2));
    ASSERT_EQ(memcmp(buf, s2, recvd), 0);

    // //test that buffer clears
    // rclcpp::spin_some(n);
    // recvd = trans->recv(buf, sizeof(buf));
    // ASSERT_EQ(recvd, 0);
}

TEST_F(RosTransceiverTest, TestRosTransceiver_Tx)
{
    rclcpp::Node::SharedPtr n = std::make_shared<rclcpp::Node>("test_tx_node");
    serial_library::SerialTransceiver::SharedPtr trans = std::make_shared<serial_library::RosTransceiver>(n, "test");
    ASSERT_TRUE(trans->init());
    rclcpp::Subscription<std_msgs::msg::ByteMultiArray>::SharedPtr sub = n->create_subscription<std_msgs::msg::ByteMultiArray>(
        "test/tx", 10, std::bind(&RosTransceiverTest::cb, this, _1));
    
    // test that nothing is unintentionally transmitted
    rclcpp::spin_some(n);
    std::vector<char> vc = getLastRecvd();
    ASSERT_EQ(vc.size(), 0);

    // transmit small
    char s1[] = "h";
    trans->send(s1, sizeof(s1));
    rclcpp::spin_some(n);
    vc = getLastRecvd();
    std::string rs1(vc.begin(), vc.end()-1);
    ASSERT_EQ(rs1, std::string(s1));

    // test that nothing is unintentionally transmitted
    rclcpp::spin_some(n);
    vc = getLastRecvd();
    ASSERT_EQ(vc.size(), 0);

    //transmit big
    char s2[] = "hello world!";
    trans->send(s2, sizeof(s2));
    rclcpp::spin_some(n);
    vc = getLastRecvd();
    std::string rs2(vc.begin(), vc.end()-1);
    ASSERT_EQ(rs2, std::string(s2));

    // test that nothing is unintentionally transmitted
    rclcpp::spin_some(n);
    vc = getLastRecvd();
    ASSERT_EQ(vc.size(), 0);
}

#endif