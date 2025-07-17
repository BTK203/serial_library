#include "serial_library/serial_library.hpp"

namespace serial_library
{
    SerialProcessor::SerialProcessor(
        std::unique_ptr<SerialTransceiver> transceivr,
        const SerialFramesMap& frames,
        const SerialFrameId& defaultFrame,
        const char syncValue[],
        size_t syncValueLen,
        bool switchEndianness,
        const SerialProcessorCallbacks& callbacks,
        const std::string& debugName)
     : failedOfLastTen(0),
       failedOfLastTenCounter(0),
       totalOfLastTenCounter(0),
       msgBufferCursorPos(0),
       syncValueLen(syncValueLen),
       frameMap(frames),
       defaultFrame(defaultFrame),
       switchEndianness(switchEndianness),
       callbacks(callbacks),
       debugName(debugName),
       valueMapResource(std::make_unique<SerialValuesMap>()),
       transceiverResource(std::move(transceivr))
    {
        SerialTransceiver::UniquePtr transceiver = transceiverResource.lockResource();
        SERIAL_LIB_ASSERT(transceiver->init(), "Transceiver initialization failed!");
        transceiverResource.unlockResource(std::move(transceiver));

        memcpy(this->syncValue, syncValue, syncValueLen);
        
        //check that the frames include a sync and checksums are 16 bits
        //TODO: must check all individual frames for a sync, not the frame ids
        for(auto it = frames.begin(); it != frames.end(); it++)
        {
            SerialFrame frame = it->second;
            if(findit(frame.begin(), frame.end(), FIELD_SYNC) == frame.end())
            {
                THROW_NON_FATAL_SERIAL_LIB_EXCEPTION(debugName + "No sync field provided in frame " + to_string(it->first) + " of the map.");
            }

            size_t numChecksumBytes = countit(frame.begin(), frame.end(), FIELD_CHECKSUM);
            if(numChecksumBytes != 0 && numChecksumBytes != sizeof(Checksum))
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION(debugName + "Support for non-" + to_string(sizeof(Checksum) * 8) + "-bit checksums is not implemented yet.");
            }
        }

        //add sync value
        SerialData syncData = serialDataFromString(syncValue, syncValueLen);
        SerialDataStamped syncDataStamped;
        syncDataStamped.data = syncData;
        std::unique_ptr<SerialValuesMap> values = valueMapResource.lockResource();
        values->insert( {FIELD_SYNC, syncDataStamped} );
        valueMapResource.unlockResource(std::move(values));
        
        SERIAL_LIB_ASSERT(frames.size() > 0, "Must have at least one frame");
        SERIAL_LIB_ASSERT(frames.find(defaultFrame) != frames.end(), "Default frame must be contained within frames");

        auto syncFieldIt = findit(frames.at(0).begin(), frames.at(0).end(), FIELD_SYNC);
        SERIAL_LIB_ASSERT(syncFieldIt != frames.at(0).end(), "Frame 0 does not contain a sync!");
        size_t syncFieldLoc = syncFieldIt - frames.at(0).begin();

        auto frameFieldIt = findit(frames.at(0).begin(), frames.at(0).end(), FIELD_FRAME);
        size_t frameFieldLoc = frameFieldIt - frames.at(0).begin();

        for(auto it = frames.begin(); it != frames.end(); it++)
        {
            auto iSyncIt = findit(it->second.begin(), it->second.end(), FIELD_SYNC);
            auto iFrameIt = findit(it->second.begin(), it->second.end(), FIELD_FRAME);

            // SERIAL_LIB_ASSERT((size_t) (iSyncIt - frames.at(i).begin()) == syncFieldLoc, "Sync fields not aligned!");
            SERIAL_LIB_ASSERT((size_t) (iFrameIt - it->second.begin()) == frameFieldLoc, "Frame fields not aligned!");
            SERIAL_LIB_ASSERT(findit(iFrameIt + 1, it->second.end(), FIELD_FRAME) == it->second.end(), "Large frame fields are not supported yet.");

            //check that the sync is continuous
            size_t syncFrameLen = 1;
            while(iSyncIt != it->second.end())
            {
                auto nextSyncFieldIt = findit(iSyncIt + 1, it->second.end(), FIELD_SYNC);
                if(nextSyncFieldIt != it->second.end())
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
        SerialTransceiver::UniquePtr transceiver = transceiverResource.lockResource();
        transceiver->deinit();
        transceiverResource.unlockResource(std::move(transceiver));
    }


    void SerialProcessor::update(const Time& now)
    {
        //TODO can probably rewrite method and use SERIAL_LIB_ASSERT
        SerialTransceiver::UniquePtr transceiver = transceiverResource.lockResource();
        if(!transceiver)
        {
            SERLIB_LOG_ERROR("%s: Transceiver is NULL for some reason", debugName.c_str());
            transceiverResource.unlockResource(std::move(transceiver));
            return;
        }

        size_t recvd = transceiver->recv(updateTransmissionBuffer, PROCESSOR_BUFFER_SIZE);
        SERLIB_LOG_DEBUG("%s: Received %d bytes", debugName.c_str(), recvd);

        transceiverResource.unlockResource(std::move(transceiver));

        if(recvd == 0)
        {
            return;
        }

        // append new contents to buffer
        size_t bytesToCopy = recvd;
        if(msgBufferCursorPos + recvd > PROCESSOR_BUFFER_SIZE)
        {
            bytesToCopy = PROCESSOR_BUFFER_SIZE - msgBufferCursorPos;
        }

        memcpy(&msgBuffer[msgBufferCursorPos], updateTransmissionBuffer, bytesToCopy);
        msgBufferCursorPos += bytesToCopy;

        char *syncLocation = nullptr;
        do
        {
            syncLocation = memstr(msgBuffer, msgBufferCursorPos, syncValue, syncValueLen);
            if(!syncLocation)
            {
                return;
            }

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
                SERLIB_LOG_DEBUG("%s: Dropping message because cursor position is less than the size of the default frame (not enough info to parse)", debugName.c_str());
                //we dont have enough information to parse the default frame for a frame id.
                break;
            }

            //parse for a frame id
            bool hasFrameToUse = true;
            if(frameMapSz > 1)
            {
                char frameIdBuf[MAX_DATA_BYTES] = {0};
                size_t bytes = extractFieldFromBuffer(msgStart, msgBufferCursorPos, frameToUse, FIELD_FRAME, frameIdBuf, MAX_DATA_BYTES);
                if(bytes > 0)
                {
                    SerialFrameId frameId = convertFromCString<SerialFrameId>(frameIdBuf, bytes);
                    SERLIB_LOG_DEBUG("Received frame with id %d", frameId);

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
                            SERLIB_LOG_DEBUG("%s: Dropping message because cursor position is less than the size of the selected frame", debugName.c_str());
                            break;
                        }
                    }
                }
            }

            bool msgPassesUserTest = true;
            if(set<SerialFieldId>(frameToUse.begin(), frameToUse.end()).count(FIELD_CHECKSUM) > 0)
            {
                //grab checksum out of message
                size_t csLen = extractFieldFromBuffer(msgStart, msgBufferCursorPos, frameToUse, FIELD_CHECKSUM, fieldBuf, sizeof(fieldBuf));
                Checksum checksum = convertFromCString<Checksum>(fieldBuf, csLen);

                //remove checksum from message
                memcpy(updateChecksumlessBuffer, msgStart, msgBufferCursorPos);
                deleteChecksumFromBuffer(updateChecksumlessBuffer, msgBufferCursorPos, frameToUse);

                //pass edited message to user function to evaluate checksum
                msgPassesUserTest = callbacks.checksumEvaluationFunc(updateChecksumlessBuffer, frameToUse.size() - sizeof(Checksum), checksum);
            }

            if(msgStartOffsetFromSync <= syncOffsetFromBuffer && msgPassesUserTest && hasFrameToUse)
            {
                SERLIB_LOG_DEBUG("%s: Processing message with frame because it passed all checks", debugName.c_str());

                //iterate through frame and find all unknown fields
                for(auto it = frameToUse.begin(); it != frameToUse.end(); it++)
                {
                    std::unique_ptr<SerialValuesMap> values = valueMapResource.lockResource();
                    if(values->find(*it) == values->end())
                    {
                        values->insert({ *it, SerialDataStamped() });
                    }

                    valueMapResource.unlockResource(std::move(values));
                }

                //iterate through known fields and update their values from message
                std::unique_ptr<SerialValuesMap> values = valueMapResource.lockResource();
                SerialValuesMap msgValueMap;
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

                        msgValueMap.insert({ it->first, it->second });
                    }
                }

                valueMapResource.unlockResource(std::move(values));
                
                //call new message function
                callbacks.newMessageCallback(msgValueMap);

                //set lastmsg timestamp
                lastMsgRecvTime = now;
            } else
            {
                //message bad. dont remove like normal, just delete through the sync character
                SERLIB_LOG_DEBUG("%s: Skipping message because it failed some checks", debugName.c_str());
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
        std::unique_ptr<SerialValuesMap> values = valueMapResource.lockResource();
        bool hasData = values->find(field) != values->end();
        valueMapResource.unlockResource(std::move(values));
        return hasData;
    }


    Time SerialProcessor::getLastMsgRecvTime(void) const
    {
        return lastMsgRecvTime;
    }
    
    
    SerialDataStamped SerialProcessor::getField(SerialFieldId field)
    {
        std::unique_ptr<SerialValuesMap> values = valueMapResource.lockResource();
        SerialDataStamped data;
        if(values->find(field) != values->end())
        {
            data = values->at(field);

            if(switchEndianness)
            {
                data = switchStampedDataEndianness(data);
            }
        }

        valueMapResource.unlockResource(std::move(values));
        return data;
    }


    Time SerialProcessor::getFieldTimestamp(SerialFieldId id)
    {
        return getField(id).timestamp;
    }
    
    
    void SerialProcessor::setField(SerialFieldId field, SerialData data, const Time& now)
    {
        std::unique_ptr<SerialValuesMap> values = valueMapResource.lockResource();
        if(values->find(field) == values->end())
        {
            //no field currently set, so add one
            values->insert({ field, SerialDataStamped() });
        }

        SerialDataStamped stampedData;
        stampedData.data = data;
        stampedData.timestamp = now;
        values->at(field) = stampedData;
        valueMapResource.unlockResource(std::move(values));
    }
    
    
    void SerialProcessor::send(const SerialFrameId& frameId)
    {
        SERLIB_LOG_DEBUG("%s sending frame %d", debugName.c_str(), frameId);

        if(frameMap.find(frameId) == frameMap.end())
        {
            THROW_NON_FATAL_SERIAL_LIB_EXCEPTION(debugName + "Cannot send message with unknown frame id " + to_string(frameId));
        }

        memset(sendTransmissionBuffer, 0, sizeof(sendTransmissionBuffer));
        SerialFrame frame = frameMap.at(frameId);

        //loop through minimal set of frames and pack each frame into the transmission buffer
        set<SerialFieldId> frameSet(frame.begin(), frame.end());
        for(auto fieldIt = frameSet.begin(); fieldIt != frameSet.end(); fieldIt++)
        {
            SerialData dataToInsert;
            std::unique_ptr<SerialValuesMap> values = valueMapResource.lockResource();
            
            //couldnt find the field included in the frame. Check if the frame is a builtin type
            if(*fieldIt == FIELD_SYNC)
            {
                memcpy(dataToInsert.data, syncValue, syncValueLen);
                dataToInsert.numData = syncValueLen;
            } else if(*fieldIt == FIELD_FRAME)
            {
                dataToInsert.numData = convertToCString<SerialFrameId>(frameId, dataToInsert.data, MAX_DATA_BYTES);
            } else if(*fieldIt == FIELD_CHECKSUM)
            {
                valueMapResource.unlockResource(std::move(values));
                continue;
            } else if(values->find(*fieldIt) != values->end())
            {
                dataToInsert = values->at(*fieldIt).data;
            } else
            {
                //if it is a custom type, throw exception because it is undefined
                valueMapResource.unlockResource(std::move(values));
                THROW_NON_FATAL_SERIAL_LIB_EXCEPTION(debugName + "Cannot send serial frame because it is missing field " + to_string(*fieldIt));
            }

            insertFieldToBuffer(
                sendTransmissionBuffer, 
                sizeof(sendTransmissionBuffer), 
                frame, 
                *fieldIt, 
                dataToInsert.data,
                dataToInsert.numData);
            
            valueMapResource.unlockResource(std::move(values));
        }

        //now remove checksum bytes from message, compute checksum, and add it to the message
        if(frameSet.count(FIELD_CHECKSUM) > 0)
        {
            //fill checksum buffer with contents of transmission buffer
            memcpy(sendChecksumlessBuffer, sendTransmissionBuffer, sizeof(sendTransmissionBuffer));
            deleteChecksumFromBuffer(sendChecksumlessBuffer, sizeof(sendChecksumlessBuffer), frame);
            Checksum checksum = callbacks.checksumGenerationFunc(sendChecksumlessBuffer, frame.size() - sizeof(Checksum));
            
            //fill checksum buffer with encoded checksum
            size_t checksumLen = convertToCString<Checksum>(checksum, sendChecksumlessBuffer, sizeof(sendChecksumlessBuffer));

            insertFieldToBuffer(
                sendTransmissionBuffer,
                sizeof(sendTransmissionBuffer),
                frame,
                FIELD_CHECKSUM,
                sendChecksumlessBuffer,
                checksumLen);
        }

        SerialTransceiver::UniquePtr transceiver = transceiverResource.lockResource();

        if(!transceiver)
        {
            SERLIB_LOG_ERROR("%s: Transceiver is NULL for some reason", debugName.c_str());
            transceiverResource.unlockResource(std::move(transceiver));
            return;
        }

        transceiver->send(sendTransmissionBuffer, frame.size());
        transceiverResource.unlockResource(std::move(transceiver));
    }

    unsigned short SerialProcessor::failedOfLastTenMessages()
    {
        return failedOfLastTen;
    }
}
