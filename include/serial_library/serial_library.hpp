#pragma once

#include "serial_library/serial_library_base.hpp"
#include "serial_library/logging.hpp"

#if defined(USE_LINUX)
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

//
// SerialTransceiver base class declaration
//
namespace serial_library
{
    class SERLIB_API SerialTransceiver
    {
        public:
        typedef std::shared_ptr<SerialTransceiver> SharedPtr;
        typedef std::unique_ptr<SerialTransceiver> UniquePtr;

        virtual bool init(void) = 0;
        virtual void send(const char *data, size_t numData) = 0;
        virtual size_t recv(char *data, size_t numData) = 0;
        virtual void deinit(void) = 0;
    };


    class SERLIB_API IntraProcessChannel
    {
        friend class IntraProcessChannel;

        public:
        IntraProcessChannel() = default;
        void setPartner(const std::shared_ptr<IntraProcessChannel>& partner);
        bool hasPartner() const;

        void send(const char *data, size_t numData);
        size_t recv(char *data, size_t numData);

        protected:
        void injectData(const char *data, size_t numData);

        private:
        vector<char> _data;
        std::shared_ptr<IntraProcessChannel> _partner;
    };


    class SERLIB_API IntraProcessTransceiver : public SerialTransceiver
    {
        public:
        IntraProcessTransceiver(const std::shared_ptr<IntraProcessChannel>& channel);
        IntraProcessTransceiver();

        std::shared_ptr<IntraProcessChannel> getChannel();

        bool init(void) override;
        void send(const char *data, size_t numData) override;
        size_t recv(char *data, size_t numData) override;
        void deinit(void) override;

        private:
        std::shared_ptr<IntraProcessChannel> _channel;
    };
}

#if defined(USE_LINUX)

namespace serial_library
{
    class SERLIB_API LinuxSerialTransceiver : public SerialTransceiver
    {
        public:
        LinuxSerialTransceiver() = default;
        LinuxSerialTransceiver(
            const std::string& fileName,
            int baud,
            int minimumBytes = 1,
            int maximumTimeout = 0,
            int mode = O_RDWR,
            int bitsPerByte = CS8,
            bool twoStopBits = false,
            bool parityBit = false);

        bool init(void) override;
        void send(const char *data, size_t numData) override;
        size_t recv(char *data, size_t numData) override;
        void deinit(void) override;

        private:
        std::string fileName;
        int
            file,
            baud,
            mode,
            bitsPerByte,
            minimumBytes,
            maximumTimeout;
        
        bool 
            initialized,
            twoStopBits,
            parityBit;
    };


    class SERLIB_API LinuxUDPTransceiver : public SerialTransceiver
    {
        public:
        LinuxUDPTransceiver() = default;
        LinuxUDPTransceiver(
            const std::string& address, 
            int port, 
            double recvTimeoutSeconds = 0.01,
            bool skipBind = false, 
            bool skipConnect = false, 
            bool allowAddrReuse=false);

        bool init(void) override;
        void send(const char *data, size_t numData) override;
        size_t recv(char *data, size_t numData) override;
        void deinit(void) override;

        private:
        const std::string address;
        const int port;
        const double recvTimeoutSeconds;
        const bool 
            allowAddrReuse,
            skipBind,
            skipConnect;
        int sock;
    };

    /**
     * This transceiver should be used when a bidirectional UDP connection is desired
     * between two sockets connected to localhost. This is special functionality
     * that requires two sockets per transceiver.
     */
    class SERLIB_API LinuxDualUDPTransceiver : public SerialTransceiver
    {
        public:
        LinuxDualUDPTransceiver() = default;
        LinuxDualUDPTransceiver(const std::string& address, int recvPort, int sendPort, double recvTimeoutSeconds = 0.01, bool allowReuseAddr = false);

        bool init(void) override;
        void send(const char *data, size_t numData) override;
        size_t recv(char *data, size_t numData) override;
        void deinit(void) override;

        private:
        LinuxUDPTransceiver
            recvUDP,
            sendUDP;
    };
}

#endif

#if defined(USE_WINDOWS)

#include <Windows.h>

namespace serial_library
{
    SERLIB_API std::string getWindowsMsgAsString(DWORD error);

    class WindowsSerialTransceiver : public SerialTransceiver 
    {
        public:
        WindowsSerialTransceiver(
            const std::string& name,
            DWORD baud = CBR_115200,
            DWORD readTimeout = 0,
            DWORD mode = GENERIC_READ | GENERIC_WRITE,
            DWORD bitsPerByte = 8,
            DWORD stopBits = ONESTOPBIT,
            DWORD parityBit = NOPARITY);

        bool init(void) override;
        void send(const char *data, size_t numData) override;
        size_t recv(char *data, size_t numData) override;
        void deinit(void) override;

        private:
        const std::string _portName;
        const DWORD
            _baud,
            _readTimeout,
            _mode,
            _bitsPerByte,
            _stopBits,
            _parityBit;

        HANDLE _port;
        bool _initialized;
    };

}

#endif

#if defined(USE_ROS)

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/byte_multi_array.hpp>
#include <deque>

namespace serial_library
{

    class RosTransceiver : public SerialTransceiver
    {
        public:
        RosTransceiver() = default;
        RosTransceiver(const rclcpp::Node::SharedPtr& node, const std::string& ns, size_t maxQSz = 5, bool isBridge = false);
        
        bool init(void) override;
        void send(const char *data, size_t numData) override;
        size_t recv(char *data, size_t numData) override;
        void deinit(void) override;

        private:
        const std::string ns;
        const size_t maxQueueSize;
        const bool isBridge;
        rclcpp::Node::SharedPtr n;
        std::deque<std::vector<char>> msgQ;

        void rxCb(std_msgs::msg::ByteMultiArray::ConstSharedPtr msg);

        rclcpp::Subscription<std_msgs::msg::ByteMultiArray>::SharedPtr rx;
        rclcpp::Publisher<std_msgs::msg::ByteMultiArray>::SharedPtr tx;
    };

}

#endif

//
// util funcs
//

namespace serial_library
{
    SERLIB_API string wStringToString(const std::wstring& wstr);
    SERLIB_API char *memstr(const char *haystack, size_t numHaystack, const char *needle, size_t numNeedle);
    SERLIB_API size_t extractFieldFromBuffer(const char *src, size_t srcLen, SerialFrame frame, SerialFieldId field, char *dst, size_t dstLen);
    SERLIB_API void insertFieldToBuffer(char *dst, size_t dstLen, SerialFrame frame, SerialFieldId field, const char *src, size_t srcLen);
    SERLIB_API size_t deleteFieldAndShiftBuffer(char *buf, size_t bufLen, SerialFrame frame, SerialFieldId field);
    SERLIB_API size_t deleteChecksumFromBuffer(char *buf, size_t bufLen, SerialFrame frame);
    SERLIB_API SerialData serialDataFromString(const char *str, size_t numData);
    SERLIB_API SerialData switchDataEndianness(const SerialData& data);
    SERLIB_API SerialDataStamped serialDataStampedFromString(const char *str, size_t numData, const Time& stamp);
    SERLIB_API SerialDataStamped serialDataStampedFromString(const string& data, const Time& stamp);
    SERLIB_API SerialDataStamped switchStampedDataEndianness(const SerialDataStamped& data);
    
    SERLIB_API size_t countInString(const string& s, char c);

    typedef pair<SerialFrameId, size_t> SerialFrameComponent;
    SERLIB_API SerialFrame assembleSerialFrame(const vector<SerialFrameComponent>& components);
    
    // "normalized" in this case means that the frame starts with a sync, makes processing easier
    SERLIB_API SerialFrame normalizeSerialFrame(const SerialFrame& frame);
    SERLIB_API SerialFramesMap normalizeSerialFramesMap(const SerialFramesMap& map);

    // packs c string into primitive type. 0 is most significant
    template<typename T>
    T convertFromCString(const char *str, size_t strLen)
    {
        T val = (T) 0;

        if(strLen <= 0)
        {
            return (T) 0;
        }

        size_t tSz = sizeof(T) / sizeof(*str);

        //shift the smaller number of bytes
        size_t placesToShift = (tSz < strLen ? tSz : strLen);

        val = (T) (val | str[0]);
        for(size_t i = 1; i < placesToShift; i++)
        {
            val = (T) (val << sizeof(*str) * 8);
            val = (T) (val | (str[i] & 0xFF));
        }

        return val;
    }
    
    template<typename T>
    size_t convertToCString(T val, char *str, size_t strLen)
    {
        size_t valLen = sizeof(val);
        size_t numData = 0;
        for(int i = valLen / sizeof(*str) - 1; i >= 0; i--)
        {
            char newC = (char) val & 0xFF;
            if((size_t) i < strLen)
            {
                str[i] = newC;
                numData++;
            }

            val = (T) (val >> sizeof(*str) * 8);
        }

        return numData;
    }

    template<typename T>
    T convertData(const SerialDataStamped& data)
    {
        return convertFromCString<T>(data.data.data, data.data.numData);
    }

    template<typename T>
    class ProtectedResource
    {
        public:
        typedef std::shared_ptr<ProtectedResource> SharedPtr;
        typedef std::unique_ptr<ProtectedResource> UniquePtr;
        ProtectedResource() = default;
        ProtectedResource(std::unique_ptr<T> resource)
        : resource(std::move(resource)) { }

        std::unique_ptr<T> lockResource()
        {
            lock.lock();
            return std::move(resource);
        }

        void unlockResource(std::unique_ptr<T> resource)
        {
            this->resource = std::move(resource);
            lock.unlock();
        }

        private:
        mutex lock;
        std::unique_ptr<T> resource;
    };


    static void defaultNewMessageCallback(const SerialValuesMap& map)
    { }


    static bool defaultChecksumEvaluationFunc(const char* msg, size_t len, Checksum checksum)
    {
        return true;
    }


    static Checksum defaultChecksumGeneratorFunc(const char *msgStart, size_t len)
    {
        return 0;
    }


    struct SerialProcessorCallbacks
    {
        NewMsgFunc newMessageCallback = &defaultNewMessageCallback;
        ChecksumEvaluator checksumEvaluationFunc = &defaultChecksumEvaluationFunc;
        ChecksumGenerator checksumGenerationFunc = &defaultChecksumGeneratorFunc;
    };

    const SerialProcessorCallbacks DEFAULT_CALLBACKS;


    class SerialProcessor
    {
        public:
        typedef std::shared_ptr<SerialProcessor> SharedPtr;
        typedef std::unique_ptr<SerialProcessor> UniquePtr;

        SerialProcessor() = default;

        SerialProcessor(
            const SerialFramesMap& frames,
            const SerialFrameId& defaultFrame,
            const char syncValue[],
            size_t syncValueLen,
            bool switchEndianness = false,
            const SerialProcessorCallbacks& callbacks = DEFAULT_CALLBACKS,
            const std::string& debugName = "SerialProcessor");

        SerialProcessor(
            std::unique_ptr<SerialTransceiver> transceiver,
            const SerialFramesMap& frames,
            const SerialFrameId& defaultFrame,
            const char syncValue[],
            size_t syncValueLen,
            bool switchEndianness = false,
            const SerialProcessorCallbacks& callbacks = DEFAULT_CALLBACKS,
            const std::string& debugName = "SerialProcessor");
        
        ~SerialProcessor();

        bool hasTransceiver();
        void setTransceiver(SerialTransceiver::UniquePtr& transceiver);
        void resetTransceiver();
        void update(const Time& now);
        bool hasDataForField(SerialFieldId field);
        Time getLastMsgRecvTime(void) const;
        SerialDataStamped getField(SerialFieldId field);

        template<typename T>
        T getFieldValue(SerialFieldId field)
        {
            SerialDataStamped data = getField(field);
            T val = convertFromCString<T>(data.data.data, data.data.numData);
            return val;
        }

        Time getFieldTimestamp(SerialFieldId id);
        void setField(SerialFieldId field, SerialData data, const Time& now);

        template<typename T>
        void setFieldValue(SerialFieldId field, const T& val, const Time& now)
        {
            SerialData data;
            data.numData = convertToCString(val, data.data, sizeof(T));
            setField(field, data, now);
        }

        void send(const SerialFrameId& frameId);
        unsigned short failedOfLastTenMessages();

        private:
        void ctorFunc(const char syncValue[MAX_DATA_BYTES], size_t syncLen);

        // regular member vars
        char msgBuffer[PROCESSOR_BUFFER_SIZE]; // update() only
        char updateChecksumlessBuffer[PROCESSOR_BUFFER_SIZE]; //update() only
        char sendChecksumlessBuffer[PROCESSOR_BUFFER_SIZE]; //send() only
        char updateTransmissionBuffer[PROCESSOR_BUFFER_SIZE]; //update() only
        char sendTransmissionBuffer[PROCESSOR_BUFFER_SIZE]; //send() only
        char fieldBuf[MAX_DATA_BYTES]; //update() only

        unsigned short 
            failedOfLastTen,
            failedOfLastTenCounter,
            totalOfLastTenCounter;
        
        size_t msgBufferCursorPos;
        Time lastMsgRecvTime;
        char syncValue[MAX_DATA_BYTES];
        const size_t syncValueLen;
    
        const SerialFramesMap frameMap;
        const SerialFrameId defaultFrame;
        const bool switchEndianness;
        const SerialProcessorCallbacks callbacks;
        const std::string debugName;
        
        // "thread-safe" resources 
        ProtectedResource<SerialValuesMap> valueMapResource;
        ProtectedResource<SerialTransceiver> transceiverResource;
    };

    #if defined(USE_ROS)
    class SerlibRosNode : public rclcpp::Node
    {
        public:
        typedef std::shared_ptr<SerlibRosNode> SharedPtr;
        typedef std::unique_ptr<SerlibRosNode> UniquePtr;
        typedef std::weak_ptr<SerlibRosNode> WeakPtr;

        SerlibRosNode(
            const std::string& name,
            const rclcpp::NodeOptions& options,
            SerialProcessor::SharedPtr& proc);

        SerlibRosNode(
            const std::string& name,
            const rclcpp::NodeOptions& options);
        
        protected:
        bool hasProcessor();
        void setProcessor(const SerialProcessor::SharedPtr& proc);
        SerialProcessor::SharedPtr processor();
        void update();

        private:
        void _initParams();
        void _addTransceiver(const SerialProcessor::SharedPtr& proc);
        SerialProcessor::SharedPtr _processor;
    };
    #endif

}



    