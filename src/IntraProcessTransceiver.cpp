#include "serial_library/serial_library.hpp"

//
// Source file for everything intra-process communication.
// This encapsulates both the IntraProcessChannel and IntraProcessTransceiver classes.
//

namespace serial_library
{
    //
    // INTRA-PROCESS CHANNEL
    //

    void IntraProcessChannel::setPartner(const std::shared_ptr<IntraProcessChannel>& partner)
    {
        this->_partner = partner;
    }


    bool IntraProcessChannel::hasPartner() const
    {
        return _partner != nullptr;
    }


    void IntraProcessChannel::send(const char *data, size_t numData)
    {
        if(hasPartner())
        {
            _partner->injectData(data, numData);
        }
    }


    size_t IntraProcessChannel::recv(char *data, size_t numData)
    {
        size_t
            available = _data.size(),
            n = (available > numData ? numData : available);

        memcpy(data, _data.data(), n);

        //now clear as much data from _data as was received
        _data.erase(_data.begin(), _data.begin() + n);

        return n;
    }


    void IntraProcessChannel::injectData(const char *data, size_t numData)
    {
        _data.insert(_data.end(), data, data + numData);
    }


    //
    // INTRA-PROCESS TRANSCEIVER
    //

    IntraProcessTransceiver::IntraProcessTransceiver(const std::shared_ptr<IntraProcessChannel>& channel)
    : _channel(channel) 
    { }


    IntraProcessTransceiver::IntraProcessTransceiver()
    : IntraProcessTransceiver(std::make_shared<IntraProcessChannel>())
    { }


    std::shared_ptr<IntraProcessChannel> IntraProcessTransceiver::getChannel()
    {
        return _channel;
    }


    bool IntraProcessTransceiver::init(void)
    {
        return true;
    }


    void IntraProcessTransceiver::send(const char *data, size_t numData)
    {
        _channel->send(data, numData);
    }


    size_t IntraProcessTransceiver::recv(char *data, size_t numData)
    {
        return _channel->recv(data, numData);
    }


    void IntraProcessTransceiver::deinit(void)
    { }
}
