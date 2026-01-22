#include "serial_library/serial_library.hpp"

#if defined(USE_WINDOWS)

// Windows serial port implementation: https://aticleworld.com/serial-port-programming-using-win32-api/

namespace serial_library
{

    WindowsSerialTransceiver::WindowsSerialTransceiver(
            const std::string& name,
            DWORD baud,
            DWORD readTimeout,
            DWORD mode,
            DWORD bitsPerByte,
            DWORD stopBits,
            DWORD parityBit)
    : _portName(name),
      _baud(baud),
      _readTimeout(readTimeout),
      _mode(mode),
      _bitsPerByte(bitsPerByte),
      _stopBits(stopBits),
      _parityBit(parityBit),
      _initialized(false)
    { }

    bool WindowsSerialTransceiver::init(void)
    {
        // open the port
        _port = CreateFileA(_portName.c_str(), _mode, 0, NULL, OPEN_EXISTING, 0, NULL);

        if(_port == INVALID_HANDLE_VALUE)
        {
            _initialized = false;
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not open COM port " + _portName + ": " + getWindowsMsgAsString(GetLastError()));
        }

        // setting parameters
        DCB params = {0};
        bool status;

        params.DCBlength = sizeof(params);
        status = GetCommState(_port, &params);

        if(!status)
        {
            _initialized = false;
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not obtain COM port params for modification for port " + _portName + ": " + getWindowsMsgAsString(GetLastError()));
        }

        params.BaudRate = _baud;
        params.ByteSize = _bitsPerByte;
        params.StopBits = _stopBits;
        params.Parity = _parityBit;

        status = SetCommState(_port, &params);

        if(!status)
        {
            _initialized = false;
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not set params for port " + _portName + ": " + getWindowsMsgAsString(GetLastError()));
        }

        // setting timeout information
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = _readTimeout;
        timeouts.ReadTotalTimeoutMultiplier = 1;
        timeouts.ReadTotalTimeoutConstant = 0;

        status = SetCommTimeouts(_port, &timeouts);

        if(!status)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not set timeout information for port " + _portName + ": " + getWindowsMsgAsString(GetLastError()));
        }

        // if we got here then we done!
        _initialized = true;
        return _initialized;
    }


    void WindowsSerialTransceiver::send(const char *data, size_t numData)
    {
        bool status;
        DWORD written;

        if(_initialized)
        {
            status = WriteFile(_port, data, numData, &written, NULL);

            if(!status)
            {
                SERLIB_LOG_ERROR("Failed to send out port %s: %s", _portName + ": " + getWindowsMsgAsString(GetLastError()));
            }
        }
    }


    size_t WindowsSerialTransceiver::recv(char *data, size_t numData)
    {
        bool status;
        DWORD bsRead = 0;

        if(_initialized)
        {
            ReadFile(_port, data, numData, &bsRead, NULL);
        }

        return bsRead;
    }


    void WindowsSerialTransceiver::deinit(void)
    {
        bool status = CloseHandle(_port);

        if(!status)
        {
            // dont throw an exception, just log. the damage is already done (the port may be inaccessible now), no need to halt the program because of it
            SERLIB_LOG_ERROR("Unable to close port %s: %s", _portName.c_str(), getWindowsMsgAsString(GetLastError()).c_str());
        }
    }
}

#endif
