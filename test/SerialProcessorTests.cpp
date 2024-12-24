#include "serial_library/testing.hpp"

using namespace serial_library;

#if defined(USE_LINUX)

using namespace std::chrono_literals;
using namespace std::placeholders;

bool LinuxSerialProcessorTest::waitForFrame(SerialFrameId id, Time startTime)
{
    Time now = curtime();
    while(now - startTime < 500ms)
    {
        processor->update(now);
        if(processor->getField(id).timestamp >= startTime)
        {
            return true;
        }

        now = curtime();
    }

    SERLIB_LOG_INFO("Timed out waiting for frame with id %d", id);
    return false;
}


void Type1SerialProcessorTest::SetUp() 
{
    LinuxTransceiverTest::SetUp();
    transceiver = std::make_unique<serial_library::LinuxSerialTransceiver>(
        homeDir() + "virtualsp1",
        9600,
        0, 
        1);
    
    frameMap = {
        {Type1SerialFrames1::TYPE_1_FRAME_1, 
            {
                FIELD_SYNC,
                Type1SerialFrame1Fields::TYPE_1_FRAME_1_FIELD_1,
                Type1SerialFrame1Fields::TYPE_1_FRAME_1_FIELD_2,
                Type1SerialFrame1Fields::TYPE_1_FRAME_1_FIELD_3,
            }
        }
    };

    const char syncValue[1] = {'A'};
    processor = std::make_shared<serial_library::SerialProcessor>(
        std::move(transceiver), 
        frameMap,
        Type1SerialFrames1::TYPE_1_FRAME_1, 
        syncValue, 
        sizeof(syncValue));
}


void Type1SerialProcessorTest::TearDown()
{
    LinuxTransceiverTest::TearDown();
}


void Type2SerialProcessorTest::SetUp() 
{
    LinuxTransceiverTest::SetUp();
    transceiver = std::make_unique<serial_library::LinuxSerialTransceiver>(
        homeDir() + "virtualsp1",
        9600,
        0,
        1);
    
    const char syncValue[1] = {'A'};
    processor = std::make_shared<serial_library::SerialProcessor>(
        std::move(transceiver),
        TYPE_2_FRAME_MAP,
        Type1SerialFrames1::TYPE_1_FRAME_1,
        syncValue,
        sizeof(syncValue));
}


void Type2SerialProcessorTest::TearDown()
{
    LinuxTransceiverTest::TearDown();
}


void CallbacksTest::SetUp() 
{
    LinuxTransceiverTest::SetUp();
    transceiver = std::make_unique<serial_library::LinuxSerialTransceiver>(
        homeDir() + "virtualsp1",
        9600,
        0,
        1);
    
    SerialProcessorCallbacks callbacks;
    callbacks.newMessageCallback = std::bind(&CallbacksTest::newMsgCallback, this, _1);
    callbacks.checksumEvaluationFunc = std::bind(&CallbacksTest::checksumEvaluator, this, _1, _2, _3);
    callbacks.checksumGenerationFunc = std::bind(&CallbacksTest::checksumGenerator, this, _1, _2);

    const char syncValue[1] = {'A'};
    processor = std::make_shared<serial_library::SerialProcessor>(
        std::move(transceiver),
        TYPE_2_FRAME_MAP,
        Type1SerialFrames1::TYPE_1_FRAME_1,
        syncValue,
        sizeof(syncValue),
        callbacks);
}


void CallbacksTest::TearDown()
{
    LinuxTransceiverTest::TearDown();
}


SerialValuesMap CallbacksTest::lastReceivedSerialFrames() const
{
    return lastReceivedMap;
}


void CallbacksTest::newMsgCallback(const SerialValuesMap& frames)
{
    lastReceivedMap = frames;
}

// checksum is good if sum of the characters equals the checksum
bool CallbacksTest::checksumEvaluator(const char *msg, size_t len, checksum_t checksum)
{
    unsigned int sum = 0;
    for(unsigned int i = 0; i < len; i++)
    {
        sum += msg[i];
    }

    return sum % 2 == 0;
}


uint16_t CallbacksTest::checksumGenerator(const char* msg, size_t len)
{

}

#endif
