#include "serial_library/serial_library.hpp"

#if defined(USE_LINUX)

#include <netinet/in.h>
#include <netinet/udp.h>
#include <netdb.h>

namespace serial_library
{

    LinuxUDPTransceiver::LinuxUDPTransceiver(const std::string& address, int port, double recvTimeoutSeconds, bool skipBind, bool skipConnect, bool allowAddrReuse)
     : address(address),
       port(port),
       recvTimeoutSeconds(recvTimeoutSeconds),
       allowAddrReuse(allowAddrReuse),
       skipBind(skipBind),
       skipConnect(skipConnect),
       sock(-1) { }


    bool LinuxUDPTransceiver::init(void)
    {
        int res;
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(sock < 0)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("socket() failed: " + string(strerror(errno)));
            return false;
        }

        //set socket timeout
        timeval to;
        to.tv_sec = (int) recvTimeoutSeconds;
        to.tv_usec = (recvTimeoutSeconds - (double) to.tv_sec) * 1000000;
        if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) < 0)
        {
            close(sock);
            THROW_FATAL_SERIAL_LIB_EXCEPTION("setsockopt() failed while trying to set socket recv timeout: " + string(strerror(errno)));
        }

        //allow the address to be used if the user wants. needed for testing on local machine
        int option = 1;
        if(allowAddrReuse)
        {
            if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
            {
                SERLIB_LOG_ERROR("setsockopt() failed while trying to allow addr reuse: %s. Continuing setup", strerror(errno));
            }
        }

        sockaddr_in bindaddr;
        memset(&bindaddr, 0, sizeof(bindaddr));
        bindaddr.sin_family = AF_INET;
        bindaddr.sin_addr.s_addr = INADDR_ANY;
        bindaddr.sin_port = htons(port);
        if(!skipBind)
        {
            if(bind(sock, (const sockaddr *) &bindaddr, sizeof(bindaddr)) < 0)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION("bind() failed: " + string(strerror(errno)));
                close(sock);
                return false;
            }
            
            SERLIB_LOG_DEBUG("bind() succeeded!");
        } else
        {
            SERLIB_LOG_DEBUG("bind() skipped");
        }

        if(!skipConnect)
        {
            SERLIB_LOG_DEBUG("Connecting to address %s on port %d", address.c_str(), port);

            //now connect to target
            addrinfo ahints, *ainfo;
            memset(&ahints, 0, sizeof(ahints));
            ahints.ai_family = AF_INET;
            ahints.ai_socktype = SOCK_DGRAM;
            ahints.ai_flags = 0;
            ahints.ai_protocol = 0;
            ahints.ai_addr = NULL;
            ahints.ai_next = NULL;

            if((res = getaddrinfo(address.c_str(), std::to_string(port).c_str(), &ahints, &ainfo)) < 0)
            {
                SERLIB_LOG_ERROR("getaddrinfo() failed for connect (address %s): %s", address.c_str(), strerror(errno));
                close(sock);
                return false;
            }

            //try to connect to an address returned by getaddrinfo
            addrinfo *nextainfo = NULL;
            for(nextainfo = ainfo; nextainfo; nextainfo = nextainfo->ai_next)
            {
                if(connect(sock, nextainfo->ai_addr, nextainfo->ai_addrlen) == 0)
                {
                    break;
                }

                SERLIB_LOG_ERROR("Failed to connect() to %s: %s", address.c_str(), strerror(errno));
            }

            if(!nextainfo)
            {
                THROW_FATAL_SERIAL_LIB_EXCEPTION("connect() failed for all address options!");
                close(sock);
                return false;
            }
            
            SERLIB_LOG_DEBUG("connect() succeeded!");
            freeaddrinfo(ainfo);
        } else
        {
            SERLIB_LOG_DEBUG("connect() skipped");
        }

        return true;
    }


    void LinuxUDPTransceiver::send(const char *data, size_t numData) const
    {
        size_t ret = ::send(sock, data, numData, 0);
        if((ssize_t) ret == -1)
        {
            SERLIB_LOG_DEBUG("send() to %s failed: %s", address.c_str(), strerror(errno));
        }
    }


    size_t LinuxUDPTransceiver::recv(char *data, size_t numData)
    {
        size_t ret = ::recv(sock, data, numData, 0);
        if((ssize_t) ret == -1)
        {
            if(errno != EAGAIN)
            {
                SERLIB_LOG_DEBUG("recv() from %s failed (%d) : %s", address.c_str(), errno, strerror(errno));
            }
            
            return 0;
        }

        return ret;
    }


    void LinuxUDPTransceiver::deinit(void)
    {
        close(sock);
    }
}

#endif
