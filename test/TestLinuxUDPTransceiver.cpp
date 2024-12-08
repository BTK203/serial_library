#include "serial_library/serial_library.hpp"
#include "serial_library/testing.hpp"

#if defined(USE_LINUX)

TEST_F(LinuxTransceiverTest, TestUDPTransceiverUnidirectional)
{
    serial_library::LinuxUDPTransceiver
        transceiver1("127.0.0.1", 9978, true),
        transceiver2("127.0.0.1", 9978, true);
    
    const std::string
        expected1 = "hello!",
        expected2 = "world!",
        expected3 = "!!";

    char buffer[50] = {0};

    transceiver1.init();
    transceiver2.init();

    transceiver1.send(expected1.c_str(), expected1.length());
    size_t recvd1 = transceiver2.recv(buffer, sizeof(buffer));
    std::string msg1((char*) buffer);
    memset(buffer, 0, sizeof(buffer));
    transceiver1.send(expected2.c_str(), expected2.length());
    size_t recvd2 = transceiver2.recv(buffer, sizeof(buffer));
    std::string msg2((char*) buffer);
    memset(buffer, 0, sizeof(buffer));
    transceiver1.send(expected3.c_str(), expected3.length());
    size_t recvd3 = transceiver2.recv(buffer, sizeof(buffer));
    std::string msg3((char*) buffer);
    memset(buffer, 0, sizeof(buffer));

    transceiver1.deinit();
    transceiver2.deinit();
    
    ASSERT_EQ(expected1, msg1);
    ASSERT_EQ(expected1.length(), recvd1);
    ASSERT_EQ(expected2, msg2);
    ASSERT_EQ(expected2.length(), recvd2);
    ASSERT_EQ(expected3, msg3);
    ASSERT_EQ(expected3.length(), recvd3);
}

TEST_F(LinuxTransceiverTest, TestUDPTransceiverBidirectional)
{
    serial_library::LinuxUDPTransceiver
        transceiver1("127.0.0.1", 9978, true),
        transceiver2("127.0.0.1", 9978, true);
    
    const std::string
        expected1 = "hello!",
        expected2 = "world!";

    char buffer[50] = {0};

    transceiver1.init();
    transceiver2.init();

    transceiver1.send(expected1.c_str(), expected1.length());
    size_t recvd1 = transceiver2.recv(buffer, sizeof(buffer));
    std::string msg1((char*) buffer);
    memset(buffer, 0, sizeof(buffer));
    transceiver2.send(expected2.c_str(), expected2.length());
    size_t recvd2 = transceiver1.recv(buffer, sizeof(buffer));
    std::string msg2((char*) buffer);
    memset(buffer, 0, sizeof(buffer));

    transceiver1.deinit();
    transceiver2.deinit();
    
    ASSERT_EQ(expected1, msg1);
    ASSERT_EQ(expected1.length(), recvd1);
    ASSERT_EQ(expected2, msg2);
    ASSERT_EQ(expected2.length(), recvd2);
}

#endif
