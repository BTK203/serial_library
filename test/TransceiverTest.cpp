#include "serial_library/testing.hpp"

using namespace serial_library;

bool TransceiverTest::compareSerialData(const SerialData& data1, const SerialData& data2)
{
    if(data1.numData != data2.numData || memcmp(data1.data, data2.data, data1.numData) != 0)
    {
        SERLIB_LOG_INFO("data1 with length %d does not match data2 with length %d", data1.numData, data2.numData);
        SERLIB_LOG_INFO("data1: \"%s\", data2: \"%s\"", data1.data, data2.data);
        return false;
    }

    return true;
}
