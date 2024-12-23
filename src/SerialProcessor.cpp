#include "serial_library/serial_library.hpp"

namespace serial_library
{
    SerialProcessor::SerialProcessor(std::unique_ptr<SerialTransceiver> transceiver, const SerialFramesMap& frames, const SerialFrameId& defaultFrame, const char syncValue[], size_t syncValueLen, CheckFunc checker)
     : transceiver(std::move(transceiver)),
       failedOfLastTen(0),
       failedOfLastTenCounter(0),
       totalOfLastTenCounter(0),
       msgBufferCursorPos(0),
       syncValueLen(syncValueLen),
       frameMap(frames),
       defaultFrame(defaultFrame),
       checker(checker),
       newMsgFunc(nullptr),
       valueMap(std::make_unique<SerialValuesMap>())
    {
        SERIAL_LIB_ASSERT(this->transceiver->init(), "Transceiver initialization failed!");

        memcpy(this->syncValue, syncValue, syncValueLen);
        
        //check that the frames include a sync
        //TODO: must check all individual frames for a sync, not the frame ids
        for(size_t i = 0; i < frames.size(); i++)
        {
            SerialFrame frame = frames.at(i);
            if(findit(frame.begin(), frame.end(), FIELD_SYNC) == frame.end())
            {
                THROW_NON_FATAL_SERIAL_LIB_EXCEPTION("No sync field provided in frame " + to_string(i) + " of the map.");
            }
        }

        //add sync value
        SerialData syncData = serialDataFromString(syncValue, syncValueLen);
        SerialDataStamped syncDataStamped;
        syncDataStamped.data = syncData;
        std::unique_ptr<SerialValuesMap> values = valueMap.lockResource();
        values->insert( {FIELD_SYNC, syncDataStamped} );
        valueMap.unlockResource(std::move(values));
        
        SERIAL_LIB_ASSERT(frames.size() > 0, "Must have at least one frame");
        SERIAL_LIB_ASSERT(frames.find(defaultFrame) != frames.end(), "Default frame must be contained within frames");

        auto syncFieldIt = findit(frames.at(0).begin(), frames.at(0).end(), FIELD_SYNC);
        SERIAL_LIB_ASSERT(syncFieldIt != frames.at(0).end(), "Frame 0 does not contain a sync!");
        size_t syncFieldLoc = syncFieldIt - frames.at(0).begin();

        auto frameFieldIt = findit(frames.at(0).begin(), frames.at(0).end(), FIELD_FRAME);
        size_t frameFieldLoc = frameFieldIt - frames.at(0).begin();

        for(size_t i = 0; i < frames.size(); i++)
        {
            auto iSyncIt = findit(frames.at(i).begin(), frames.at(i).end(), FIELD_SYNC);
            auto iFrameIt = findit(frames.at(i).begin(), frames.at(i).end(), FIELD_FRAME);

            SERIAL_LIB_ASSERT((size_t) (iSyncIt - frames.at(i).begin()) == syncFieldLoc, "Sync fields not aligned!");
            SERIAL_LIB_ASSERT((size_t) (iFrameIt - frames.at(i).begin()) == frameFieldLoc, "Frame fields not aligned!");
            SERIAL_LIB_ASSERT(findit(iFrameIt + 1, frames.at(i).end(), FIELD_FRAME) == frames.at(i).end(), "Large frame fields are not supported yet.");

            //check that the sync is continuous
            size_t syncFrameLen = 1;
            while(iSyncIt != frames.at(i).end())
            {
                auto nextSyncFieldIt = findit(iSyncIt + 1, frames.at(i).end(), FIELD_SYNC);
                if(nextSyncFieldIt != frames.at(i).end())
                {
                    SERIAL_LIB_ASSERT(nextSyncFieldIt - iSyncIt == 1, "Sync frame is not continuous!");
                    syncFrameLen++;
                }

                iSyncIt = nextSyncFieldIt;
            }

            SERIAL_LIB_ASSERT(syncFrameLen == syncValueLen, "Sync field length is not equal to the sync value length!");
        }
    }


    SerialProcessor::~SerialProcessor()
    {
        transceiver->deinit();
    }


    void SerialProcessor::setNewMsgCallback(const NewMsgFunc& func)
    {
        this->newMsgFunc = func;
    }


    void SerialProcessor::update(const Time& now)
    {
        //TODO can probably rewrite method and use SERIAL_LIB_ASSERT
        size_t recvd = transceiver->recv(transmissionBuffer, PROCESSOR_BUFFER_SIZE);
        if(recvd == 0)
        {
            return;
        }

        // append new contents to buffer
        size_t bytesToCopy = recvd;
        if(msgBufferCursorPos + recvd > PROCESSOR_BUFFER_SIZE)
        {
            bytesToCopy = -msgBufferCursorPos + PROCESSOR_BUFFER_SIZE;
        }

        memcpy(&msgBuffer[msgBufferCursorPos], transmissionBuffer, bytesToCopy);
        msgBufferCursorPos += bytesToCopy;

        char *syncLocation = nullptr;
        do
        {
            // find sync values. having two means we have one potential messages
            syncLocation = memstr(msgBuffer, msgBufferCursorPos, syncValue, syncValueLen);
            if(!syncLocation)
            {
                return;
            }

            size_t msgSz = msgBufferCursorPos + (size_t) msgBuffer - (size_t) syncLocation;

            // can process message here. first need to figure out the frame to use.
            // if there was only one frame provided, this is easy. otherwise, need to look for indication in the message
            SerialFrame frameToUse = frameMap.at(defaultFrame);
            size_t
                frameMapSz = frameMap.size(),
                frameSz = frameToUse.size();

            //determine the message string based on the sync location. then process it

            int
                msgStartOffsetFromSync = findit(frameToUse.begin(), frameToUse.end(), FIELD_SYNC) - frameToUse.begin(),
                syncOffsetFromBuffer = syncLocation - msgBuffer;

            char 
                *msgStart = syncLocation,
                *msgEnd = msgBuffer + msgBufferCursorPos;

            totalOfLastTenCounter++;

            msgStart = syncLocation - msgStartOffsetFromSync;
            msgEnd = msgStart + frameSz;

            //check that we can parse for a frame id
            if(msgBufferCursorPos < frameSz)
            {
                //we dont have enough information to parse the default frame for a frame id.
                break;
            }

            //parse for a frame id
            bool hasFrameToUse = true;
            if(frameMapSz > 1)
            {
                char frameIdBuf[MAX_DATA_BYTES] = {0};
                size_t bytes = extractFieldFromBuffer(msgStart, msgSz, frameToUse, FIELD_FRAME, frameIdBuf, MAX_DATA_BYTES);
                if(bytes > 0)
                {
                    SerialFrameId frameId = convertFromCString<SerialFrameId>(frameIdBuf, bytes);

                    if(frameMap.find(frameId) == frameMap.end())
                    {
                        hasFrameToUse = false;
                    } else
                    {
                        frameToUse = frameMap.at(frameId);
                        
                        //check if the frame is parsable
                        frameSz = frameToUse.size();
                        if(msgBufferCursorPos < frameSz)
                        {
                            //we dont have enough information to parse this frame
                            continue;
                        }
                    }
                }
            }

            bool msgPassesUserTest = checker(msgStart, frameToUse);
            if(msgStartOffsetFromSync <= syncOffsetFromBuffer && msgPassesUserTest && hasFrameToUse)
            {
                //iterate through frame and find all unknown fields
                for(auto it = frameToUse.begin(); it != frameToUse.end(); it++)
                {
                    std::unique_ptr<SerialValuesMap> values = valueMap.lockResource();
                    if(values->find(*it) == values->end())
                    {
                        values->insert({ *it, SerialDataStamped() });
                    }

                    valueMap.unlockResource(std::move(values));
                }

                //iterate through known fields and update their values from message
                std::unique_ptr<SerialValuesMap> values = valueMap.lockResource();
                for(auto it = values->begin(); it != values->end(); it++)
                {
                    memset(fieldBuf, 0, sizeof(fieldBuf));
                    SerialFieldId field = it->first;
                    size_t extracted = extractFieldFromBuffer(msgStart, frameSz, frameToUse, field, fieldBuf, sizeof(fieldBuf));
                    if(extracted > 0)
                    {
                        SerialDataStamped serialData;
                        serialData.timestamp = now;
                        memcpy(serialData.data.data, fieldBuf, extracted);
                        serialData.data.numData = extracted;

                        it->second = serialData;
                    }
                }

                valueMap.unlockResource(std::move(values));
                
                //call new message function
                if(newMsgFunc)
                {
                    newMsgFunc();
                }
            } else
            {
                //message bad. dont remove like normal, just delete through the sync character
                msgEnd = syncLocation + 1;
                failedOfLastTenCounter++;
            }

            if(totalOfLastTenCounter >= 10)
            {
                failedOfLastTen = failedOfLastTenCounter;
                failedOfLastTenCounter = 0;
                totalOfLastTenCounter = 0;
            }

            //remove message from the buffer
            memmove(msgBuffer, msgEnd, PROCESSOR_BUFFER_SIZE - msgBufferCursorPos);
            size_t amountRemoved = msgEnd - msgBuffer;

            if(amountRemoved < msgBufferCursorPos)
            {
                msgBufferCursorPos -= amountRemoved;
            } else
            {
                msgBufferCursorPos = 0;
            }

        } while(syncLocation);
    }
    
    
    bool SerialProcessor::hasDataForField(SerialFieldId field)
    {
        std::unique_ptr<SerialValuesMap> values = valueMap.lockResource();
        bool hasData = values->find(field) != values->end();
        valueMap.unlockResource(std::move(values));
        return hasData;
    }
    
    
    SerialDataStamped SerialProcessor::getField(SerialFieldId field)
    {
        std::unique_ptr<SerialValuesMap> values = valueMap.lockResource();
        SerialDataStamped data;
        if(values->find(field) != values->end())
        {
            data = values->at(field);
        }

        valueMap.unlockResource(std::move(values));
        return data;
    }
    
    
    void SerialProcessor::setField(SerialFieldId field, SerialData data, const Time& now)
    {
        std::unique_ptr<SerialValuesMap> values = valueMap.lockResource();
        if(values->find(field) == values->end())
        {
            //no field currently set, so add one
            values->insert({ field, SerialDataStamped() });
        }

        SerialDataStamped stampedData;
        stampedData.data = data;
        stampedData.timestamp = now;
        values->at(field) = stampedData;
        valueMap.unlockResource(std::move(values));
    }
    
    
    void SerialProcessor::send(SerialFrameId frameId)
    {
        if(frameMap.find(frameId) == frameMap.end())
        {
            THROW_NON_FATAL_SERIAL_LIB_EXCEPTION("Cannot send message with unknown frame id " + to_string(frameId));
        }

        SerialFrame frame = frameMap.at(frameId);

        //loop through minimal set of frames and pack each frame into the transmission buffer
        set<SerialFieldId> frameSet(frame.begin(), frame.end());
        for(auto fieldIt = frameSet.begin(); fieldIt != frameSet.end(); fieldIt++)
        {
            SerialData dataToInsert;
            std::unique_ptr<SerialValuesMap> values = valueMap.lockResource();
            if(values->find(*fieldIt) != values->end())
            {
                dataToInsert = values->at(*fieldIt).data;
            } else
            {
                //couldnt find the field included in the frame. Check if the frame is a builtin type
                if(*fieldIt == FIELD_SYNC)
                {
                    memcpy(dataToInsert.data, syncValue, syncValueLen);
                    dataToInsert.numData = syncValueLen;
                } else if(*fieldIt == FIELD_FRAME)
                {
                    dataToInsert.numData = convertToCString<SerialFrameId>(frameId, dataToInsert.data, MAX_DATA_BYTES);
                } else
                {
                    //if it is a custom type, throw exception because it is undefined
                    valueMap.unlockResource(std::move(values));
                    THROW_NON_FATAL_SERIAL_LIB_EXCEPTION("Cannot send serial frame because it is missing field " + to_string(*fieldIt));
                }
            }

            insertFieldToBuffer(
                transmissionBuffer, 
                sizeof(transmissionBuffer), 
                frame, 
                *fieldIt, 
                dataToInsert.data,
                dataToInsert.numData);
            
            valueMap.unlockResource(std::move(values));
        }

        transceiver->send(transmissionBuffer, frame.size());
    }

    unsigned short SerialProcessor::failedOfLastTenMessages()
    {
        return failedOfLastTen;
    }
}
