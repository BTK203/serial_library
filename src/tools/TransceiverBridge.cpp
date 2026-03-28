#include "serial_library/serial_library.hpp"

#if defined(USE_ROS)
#include <rclcpp/rclcpp.hpp>

class SpinningRosTransceiver : public serial_library::RosTransceiver
{
    public:
    SpinningRosTransceiver(const rclcpp::Node::SharedPtr& node, const std::string& ns, size_t maxQSz = 5, bool isBridge = false)
    : serial_library::RosTransceiver(node, ns, maxQSz, isBridge), n(node)
    { }

    size_t recv(char *data, size_t numData) override
    {
        rclcpp::spin_some(n);
        return serial_library::RosTransceiver::recv(data, numData);
    }

    private:
    rclcpp::Node::SharedPtr n;
};

serial_library::RosTransceiver::SharedPtr initRosTransceiverWithArgs(int argc, char **argv, int *cursor)
{
    //initialize ros
    static bool rclcppInitted = false;

    if(!rclcppInitted)
    {
        rclcpp::init(argc, argv);
        rclcppInitted = true;
    }

    // get namespace (arg 2)

    if(argc <= *cursor + 1)
    {
        SERLIB_LOG_ERROR("Not enough arguments for ROS transceiver. Use transceiver_bridge ... ros [namespace]");
        return nullptr;
    }

    std::string 
        ns(argv[*cursor + 1]),
        nodeName = "transceiver_bridge_" + ns;

    rclcpp::Node::SharedPtr n = std::make_shared<rclcpp::Node>(nodeName);
    *cursor += 2;
    SERLIB_LOG_INFO("Creating ROS Transceiver with ns=%s", ns.c_str());
    return std::make_shared<SpinningRosTransceiver>(n, ns, 5, true);
}

#else

serial_library::SerialTransceiver::SharedPtr initRosTransceiverWithArgs(int argc, char **argv)
{
    SERLIB_LOG_ERROR("ROS Support not built!");
    return nullptr;
}

#endif

#if defined(USE_LINUX)

serial_library::LinuxSerialTransceiver::SharedPtr initLinuxSerialTransceiverWithArgs(int argc, char **argv, int *cursor)
{
    // need to define these args
    // const std::string& fileName
    // int baud

    if(argc <= *cursor + 2)
    {
        SERLIB_LOG_ERROR("Not enough arguments for Linux Serial transceiver. Use transceiver_bridge ... linuxser [file] [baud]");
        return nullptr;
    }

    std::string
        file(argv[*cursor + 1]),
        baudStr(argv[*cursor + 2]);
    
    int baud = std::stoi(baudStr);

    *cursor += 3;
    SERLIB_LOG_INFO("Creating Linux Serial transceiver with file=%s, baud=%d", file.c_str(), baud);
    return std::make_shared<serial_library::LinuxSerialTransceiver>(file, baud);
}

serial_library::LinuxUDPTransceiver::SharedPtr initLinuxUDPTransceiverWithArgs(int argc, char **argv, int *cursor)
{
    SERLIB_LOG_ERROR("Linux UDP Transceiver support is not implemented yet");
    return nullptr;
}

serial_library::LinuxDualUDPTransceiver::SharedPtr initLinuxDualUDPTransceiverWithArgs(int argc, char **argv, int *cursor)
{
    SERLIB_LOG_ERROR("Linux Dual UDP Transceiver support is not implemented yet");
    return nullptr;
}

#else

serial_library::LinuxSerialTransceiver::SharedPtr initLinuxSerialTransceiverWithArgs(int argc, char **argv, int *cursor)
{
    SERLIB_LOG_ERROR("Linux support is not built");
    return nullptr;
}

serial_library::LinuxUDPTransceiver::SharedPtr initLinuxUDPTransceiverWithArgs(int argc, char **argv, int *cursor)
{
    SERLIB_LOG_ERROR("Linux support is not built");
    return nullptr;
}

serial_library::LinuxDualUDPTransceiver::SharedPtr initLinuxDualUDPTransceiverWithArgs(int argc, char **argv, int *cursor)
{
    SERLIB_LOG_ERROR("Linux support is not built");
    return nullptr;
}

#endif

serial_library::SerialTransceiver::SharedPtr initWithArgs(int argc, char **argv, int *cursor)
{
    if(argc <= *cursor)
    {
        SERLIB_LOG_ERROR("Not enough arguments to init a transceiver; no transceiver type found. Use transceiver_bridge ... [transc_name] [transc_args...]");
        SERLIB_LOG_ERROR("Some transceiver options: ros, linuxser");
        return nullptr;
    }

    std::string tName(argv[*cursor]);

    if(tName == "ros")
    {
        return initRosTransceiverWithArgs(argc, argv, cursor);
    } else if(tName == "linuxser")
    {
        return initLinuxSerialTransceiverWithArgs(argc, argv, cursor);
    }

    // if we got here then the transceiver name is not known
    SERLIB_LOG_ERROR("Cannot build transceiver by name %s, that type is unknown", tName.c_str());
    return nullptr;
}

bool isRunning = true;

void spin(
    const serial_library::SerialTransceiver::SharedPtr& sender,
    const serial_library::SerialTransceiver::SharedPtr& recver)
{
    char buf[4096];

    while(isRunning)
    {
        size_t recvd = recver->recv(buf, sizeof(buf));
        if(recvd > 0)
        {
            sender->send(buf, recvd);
        }
    }
}


void handleSignal(int signal)
{
    if(signal == SIGINT)
    {
        isRunning = false;
    }
}


void deinit(
    const serial_library::SerialTransceiver::SharedPtr& trans1,
    const serial_library::SerialTransceiver::SharedPtr& trans2)
{
    trans1->deinit();
    trans2->deinit();

    #if defined(USE_ROS)
    rclcpp::shutdown();
    #endif
}


int main(int argc, char **argv)
{
    int cursor = 1;
    serial_library::SerialTransceiver::SharedPtr
        trans1 = initWithArgs(argc, argv, &cursor),
        trans2 = initWithArgs(argc, argv, &cursor);
    
    if(!trans1 || !trans2)
    {
        SERLIB_LOG_ERROR("Failed to init at least one transceiver. Exiting");
        return 1;
    }

    if(!trans1->init() || !trans2->init())
    {
        SERLIB_LOG_ERROR("Failed to init at least one transceiver. Exiting");
        return 1;
    }

    //configure signal handler, set running, then spin
    std::signal(SIGINT, handleSignal);
    isRunning = true;

    //one thread translates messages from trans1 to trans2. The other translates from trans2 to trans1
    std::thread
        t1(spin, trans1, trans2),
        t2(spin, trans2, trans1);

    SERLIB_LOG_ERROR("Transceiver bridge started.");
    
    t1.join();
    t2.join();

    deinit(trans1, trans2);
    return 0;
}