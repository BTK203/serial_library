#include "serial_library/serial_library.hpp"
#include "serial_library/testing.hpp"

#if defined(USE_LINUX)

TEST_F(LinuxTransceiverTest, TestSocketpair)
{
    serial_library::LinuxSocketpairTransceiver trans1(AF_UNIX, SOCK_STREAM);
    trans1.init();

    pid_t p = fork();
    ASSERT_NE(p, -1);

    char buf[32];
    size_t s;
    if(p == 0)
    {
        // child
        serial_library::LinuxSocketpairTransceiver trans2(trans1.childFd());
        trans2.init();
        trans2.postForkInit();
        trans2.send("hello", sizeof("hello"));
        exit(0);
    } else
    {
        trans1.postForkInit();
        int ret = waitpid(p, NULL, 0); // wait for child process to send a message and exit
        ASSERT_NE(ret, -1);

        s = trans1.recv(buf, sizeof(buf));
        ASSERT_EQ(s, sizeof("hello"));
        ASSERT_EQ("hello", std::string(buf));
    }

    s = trans1.recv(buf, sizeof(buf));
    ASSERT_EQ(s, 0);
}

#endif
