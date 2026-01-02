#include <rclcpp/rclcpp.hpp>
#include "serial_library/serial_library.hpp"

#if defined(USE_ROS)

using namespace std::chrono_literals;
using namespace std::placeholders;

namespace serial_library
{
    
    SerlibRosNode::SerlibRosNode(
        const std::string& name,
        const rclcpp::NodeOptions& options,
        SerialProcessor::SharedPtr& proc)
    : rclcpp::Node(name, options)
    {
        _processor = proc;
        _initParams();
    }


    SerlibRosNode::SerlibRosNode(
        const std::string& name,
        const rclcpp::NodeOptions& options)
    : rclcpp::Node(name, options)
    {
        _initParams();
    }


    bool SerlibRosNode::hasProcessor()
    {
        return _processor != nullptr;
    }


    void SerlibRosNode::setProcessor(const SerialProcessor::SharedPtr& proc)
    {
        _processor = proc;
    }


    SerialProcessor::SharedPtr SerlibRosNode::processor()
    {
        return _processor;
    }


    void SerlibRosNode::update()
    {
        if(!_processor)
        {
            RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 1000, "Cannot update because there is no processor. Please set it using setProcessor()");
        }

        if(!_processor->hasTransceiver())
        {
            _addTransceiver(_processor);
            return;
        }

        try
        {
            serial_library::Time now = serial_library::curtime();

            //process incoming messages
            _processor->update(now);
        } catch(serial_library::NonFatalSerialLibraryException& ex)
        {
            RCLCPP_WARN(get_logger(), "Caught exception: %s", ex.what());
        }
    }


    void SerlibRosNode::_initParams()
    {
        declare_parameters("", std::map<std::string, rclcpp::ParameterValue>{
            { "transceiver", rclcpp::ParameterValue("ser") },
            { "serial_port", rclcpp::ParameterValue("/dev/ttyUSB0") },
            { "serial_baud", rclcpp::ParameterValue(9600) },
            { "ros_ns", rclcpp::ParameterValue("/motor") }
        });
    }


    void SerlibRosNode::_addTransceiver(const SerialProcessor::SharedPtr& proc)
    {
        std::string 
            transType = get_parameter("transceiver").as_string(),
            port = get_parameter("serial_port").as_string();

        int baud = get_parameter("serial_baud").as_int();
        std::string rosNs = get_parameter("ros_ns").as_string();

        RCLCPP_INFO(get_logger(), "Init transcevier, type %s", transType.c_str());

        //initialize serial library
        try
        {
            serial_library::SerialTransceiver::UniquePtr transceiver = nullptr;
            if(transType == "ser")
            {
                RCLCPP_INFO(get_logger(), "Serial transceiver port \"%s\" baud %d", port.c_str(), baud);
                transceiver = std::make_unique<serial_library::LinuxSerialTransceiver>(port, baud);
            } else if(transType == "ros")
            {
                RCLCPP_INFO(get_logger(), "ROS transceiver namespace \"%s\"", rosNs.c_str());
                transceiver = std::make_unique<serial_library::RosTransceiver>(shared_from_this(), rosNs);
            }

            if(!transceiver)
            {
                throw std::runtime_error("unknown transceiver type " + transType);
            }

            if(proc->hasTransceiver())
            {
                // bad
                RCLCPP_ERROR(get_logger(), "Transceiver is already populated! Serlib may display undefined behavior");
                return;
            }

            proc->setTransceiver(transceiver);

        } catch(serial_library::FatalSerialLibraryException& ex)
        {
            RCLCPP_FATAL(get_logger(), "Failed to initialize serial processor! %s", ex.what());
            exit(1);
        }
    }
}

#endif
