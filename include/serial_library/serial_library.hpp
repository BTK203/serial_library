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
        LinuxUDPTransceiver(const std::string& address, int port, bool skipBind = false, bool skipConnect = false, bool allowAddrReuse=false);

        bool init(void) override;
        void send(const char *data, size_t numData) const override;
        size_t recv(char *data, size_t numData) const override;
        void deinit(void) override;

        private:
        const std::string address;
        const int port;
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
        LinuxDualUDPTransceiver(const std::string& address, int recvPort, int sendPort);

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
    SerialData serialDataFromString(const char *str, size_t numData);
    
    // "normalized" in this case means that the frame starts with a sync, makes processing easier
    SerialFrame normalizeSerialFrame(const SerialFrame& frame);
    SerialFramesMap normalizeSerialFramesMap(const SerialFramesMap& map);

    // packs c string into primitive type. 0 is most significant
    template<typename T>
    T convertFromCString(const char *str, size_t strLen)
    {
        T val = 0;

        if(strLen <= 0)
        {
            return 0;
        }

        size_t tSz = sizeof(T) / sizeof(*str);

        //shift the smaller number of bytes
        size_t placesToShift = (tSz < strLen ? tSz : strLen);

        val |= str[0];
        for(size_t i = 1; i < placesToShift; i++)
        {
            val = val << sizeof(*str) * 8;
            val |= str[i] & 0xFF;
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

            val = val >> sizeof(*str) * 8;
        }

        return numData;
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


    static bool defaultCheckFunc(const char*, const SerialFrame&)
    {
        return true;
    }


    class SerialProcessor
    {
        public:
        #if defined(USE_LINUX)
        typedef std::shared_ptr<SerialProcessor> SharedPtr;
        typedef std::unique_ptr<SerialProcessor> UniquePtr;
        #endif

        SerialProcessor() = default;
        SerialProcessor(std::unique_ptr<SerialTransceiver> transceiver, const SerialFramesMap& frames, const SerialFrameId& defaultFrame, const char syncValue[], size_t syncValueLen, CheckFunc checker = &defaultCheckFunc);
        ~SerialProcessor();
        void setNewMsgCallback(const NewMsgFunc& func);
        void update(const Time& now);
        bool hasDataForField(SerialFieldId field);
        SerialDataStamped getField(SerialFieldId field);
        void setField(SerialFieldId field, SerialData data, const Time& now);
        void send(SerialFrameId frameId);
        unsigned short failedOfLastTenMessages();

        private:
        // regular member vars
        std::unique_ptr<SerialTransceiver> transceiver;
        char 
            msgBuffer[PROCESSOR_BUFFER_SIZE],
            transmissionBuffer[PROCESSOR_BUFFER_SIZE],
            fieldBuf[PROCESSOR_BUFFER_SIZE];
        
        unsigned short 
            failedOfLastTen,
            failedOfLastTenCounter,
            totalOfLastTenCounter;
        
        size_t msgBufferCursorPos;
        char syncValue[MAX_DATA_BYTES];
        size_t syncValueLen;

        const SerialFramesMap frameMap;
        const SerialFrameId defaultFrame;
        const CheckFunc checker;
        NewMsgFunc newMsgFunc; // non-const becuase this can be set with a function
        
        // "thread-safe" resources 
        ProtectedResource<SerialValuesMap> valueMap;
    };
}
