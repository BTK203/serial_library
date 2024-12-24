#pragma once

#if (__linux__ && !defined(FORCE_ARDUINO)) || defined(ENABLE_TESTING)
#define USE_LINUX
#endif

#if defined(FORCE_ARDUINO)
#define USE_ARDUINO
#endif

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(USE_LINUX)
#include <string>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <cstring>
#include <chrono>
#include <mutex>
#include <memory>
#include <functional>
#include <algorithm>
#endif


//
// settings
//
#define MAX_DATA_BYTES 8
#define PROCESSOR_BUFFER_SIZE 4096

namespace serial_library
{
    //
    // platform-dependent includes and typedefs
    //

    #if defined(USE_LINUX)

        typedef std::chrono::time_point<std::chrono::system_clock> Time;
        typedef std::string string;
        typedef std::mutex mutex;
        typedef uint16_t checksum_t;

        template<typename K, typename V>
        using pair = std::pair<K, V>;
        
        template<typename K, typename V>
        using map = std::map<K, V>;

        template<typename T>
        using list = std::list<T>;

        template<typename T>
        using vector = std::vector<T>;

        template<typename T>
        using set = std::set<T>;

        template<typename T>
        using NewMessageFunctionTemplate = std::function<void(const T&)>;

        typedef std::function<bool(const char*, size_t, checksum_t)> ChecksumEvaluator;
        typedef std::function<uint16_t(const char*, size_t)> ChecksumGenerator;
        
        inline Time curtime()
        {
            return std::chrono::system_clock::now();
        }

        template<typename InputIterator, typename T>
        inline InputIterator findit(InputIterator first, InputIterator last, const T& val)
        {
            return std::find(first, last, val);
        }

        template<typename InputIterator, typename T>
        inline size_t countit(InputIterator first, InputIterator last, const T& val)
        {
            return std::count(first, last, val);
        } 

        template<typename T>
        inline string to_string(const T& arg)
        {
            return std::to_string(arg);
        }

    #elif defined(USE_ARDUINO)
        #include "riptide_gyro/arduino_library.hpp"

        typedef long Time;
        typedef arduino_lib::string string;
        typedef arduino_lib::mutex mutex;
        
        template<typename K, typename V>
        using map = arduino_lib::map<K, V>;

        template<typename T>
        using list = arduino_lib::vector<T>;

        template<typename T>
        using vector = arduino_lib::vector<T>;

        typedef void(*NewMsgFunc)(void);

        template<typename T>
        inline string to_string(const T& arg)
        {
            return arduino_lib::to_string(arg);
        }
    #endif


    //
    // exception
    //
    class SerialLibraryException
    {
        public:
        SerialLibraryException(const string& error)
        : error(error)
        { }

        string what()
        {
            return error;
        }

        private:
        string error;
    };

    class NonFatalSerialLibraryException : public SerialLibraryException
    {
        public:
        NonFatalSerialLibraryException(const string& error)
        : SerialLibraryException(error) { }
    };

    class FatalSerialLibraryException : public SerialLibraryException
    {
        public:
        FatalSerialLibraryException(const string& error)
        : SerialLibraryException(error) { }
    };

    #define THROW_FATAL_SERIAL_LIB_EXCEPTION(errmsg) throw FatalSerialLibraryException(string(__FILE__) + string(":") + to_string(__LINE__) + string(": ") + errmsg);
    #define THROW_NON_FATAL_SERIAL_LIB_EXCEPTION(errmsg) throw NonFatalSerialLibraryException(string(__FILE__) + string(":") + to_string(__LINE__) + string(": ") + errmsg);
    #define SERIAL_LIB_ASSERT(cond, msg) \
        do \
        { \
            if(!(cond)) \
            { \
                THROW_FATAL_SERIAL_LIB_EXCEPTION(#cond ": " msg); \
            } \
        } while(0)


    //
    // library typedefs
    //

    #define FIELD_SYNC INT_MAX
    #define FIELD_FRAME FIELD_SYNC - 1
    #define FIELD_CHECKSUM FIELD_FRAME - 1
    #define FIELD_TERM FIELD_CHECKSUM - 1

    typedef uint8_t SerialFrameId;
    typedef int SerialFieldId;

    //describes the fields held by a serial frame. Each frame represents 8 bits.
    typedef vector<SerialFieldId> SerialFrame;
    typedef map<SerialFrameId, SerialFrame> SerialFramesMap;

    struct SerialData
    {
        SerialData() = default;
        SerialData(const SerialData& other)
        : numData(other.numData)
        {
            memcpy(data, other.data, other.numData);
        }
        
        size_t numData;
        char data[MAX_DATA_BYTES];

        SerialData& operator=(const SerialData& rhs)
        {
            if(rhs.numData >= MAX_DATA_BYTES)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION(string("SerialData being assigned must have less than ") + to_string(MAX_DATA_BYTES) + string(" data, but has ") + to_string(rhs.numData));
            }
            numData = rhs.numData;
            memcpy(data, rhs.data, numData);
            return *this;
        }
    };

    struct SerialDataStamped
    {
        SerialDataStamped() = default;
        SerialDataStamped(const SerialDataStamped& other)
        : timestamp(other.timestamp),
        data(other.data) { }

        Time timestamp;
        SerialData data;

        SerialDataStamped& operator=(const SerialDataStamped& rhs)
        {
            timestamp = rhs.timestamp;
            data = rhs.data;
            return *this;
        }
    };

    struct SerialValuesPair
    {
        SerialFieldId id;
        SerialDataStamped data;
    };

    typedef map<SerialFieldId, SerialDataStamped> SerialValuesMap;

    typedef NewMessageFunctionTemplate<SerialValuesMap> NewMsgFunc;
}
