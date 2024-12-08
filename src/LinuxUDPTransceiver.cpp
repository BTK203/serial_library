#include "serial_library/serial_library.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netdb.h>

namespace serial_library
{

    LinuxUDPTransceiver::LinuxUDPTransceiver(const std::string& address, int port, bool allowAddrReuse)
     : address(address),
       port(port),
       allowAddrReuse(allowAddrReuse),
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

        //allow the address to be used if the user wants. needed for testing on local machine
        int option = 1;
        if(allowAddrReuse)
        {
            if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
            {
                SERLIB_LOG_ERROR("setsockopt() failed while trying to allow addr reuse: %s", strerror(errno));
            }
        }

        //bind to address
        addrinfo ahints, *ainfo;
        ahints.ai_family = AF_UNSPEC;
        ahints.ai_socktype = SOCK_DGRAM;
        ahints.ai_flags = AI_PASSIVE;
        ahints.ai_protocol = 0;
        ahints.ai_addr = NULL;
        ahints.ai_next = NULL;

        if((res = getaddrinfo(NULL, std::to_string(port).c_str(), &ahints, &ainfo)) < 0)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("getaddrinfo() failed for bind: " + to_string(res));
            return false;
        }

        //try to bind to an address returned by getaddrinfo
        addrinfo *nextainfo = ainfo;
        while(nextainfo)
        {
            if(bind(sock, nextainfo->ai_addr, nextainfo->ai_addrlen) == 0)
            {
                break;
            }

            SERLIB_LOG_DEBUG("Failed to bind(): %s", strerror(errno));
            nextainfo = nextainfo->ai_next;
        }

        if(!nextainfo) //ran out of options to try. didnt bind
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("bind() for all address options failed!");
            return false;
        } else
        {
            SERLIB_LOG_DEBUG("bind() succeeded!");
        }

        freeaddrinfo(ainfo);

        //now connect to target
        ahints.ai_family = AF_UNSPEC;
        ahints.ai_socktype = SOCK_DGRAM;
        ahints.ai_flags = AI_PASSIVE;
        ahints.ai_protocol = 0;
        ahints.ai_addr = NULL;
        ahints.ai_next = NULL;

        if((res = getaddrinfo(address.c_str(), std::to_string(port).c_str(), &ahints, &ainfo)) < 0)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("getaddrinfo() failed for connect: " + to_string(res));
            return false;
        }

        //try to connect to an address returned by getaddrinfo
        nextainfo = ainfo;
        while(nextainfo)
        {
            if(connect(sock, nextainfo->ai_addr, nextainfo->ai_addrlen) == 0)
            {
                break;
            }

            SERLIB_LOG_DEBUG("Failed to connect(): %s", strerror(errno));
            nextainfo = nextainfo->ai_next;
        }

        if(!nextainfo)
        {
            THROW_FATAL_SERIAL_LIB_EXCEPTION("connect() failed for all address options!");
            return false;
        } else
        {
            SERLIB_LOG_DEBUG("connect() succeeded!");
        }

        return true;
    }


    void LinuxUDPTransceiver::send(const char *data, size_t numData) const
    {
        ::send(sock, data, numData, 0);
    }


    size_t LinuxUDPTransceiver::recv(char *data, size_t numData) const
    {
        return ::recv(sock, data, numData, MSG_DONTWAIT);
    }


    void LinuxUDPTransceiver::deinit(void)
    {
        close(sock);
    }
}
