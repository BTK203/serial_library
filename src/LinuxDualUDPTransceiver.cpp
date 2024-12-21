#include "serial_library/serial_library.hpp"

namespace serial_library  {
    
    LinuxDualUDPTransceiver::LinuxDualUDPTransceiver(const std::string& address, int recvPort, int sendPort)
    : recvUDP(address, recvPort, false, true),
      sendUDP(address, sendPort, true, false) { }

    bool LinuxDualUDPTransceiver::init(void)
    {
        return recvUDP.init() && sendUDP.init();
    }

    void LinuxDualUDPTransceiver::send(const char *data, size_t numData) const
    {
        sendUDP.send(data, numData);
    }

    size_t LinuxDualUDPTransceiver::recv(char *data, size_t numData) const
    {
        return recvUDP.recv(data, numData);
    }

    void LinuxDualUDPTransceiver::deinit(void) 
    {
        recvUDP.deinit();
        sendUDP.deinit();
    }

}
