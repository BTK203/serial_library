#include "serial_library/serial_library.hpp"
#include "serial_library/testing.hpp"

using namespace std::chrono_literals;

#if defined(USE_LINUX)

class TestTransceiver : public serial_library::SerialTransceiver
{
    public:
    TestTransceiver(bool initRet)
     : initRet(initRet) { }

    bool init(void)
    {
        return initRet;
    }

    void send(const char *data, size_t numData) const { }
    size_t recv(char *data, size_t numData) const { return 0; }
    void deinit(void) { }

    private:
    bool initRet;
};


TEST_F(Type1SerialProcessorTest, TestBasicRecvWithManualSendType1)
{
    serial_library::LinuxSerialTransceiver client(
        homeDir() + "virtualsp2",
        9600,
        1,
        0);
    
    client.init();
    
    const char msg[] = "AqweA";

    client.send(msg, sizeof(msg));
    
    Time startTime = curtime();
    processor->update(curtime());

    ASSERT_TRUE(processor->hasDataForField(FIELD_SYNC));

    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_1_FRAME_1_FIELD_1).data, serial_library::serialDataFromString("q", 1)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_1_FRAME_1_FIELD_2).data, serial_library::serialDataFromString("w", 1)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_1_FRAME_1_FIELD_3).data, serial_library::serialDataFromString("e", 1)));
}

TEST_F(Type1SerialProcessorTest, TestBasicSendWithManualRecvType1)
{
    serial_library::LinuxSerialTransceiver client(
        homeDir() + "virtualsp2",
        9600,
        1,
        0);
    
    client.init();

    //pack msg and send
    processor->setField(TYPE_1_FRAME_1_FIELD_1, serial_library::serialDataFromString("a", 1), curtime());
    processor->setField(TYPE_1_FRAME_1_FIELD_2, serial_library::serialDataFromString("b", 1), curtime());
    processor->setField(TYPE_1_FRAME_1_FIELD_3, serial_library::serialDataFromString("c", 1), curtime());

    processor->send(TYPE_1_FRAME_1);

    char buf[4];
    client.recv(buf, 4);
    
    ASSERT_TRUE(memcmp(buf, "Aabc", 4) == 0);
}

TEST_F(Type2SerialProcessorTest, TestBasicRecvWithManualSendType2)
{
    serial_library::LinuxSerialTransceiver client(
        homeDir() + "virtualsp2",
        9600,
        1,
        0);
    
    client.init();

    //define messages (to be sent in increasing order)
    const char
        msg1[] = {'A', 'a', 0, 'b', 'c', 'A', 'd', 'e'},
        msg2[] = {'a', 1, 'b', 'c', 'A', 'd', 'e'},
        msg3[] = {'q', 'A', 'b', 2, 'c', 'd', 'A', 'e', 'z'};
    
    Time now = curtime();

    client.send(msg1, sizeof(msg1));
    processor->update(now);

    ASSERT_TRUE(processor->hasDataForField(FIELD_SYNC));
    // ASSERT_TRUE(strcmp(processor->getField(FIELD_SYNC).data.data, "A") == 0);
    SerialDataStamped data = processor->getField(FIELD_FRAME);
    int frameId = serial_library::convertFromCString<int>(data.data.data, data.data.numData);
    ASSERT_EQ(frameId, 0);

    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_1).data, serial_library::serialDataFromString("a", 1)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_2).data, serial_library::serialDataFromString("bcd", 3)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_3).data, serial_library::serialDataFromString("e", 1)));

    client.send(msg2, sizeof(msg2));
    processor->update(now);

    ASSERT_TRUE(processor->hasDataForField(FIELD_SYNC));
    data = processor->getField(FIELD_FRAME);
    frameId = serial_library::convertFromCString<int>(data.data.data, data.data.numData);
    ASSERT_EQ(frameId, 1);
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_2).data, serial_library::serialDataFromString("abe", 3)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_3).data, serial_library::serialDataFromString("d", 1)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_4).data, serial_library::serialDataFromString("c", 1)));
    
    client.send(msg3, sizeof(msg3));
    processor->update(now);

    ASSERT_TRUE(processor->hasDataForField(FIELD_SYNC));
    data = processor->getField(FIELD_FRAME);
    frameId = serial_library::convertFromCString<int>(data.data.data, data.data.numData);
    ASSERT_EQ(frameId, 2);
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_1).data, serial_library::serialDataFromString("c", 1)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_5).data, serial_library::serialDataFromString("be", 2)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_6).data, serial_library::serialDataFromString("dz", 2)));
}

TEST_F(Type2SerialProcessorTest, TestBasicRecvAndSendType2)
{
    //create another processor to recv
    std::unique_ptr<serial_library::LinuxSerialTransceiver> sender = std::make_unique<serial_library::LinuxSerialTransceiver>(
        homeDir() + "/virtualsp2",
        9600,
        1,
        0);
    
    const char syncValue[1] = {'A'};
    serial_library::SerialProcessor senderProcessor(
        std::move(sender),
        frameMap,
        Type1SerialFrames1::TYPE_1_FRAME_1,
        syncValue,
        sizeof(syncValue));

    //test sending frame 1
    Time now = curtime();
    senderProcessor.setField(TYPE_2_FIELD_1, serial_library::serialDataFromString("1", 1), now);
    senderProcessor.setField(TYPE_2_FIELD_2, serial_library::serialDataFromString("234", 3), now);
    senderProcessor.setField(TYPE_2_FIELD_3, serial_library::serialDataFromString("5", 1), now);

    senderProcessor.send(TYPE_2_FRAME_1);
    senderProcessor.send(TYPE_2_FRAME_1);
    
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_1, now));
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_2, now));
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_3, now));

    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_1).data, serial_library::serialDataFromString("1", 1)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_2).data, serial_library::serialDataFromString("234", 3)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_3).data, serial_library::serialDataFromString("5", 1)));

    //test sending frame 2
    now = curtime();
    senderProcessor.setField(TYPE_2_FIELD_2, serial_library::serialDataFromString("abc", 3), now);
    senderProcessor.setField(TYPE_2_FIELD_3, serial_library::serialDataFromString("d", 1), now);
    senderProcessor.setField(TYPE_2_FIELD_4, serial_library::serialDataFromString("E", 1), now);

    senderProcessor.send(TYPE_2_FRAME_2);
    senderProcessor.send(TYPE_2_FRAME_2);
    
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_2, now));
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_3, now));
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_4, now));

    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_2).data, serial_library::serialDataFromString("abc", 3)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_3).data, serial_library::serialDataFromString("d", 1)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_4).data, serial_library::serialDataFromString("E", 1)));

    //test sending frame 3
    now = curtime();
    senderProcessor.setField(TYPE_2_FIELD_5, serial_library::serialDataFromString("zy", 2), now);
    senderProcessor.setField(TYPE_2_FIELD_6, serial_library::serialDataFromString("xw", 2), now);
    senderProcessor.setField(TYPE_2_FIELD_1, serial_library::serialDataFromString("s", 1), now);

    senderProcessor.send(TYPE_2_FRAME_3);
    senderProcessor.send(TYPE_2_FRAME_3);
    
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_5, now));
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_6, now));
    ASSERT_TRUE(waitForFrame(TYPE_2_FIELD_1, now));

    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_5).data, serial_library::serialDataFromString("zy", 2)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_6).data, serial_library::serialDataFromString("xw", 2)));
    ASSERT_TRUE(compareSerialData(processor->getField(TYPE_2_FIELD_1).data, serial_library::serialDataFromString("s", 1)));
}

TEST(GenericType2SerialProcessorTest, TestConstructorSyncValueAssertions)
{
    std::unique_ptr<TestTransceiver> trans = std::make_unique<TestTransceiver>(true);
    SerialFramesMap goodFrames = {
        {Type2SerialFrames1::TYPE_2_FRAME_1,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                FIELD_SYNC,
                TYPE_2_FIELD_2
            }
        }
    };

    ASSERT_NO_THROW(serial_library::SerialProcessor proc(std::move(trans), goodFrames, Type2SerialFrames1::TYPE_2_FRAME_1, "ba", 2));
    
    SerialFramesMap nonContinuousSyncFrames = {
        {Type2SerialFrames1::TYPE_2_FRAME_1,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                TYPE_2_FIELD_2,
                FIELD_SYNC
            }
        }
    };

    trans = std::make_unique<TestTransceiver>(true);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(trans), nonContinuousSyncFrames, Type2SerialFrames1::TYPE_2_FRAME_1, "ab", 2),
        SerialLibraryException);
    
    //sync too long
    trans = std::make_unique<TestTransceiver>(true);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(trans), goodFrames, Type2SerialFrames1::TYPE_2_FRAME_1, "abc", 3),
        SerialLibraryException);
    
    //sync too short
    trans = std::make_unique<TestTransceiver>(true);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(trans), goodFrames, Type2SerialFrames1::TYPE_2_FRAME_1, "abc", 3),
        SerialLibraryException);
}

TEST(GenericType2SerialProcessorTest, TestConstructorFrameValueAssertions)
{
    std::unique_ptr<TestTransceiver> trans = std::make_unique<TestTransceiver>(true);

    //good frames
    SerialFramesMap frames = {
        {Type2SerialFrames1::TYPE_2_FRAME_1,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                FIELD_SYNC,
                TYPE_2_FIELD_2,
            }
        }
    };

    //control frame
    ASSERT_NO_THROW(
        serial_library::SerialProcessor proc(std::move(trans), frames, Type2SerialFrames1::TYPE_2_FRAME_1, "ab", 2));

    //bad default frame
    trans = std::make_unique<TestTransceiver>(true);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(trans), frames, Type2SerialFrames1::TYPE_2_FRAME_2, "ab", 2),
        SerialLibraryException);

    //misaligned frames
    SerialFramesMap misalignedSyncFrames = {
        {Type2SerialFrames1::TYPE_2_FRAME_1,
            {
                FIELD_SYNC,
                TYPE_2_FIELD_1,
                FIELD_FRAME,
                TYPE_2_FIELD_2,
            }
        },
        {Type2SerialFrames1::TYPE_2_FRAME_2,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                FIELD_FRAME,
                TYPE_2_FIELD_2,
            }
        }
    };

    trans = std::make_unique<TestTransceiver>(true);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(trans), misalignedSyncFrames, Type2SerialFrames1::TYPE_2_FRAME_1, "ab", 2),
        SerialLibraryException);

    SerialFramesMap misalignedFrameFrames = {
        {Type2SerialFrames1::TYPE_2_FRAME_1,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                FIELD_FRAME,
                TYPE_2_FIELD_2,
            }
        },
        {Type2SerialFrames1::TYPE_2_FRAME_2,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                TYPE_2_FIELD_2,
                FIELD_FRAME,
            }
        }
    };

    trans = std::make_unique<TestTransceiver>(true);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(trans), misalignedFrameFrames, Type2SerialFrames1::TYPE_2_FRAME_1, "ab", 2),
        SerialLibraryException);
    
    //multiple frames but no frame field
    SerialFramesMap missingFrameFieldFrames = {
        {Type2SerialFrames1::TYPE_2_FRAME_1,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                TYPE_2_FIELD_4,
                TYPE_2_FIELD_2,
            }
        },
        {Type2SerialFrames1::TYPE_2_FRAME_2,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                TYPE_2_FIELD_2,
                TYPE_2_FIELD_4,
            }
        }
    };

    trans = std::make_unique<TestTransceiver>(true);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(trans), missingFrameFieldFrames, Type2SerialFrames1::TYPE_2_FRAME_1, "ab", 2),
        SerialLibraryException);
}

TEST(GenericType2SerialProcessorTest, TestConstructorTransceiverInitFailed)
{
    std::unique_ptr<TestTransceiver> goodTrans = std::make_unique<TestTransceiver>(true);

    //good frames
    SerialFramesMap frames = {
        {Type2SerialFrames1::TYPE_2_FRAME_1,
            {
                TYPE_2_FIELD_1,
                FIELD_SYNC,
                FIELD_SYNC,
                TYPE_2_FIELD_2,
            }
        }
    };

    //control
    ASSERT_NO_THROW(
        serial_library::SerialProcessor proc(std::move(goodTrans), frames, Type2SerialFrames1::TYPE_2_FRAME_1, "ab", 2));
    
    //bad transceiver
    std::unique_ptr<TestTransceiver> badTrans = std::make_unique<TestTransceiver>(false);
    ASSERT_THROW(
        serial_library::SerialProcessor proc(std::move(badTrans), frames, Type2SerialFrames1::TYPE_2_FRAME_1, "ab", 2),
        SerialLibraryException);
}

#endif
