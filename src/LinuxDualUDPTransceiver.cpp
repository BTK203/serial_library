#include "serial_library/serial_library.hpp"

#if defined(USE_LINUX)

namespace serial_library  {
    
    LinuxDualUDPTransceiver::LinuxDualUDPTransceiver(
        const std::string& address, 
        int recvPort, 
        int sendPort, 
        double recvTimeoutSeconds, 
        bool allowReuseAddr)
    : recvUDP(address, recvPort, recvTimeoutSeconds, false, true, allowReuseAddr),
      sendUDP(address, sendPort, recvTimeoutSeconds, true, false, allowReuseAddr) { }

    bool LinuxDualUDPTransceiver::init(void)
    {
        return recvUDP.init() && sendUDP.init();
    }

    void LinuxDualUDPTransceiver::send(const char *data, size_t numData)
    {
        sendUDP.send(data, numData);
    }

    size_t LinuxDualUDPTransceiver::recv(char *data, size_t numData)
    {
        return recvUDP.recv(data, numData);
    }

    void LinuxDualUDPTransceiver::deinit(void) 
    {
        recvUDP.deinit();
        sendUDP.deinit();
    }

}

#endif
