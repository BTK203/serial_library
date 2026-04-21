#include "serial_library/serial_library.hpp"

#if defined(USE_LINUX)

// linux socketpair implementation: https://man7.org/linux/man-pages/man2/socketpair.2.html

namespace serial_library
{
    LinuxSocketpairTransceiver::LinuxSocketpairTransceiver(int domain, int type, int protocol, bool blocking)
    : _domain(domain),
      _type(type),
      _protocol(protocol),
      _isParent(true),
      _blocking(blocking),
      _parentFd(-1),
      _childFd(-1),
      _initialized(false)
    { }


    LinuxSocketpairTransceiver::LinuxSocketpairTransceiver(int childFd)
    : _domain(AF_UNIX),
      _type(SOCK_STREAM),
      _protocol(0),
      _isParent(false),
      _blocking(false),
      _parentFd(-1),
      _childFd(childFd),
      _initialized(false)
    { }


    int LinuxSocketpairTransceiver::childFd() const
    {
        return _childFd;
    }


    void LinuxSocketpairTransceiver::postForkInit()
    {
        if(_initialized)
        {
            if(_parentFd != -1 && !_isParent)
            {
                // child closes parent socket
                close(_parentFd);
            } 
            else if(_childFd != -1 && _isParent)
            {
                //parent closes child socket
                close(_childFd);
            }
        } else
        {
            SERLIB_LOG_ERROR("postForkInit() must be called AFTER init() and fork()");
        }
    }


    bool LinuxSocketpairTransceiver::init(void)
    {
        int flags;
        
        if(_initialized)
        {
            // already initialized, return here
            return true;
        }

        if(_isParent)
        {
            int fds[2]; // 0 is parent fd, 1 is child fd
            if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not open socketpair: " + string(strerror(errno)));
                _initialized = false;
                return false;
            }

            // set blocking if desired on fds[0]
            flags = fcntl(fds[0], F_GETFL);
            if(flags == -1)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not get flags for parent socket: " + string(strerror(errno)));
                _initialized = false;
                return false;
            }

            if(!_blocking)
            {
                flags |= O_NONBLOCK;
            }

            if(fcntl(fds[0], F_SETFL, flags) == -1)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not set new flags for parent socket: " + string(strerror(errno)));
                _initialized = false;
                return false;
            }

            _parentFd = fds[0];
            _childFd = fds[1];
        }

        // ensure child fd survives exec
        flags = fcntl(_childFd, F_GETFD);
        if(flags == -1)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not get flags for child socket: " + string(strerror(errno)) + " ... has the socket been opened?");
            _initialized = false;
            return false;
        }

        flags &= ~FD_CLOEXEC;
        
        if(fcntl(_childFd, F_SETFD, flags) == -1)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not set new flags for child socket: " + string(strerror(errno)));
            _initialized = false;
            return false;
        }

        if(!_blocking)
        {
            flags = fcntl(_childFd, F_GETFL);
            if(flags == -1)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not get FL flags for child socket: " + string(strerror(errno)) + " ... has the socket been opened?");
                _initialized = false;
                return false;
            }

            flags |= O_NONBLOCK;

            if(fcntl(_childFd, F_SETFL, flags) == -1)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION("Could not set new FL flags for child socket: " + string(strerror(errno)));
                _initialized = false;
                return false;
            }
        }

        _initialized = true;
        return true;
    }


    void LinuxSocketpairTransceiver::send(const char *data, size_t numData)
    {
        if(_initialized)
        {
            int fd = (_isParent ? _parentFd : _childFd);
            
            if(write(fd, data, numData) == -1)
            {
                SERLIB_LOG_ERROR("Failed to write: %s", strerror(errno));
            }
        }
    }


    size_t LinuxSocketpairTransceiver::recv(char *data, size_t numData)
    {
        memset(data, 0, numData);
        if(_initialized)
        {
            int fd = (_isParent ? _parentFd : _childFd);
            ssize_t ret = read(fd, data, numData);
            if(ret < 0)
            {
                return 0;
            }

            return ret;
        }

        return 0;
    }


    void LinuxSocketpairTransceiver::deinit(void)
    {
        if(_initialized)
        {
            if(_parentFd != -1)
            {
                close(_parentFd);
            }
    
            if(_childFd != -1)
            {
                close(_childFd);
            }
        }

        _initialized = false;
    }
}

#endif
