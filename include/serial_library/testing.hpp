#pragma once

#include "serial_library/serial_library.hpp"
#include <gtest/gtest.h>


class TransceiverTest : public ::testing::Test
{
    protected:
    bool compareSerialData(const serial_library::SerialData& data1, const serial_library::SerialData& data2);
};

#if defined(USE_LINUX)

class LinuxTransceiverTest : public TransceiverTest
{
    protected:
    void SetUp() override;
    void TearDown() override;
    std::string homeDir();

    private:
    std::string home;
    pid_t socatProc;
};

#endif

#if defined(USE_WINDOWS)

class WindowsTransceiverTest : public TransceiverTest
{ };

#endif

class SerialProcessorTest : public TransceiverTest
{
    protected:
    void SetUp() override;
    bool waitForFrame(serial_library::SerialFrameId id, const serial_library::Time& now);
    serial_library::SerialProcessor::SharedPtr processor;
    std::unique_ptr<serial_library::IntraProcessTransceiver> 
        transceiver,
        client;
};



//
// type1 processor test stuff
//
enum Type1SerialFrames1
{
    TYPE_1_FRAME_1
};

enum Type1SerialFrame1Fields
{
    TYPE_1_FRAME_1_FIELD_1,
    TYPE_1_FRAME_1_FIELD_2,
    TYPE_1_FRAME_1_FIELD_3
};

class Type1SerialProcessorTest : public SerialProcessorTest
{
    protected:
    void SetUp() override;

    serial_library::SerialFramesMap frameMap;
};

enum Type2SerialFrames1
{
    TYPE_2_FRAME_1,
    TYPE_2_FRAME_2,
    TYPE_2_FRAME_3,
    TYPE_2_CHKSM_FRAME
};

enum Type2SerialFields
{
    TYPE_2_FIELD_1,
    TYPE_2_FIELD_2,
    TYPE_2_FIELD_3,
    TYPE_2_FIELD_4,
    TYPE_2_FIELD_5,
    TYPE_2_FIELD_6
};


static serial_library::SerialFramesMap TYPE_2_FRAME_MAP = {
        {Type2SerialFrames1::TYPE_2_FRAME_1, 
            {
                TYPE_2_FIELD_1,
                FIELD_FRAME,
                TYPE_2_FIELD_2,
                TYPE_2_FIELD_2,
                FIELD_SYNC,
                TYPE_2_FIELD_2,
                TYPE_2_FIELD_3
            }
        },
        {Type2SerialFrames1::TYPE_2_FRAME_2, 
            {
                TYPE_2_FIELD_2,
                FIELD_FRAME,
                TYPE_2_FIELD_2,
                TYPE_2_FIELD_4,
                FIELD_SYNC,
                TYPE_2_FIELD_3,
                TYPE_2_FIELD_2,
            }
        },
        {Type2SerialFrames1::TYPE_2_FRAME_3, 
            {
                TYPE_2_FIELD_5,
                FIELD_FRAME,
                TYPE_2_FIELD_1,
                TYPE_2_FIELD_6,
                FIELD_SYNC,
                TYPE_2_FIELD_5,
                TYPE_2_FIELD_6
            }
        },
        {Type2SerialFrames1::TYPE_2_CHKSM_FRAME, 
            {
                TYPE_2_FIELD_5,
                FIELD_FRAME,
                FIELD_CHECKSUM,
                FIELD_CHECKSUM,
                FIELD_SYNC,
                TYPE_2_FIELD_1,
                TYPE_2_FIELD_5
            }
        }
    };

class Type2SerialProcessorTest : public SerialProcessorTest
{
    protected:
    void SetUp() override;
};


class CallbacksTest : public SerialProcessorTest
{
    protected:
    void SetUp() override;

    serial_library::SerialValuesMap lastReceivedSerialFrames() const;
    serial_library::Checksum lastGeneratedChecksum() const;
    serial_library::SerialProcessorCallbacks getCallbacks();

    private:
    void newMsgCallback(const serial_library::SerialValuesMap& frames);
    bool checksumEvaluator(const char *msg, size_t len, serial_library::Checksum checksum);
    serial_library::Checksum checksumGenerator(const char* msg, size_t len);

    serial_library::SerialValuesMap lastReceivedMap;
    serial_library::Checksum lastChksm;
};
