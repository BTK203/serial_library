#include "serial_library/serial_library.hpp"

namespace serial_library
{
    char *memstr(const char *haystack, size_t numHaystack, const char *needle, size_t numNeedle)
    {
        char *search = (char*) haystack;

        // need this because size_t is unsigned
        if(numNeedle <= numHaystack)
        {
            for(size_t i = 0; i <= numHaystack - numNeedle; i++)
            {
                if(memcmp(search, needle, numNeedle) == 0)
                {
                    return search;
                }

                //move to next character
                search = &search[1];
            }
        }

        return nullptr;
    }


    size_t extractFieldFromBuffer(const char *src, size_t srcLen, SerialFrame frame, SerialFieldId field, char *dst, size_t dstLen)
    {
        auto it = frame.begin();
        size_t nextUnusedCharacter = 0; //for dst

        while(it != frame.end() && nextUnusedCharacter < dstLen)
        {
            //find next location of the field in the frame
            it = find(it, frame.end(), field);
            if(it == frame.end())
            {
                break;
            }

            size_t idx = it - frame.begin();
            if(idx < srcLen)
            {
                dst[nextUnusedCharacter] = src[idx];
            }

            nextUnusedCharacter++;
            it++;
        }

        return nextUnusedCharacter;
    }

    void insertFieldToBuffer(char *dst, size_t dstLen, SerialFrame frame, SerialFieldId field, const char *src, size_t srcLen)
    {
        auto it = frame.begin();
        size_t nextUnusedCharacter = 0; //for src

        while(it != frame.end() && nextUnusedCharacter < srcLen)
        {
            //find next location of the field in the frame
            it = find(it, frame.end(), field);
            if(it == frame.end())
            {
                break;
            }

            size_t idx = it - frame.begin();

            //we know where to put the next character from src; now put it there
            if(idx < dstLen)
            {
                dst[idx] = src[nextUnusedCharacter];
                nextUnusedCharacter++;
            }

            it++;
        }
    }


    size_t deleteFieldAndShiftBuffer(char *buf, size_t bufLen, SerialFrame frame, SerialFieldId field)
    {
        auto it = frame.begin();
        size_t newBufLen = bufLen;

        if(bufLen != frame.size())
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Buffer length is not the same as the frame length");
        }

        while(it != frame.end())
        {
            //find next location of the field in the frame
            it = find(it, frame.end(), field);
            if(it == frame.end())
            {
                break;
            }

            //the " + newBufLen - bufLen" makes algorithm robust to changes in buffer caused by removing characters
            size_t idx = (it - frame.begin()) + newBufLen - bufLen;
            memmove(&buf[idx], &buf[idx + 1], bufLen - idx);
            newBufLen--;

            it++;
        }

        return newBufLen;
    }

    
    size_t deleteChecksumFromBuffer(char *buf, size_t bufLen, SerialFrame frame)
    {
        return deleteFieldAndShiftBuffer(buf, bufLen, frame, FIELD_CHECKSUM);
    }


    SerialData serialDataFromString(const char* str, size_t numData)
    {
        SerialData data;
        strcpy(data.data, str);
        data.numData = numData;
        return data;
    }


    SerialDataStamped serialDataStampedFromString(const char *str, size_t numData, const Time& stamp)
    {
        SerialData data = serialDataFromString(str, numData);
        SerialDataStamped stamped;
        stamped.data = data;
        stamped.timestamp = stamp;
    }


    size_t countInString(const std::string& s, char c)
    {
        size_t n = 0;
        for(char o : s)
        {
            if(o == c)
            {
                n++;
            }
        }

        return n;
    }


    SerialFrame assembleSerialFrame(const std::vector<SerialFrameComponent>& components)
    {
        SerialFrame frame;
        
        for(size_t i = 0; i < components.size(); i++)
        {
            SerialFrameComponent comp = components.at(i);
            SerialFrameId id = comp.first;
            for(size_t j = 0; j < comp.second; j++)
            {
                frame.push_back(id);
            }
        }

        return frame;
    }


    SerialFrame normalizeSerialFrame(const SerialFrame& frame)
    {
        auto syncIt = find(frame.begin(), frame.end(), FIELD_SYNC);

        //start with frame from sync field to end
        SerialFrame normalizedFrame(syncIt, frame.end());

        //add frame begin to sync field
        normalizedFrame.insert(normalizedFrame.end(),frame.begin(), syncIt);

        return normalizedFrame;
    }


    SerialFramesMap normalizeSerialFramesMap(const SerialFramesMap& map)
    {
        SerialFramesMap normalizedFrameMap;
        
        //compute normalized frames and add them to map
        for(auto pair : map)
        {
            SerialFrame normalizedFrame = normalizeSerialFrame(pair.second);
            normalizedFrameMap.insert({ pair.first, normalizedFrame });
        }

        return normalizedFrameMap;
    }
}


// SerialData& operator=(const SerialData& other)
// {
    
// }
