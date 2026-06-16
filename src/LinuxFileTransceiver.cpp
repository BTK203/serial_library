#include "serial_library/serial_library.hpp"

#if defined(USE_LINUX)

namespace serial_library
{
    LinuxFileTransceiver::LinuxFileTransceiver(const std::string& fileName, int mode)
    : fileName(fileName),
      mode(mode)
    { }


    bool LinuxFileTransceiver::init(void)
    {
        file = open(fileName.c_str(), mode);
        if(file < 0)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not open file " + string(fileName.c_str()) + ": " + string(strerror(errno)));
            initialized = false;
            return false;
        }

        initialized = true;
        return true;
    }


    void LinuxFileTransceiver::send(const char *data, size_t numData)
    {
        if(initialized)
        {
            ssize_t ret = write(fileHandle(), data, numData);

            if(ret < 0)
            {
                SERLIB_LOG_ERROR("Failed to send: %s", strerror(errno));
            }
        }
    }


    size_t LinuxFileTransceiver::recv(char *data, size_t numData)
    {
        memset(data, 0, numData);
        if(initialized)
        {
            ssize_t ret = read(fileHandle(), data, numData);
            if(ret < 0)
            {
                return 0;
            }

            return ret;
        }

        return 0;
    }


    void LinuxFileTransceiver::deinit(void)
    {
        if(initialized)
        {
            close(fileHandle());
            initialized = false;
        }
    }


    int LinuxFileTransceiver::fileHandle() const
    {
        return file;
    }
}

#endif
