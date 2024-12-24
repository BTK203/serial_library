#pragma once

#include "serial_library/serial_library.hpp"
#include <gtest/gtest.h>

#if defined(USE_LINUX)

class LinuxTransceiverTest : public ::testing::Test
{
    protected:
    void SetUp() override;
    void TearDown() override;
    bool compareSerialData(const serial_library::SerialData& data1, const serial_library::SerialData& data2);
    std::string homeDir();

    private:
    std::string home;
    pid_t socatProc;
};

class LinuxSerialProcessorTest : public LinuxTransceiverTest
{
    protected:
    bool waitForFrame(serial_library::SerialFrameId id, serial_library::Time startTime);
    serial_library::SerialProcessor::SharedPtr processor;
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

class Type1SerialProcessorTest : public LinuxSerialProcessorTest
{
    protected:
    void SetUp() override;
    void TearDown() override;

    serial_library::SerialFramesMap frameMap;
    std::unique_ptr<serial_library::LinuxSerialTransceiver> transceiver;
    serial_library::SerialProcessor::SharedPtr processor;
};

enum Type2SerialFrames1
{
    TYPE_2_FRAME_1,
    TYPE_2_FRAME_2,
    TYPE_2_FRAME_3
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
        }
    };

class Type2SerialProcessorTest : public LinuxSerialProcessorTest
{
    protected:
    void SetUp() override;
    void TearDown() override;

    private:
    std::unique_ptr<serial_library::LinuxSerialTransceiver> transceiver;
};


class CallbacksTest : public LinuxSerialProcessorTest
{
    protected:
    void SetUp() override;
    void TearDown() override;

    serial_library::SerialValuesMap lastReceivedSerialFrames() const;

    private:
    void newMsgCallback(const serial_library::SerialValuesMap& frames);
    bool checksumEvaluator(const char *msg, size_t len, serial_library::checksum_t checksum);
    uint16_t checksumGenerator(const char* msg, size_t len);

    serial_library::SerialValuesMap lastReceivedMap;
    std::unique_ptr<serial_library::LinuxSerialTransceiver> transceiver;
};

#endif
