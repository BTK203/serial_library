#include "serial_library/serial_library.hpp"
#include "serial_library/testing.hpp"

TEST(IntraProcessTransceiverTest, TestIntraProcessTransceiverHasPartner)
{
    auto ipt = std::make_shared<serial_library::IntraProcessTransceiver>();
    ASSERT_TRUE(ipt->init());
    ASSERT_FALSE(ipt->getChannel()->hasPartner());
    auto other = std::make_shared<serial_library::IntraProcessTransceiver>();
    ASSERT_TRUE(other->init());
    ipt->getChannel()->setPartner(other->getChannel());
    ASSERT_TRUE(ipt->getChannel()->hasPartner());
    
    ipt->deinit();
    other->deinit();
}

TEST(IntraProcessTransceiverTest, TestIntraProcessTransceiverSendNoPartner)
{
    // if this test fails it will cause a segfault. if it gets to the end we golden
    auto ipt = std::make_shared<serial_library::IntraProcessTransceiver>();
    ASSERT_TRUE(ipt->init());
    const std::string s = "hello!!!";
    ipt->send(s.c_str(), s.length());
    ASSERT_TRUE(true); // yay!!!!!!

    ipt->deinit();
}

TEST(IntraProcessTransceiverTest, TestIntraProcessTransceiverUnidirectional)
{
    auto t1 = std::make_shared<serial_library::IntraProcessTransceiver>();
    auto t2 = std::make_shared<serial_library::IntraProcessTransceiver>();

    ASSERT_TRUE(t1->init() && t2->init());

    t1->getChannel()->setPartner(t2->getChannel());
    t2->getChannel()->setPartner(t1->getChannel());

    const std::string
        expected1 = "hello ",
        expected2 = "world",
        expected3 = "!!";

    char buffer[64] = {0};

    //test that a message is transmitted
    t1->send(expected1.c_str(), expected1.length());
    size_t recvd1 = t2->recv(buffer, sizeof(buffer));
    std::string msg1((char *) buffer);
    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(expected1, msg1);
    ASSERT_EQ(expected1.length(), recvd1);

    //test that the buffer is cleared following a call to recv()
    size_t recvd2 = t2->recv(buffer, sizeof(buffer));
    std::string msg2((char *) buffer);
    memset(buffer, 0, sizeof(buffer));
    ASSERT_TRUE(msg2.empty());
    ASSERT_EQ(recvd2, 0);

    //test that buffer buffers
    t1->send(expected1.c_str(), expected1.length());
    t1->send(expected2.c_str(), expected2.length());
    t1->send(expected3.c_str(), expected3.length());
    size_t recvd3 = t2->recv(buffer, sizeof(buffer));
    std::string msg3((char *) buffer);
    memset(buffer, 0, sizeof(buffer));
    std::string long_expected = expected1 + expected2 + expected3;
    ASSERT_EQ(long_expected, msg3);
    ASSERT_EQ(long_expected.length(), recvd3);

    t1->deinit();
    t2->deinit();
}

TEST(IntraProcessTransceiverTest, TestIntraProcessTransceiverBidirectional)
{
    auto t1 = std::make_shared<serial_library::IntraProcessTransceiver>();
    auto t2 = std::make_shared<serial_library::IntraProcessTransceiver>();

    ASSERT_TRUE(t1->init() && t2->init());

    t1->getChannel()->setPartner(t2->getChannel());
    t2->getChannel()->setPartner(t1->getChannel());

    const std::string
        something = "hello",
        somethingElse = "world";

    char buffer[64] = {0};

    t1->send(something.c_str(), something.length());
    size_t recvd1 = t2->recv(buffer, sizeof(buffer));
    std::string msg1((char *) buffer);
    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(something, msg1);
    ASSERT_EQ(something.length(), recvd1);

    t2->send(somethingElse.c_str(), somethingElse.length());
    size_t recvd2 = t1->recv(buffer, sizeof(buffer));
    std::string msg2((char *) buffer);
    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(somethingElse, msg2);
    ASSERT_EQ(somethingElse.length(), recvd2);

    t1->deinit();
    t2->deinit();
}