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

namespace serial_library
{
    class SerialTransceiver
    {
        public:
        #if defined(USE_LINUX)
        typedef std::shared_ptr<SerialTransceiver> SharedPtr;
        typedef std::unique_ptr<SerialTransceiver> UniquePtr;
        #endif

        virtual bool init(void) = 0;
        virtual void send(const char *data, size_t numData) const = 0;
        virtual size_t recv(char *data, size_t numData) const = 0;
        virtual void deinit(void) = 0;
    };


    #if defined(USE_LINUX)

    class LinuxSerialTransceiver : public SerialTransceiver
    {
        public:
        LinuxSerialTransceiver() = default;
        LinuxSerialTransceiver(
            const std::string& fileName,
            int baud,
            int minimumBytes,
            int maximumTimeout,
            int mode = O_RDWR,
            int bitsPerByte = CS8,
            bool twoStopBits = false,
            bool parityBit = false);

        bool init(void) override;
        void send(const char *data, size_t numData) const override;
        size_t recv(char *data, size_t numData) const override;
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


    class LinuxUDPTransceiver : public SerialTransceiver
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
        void send(const char *data, size_t numData) const override;
        size_t recv(char *data, size_t numData) const override;
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
    class LinuxDualUDPTransceiver : public SerialTransceiver
    {
        public:
        LinuxDualUDPTransceiver() = default;
        LinuxDualUDPTransceiver(const std::string& address, int recvPort, int sendPort, double recvTimeoutSeconds = 0.01, bool allowReuseAddr = false);

        bool init(void) override;
        void send(const char *data, size_t numData) const override;
        size_t recv(char *data, size_t numData) const override;
        void deinit(void) override;

        private:
        LinuxUDPTransceiver
            recvUDP,
            sendUDP;
    };

    #endif

    char *memstr(const char *haystack, size_t numHaystack, const char *needle, size_t numNeedle);
    size_t extractFieldFromBuffer(const char *src, size_t srcLen, SerialFrame frame, SerialFieldId field, char *dst, size_t dstLen);
    void insertFieldToBuffer(char *dst, size_t dstLen, SerialFrame frame, SerialFieldId field, const char *src, size_t srcLen);
    size_t deleteFieldAndShiftBuffer(char *buf, size_t bufLen, SerialFrame frame, SerialFieldId field);
    size_t deleteChecksumFromBuffer(char *buf, size_t bufLen, SerialFrame frame);
    SerialData serialDataFromString(const char *str, size_t numData);
    SerialData serialDataFromString(const string& data);
    SerialDataStamped serialDataStampedFromString(const char *str, size_t numData, const Time& stamp);
    SerialDataStamped serialDataStampedFromString(const string& data, const Time& stamp);
    
    size_t countInString(const string& s, char c);

    typedef pair<SerialFrameId, size_t> SerialFrameComponent;
    SerialFrame assembleSerialFrame(const vector<SerialFrameComponent>& components);
    
    // "normalized" in this case means that the frame starts with a sync, makes processing easier
    SerialFrame normalizeSerialFrame(const SerialFrame& frame);
    SerialFramesMap normalizeSerialFramesMap(const SerialFramesMap& map);

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
        #if defined(USE_LINUX)
        typedef std::shared_ptr<SerialProcessor> SharedPtr;
        typedef std::unique_ptr<SerialProcessor> UniquePtr;
        #endif

        SerialProcessor() = default;
        SerialProcessor(
            std::unique_ptr<SerialTransceiver> transceiver,
            const SerialFramesMap& frames,
            const SerialFrameId& defaultFrame,
            const char syncValue[],
            size_t syncValueLen,
            const SerialProcessorCallbacks& callbacks = DEFAULT_CALLBACKS,
            const std::string& debugName = "SerialProcessor");
        
        ~SerialProcessor();

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
        const SerialProcessorCallbacks callbacks;
        const std::string debugName;
        
        // "thread-safe" resources 
        ProtectedResource<SerialValuesMap> valueMapResource;
        ProtectedResource<SerialTransceiver> transceiverResource;
    };
}
