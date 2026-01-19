
#include <cstddef>
#include "../LoraCommon/LoraDriverConfig.hpp"   //RadioConfig

struct packetInfo{
    uint8_t data[maxLoraPacketSize];
    size_t dataSize;
};

void initProtocol(const RadioConfig config);
void protocolRestart(const RadioConfig config);

//blocks until a Lora Packet is ready to be read
bool LoraProtocolRead(packetInfo& packet);

//will attach the command in the next ACK bitmap we send back
bool LoraForwardCommand(uint8_t* command, uint8_t commandLength);