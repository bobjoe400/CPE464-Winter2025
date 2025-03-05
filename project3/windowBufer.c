#include "windowBuffer.h"

bool window_init(uint32_t windowSize)
{
	return false;
}

bool add_packet(Packet_t *packePtr)
{
	return false;
}

Packet_t *get_packet(Packet_t *packetPtr, SeqNum_t seqNum)
{
	return 0;
}

Packet_t *get_next_packet(Packet_t *nextPacketPtr)
{
	return 0;
}

bool can_send_packet()
{
	return false;
}

bool can_receive_packet()
{
	return false;
}
