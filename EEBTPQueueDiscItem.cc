/*
 * EEBTPQueueDiscItem.cc
 *
 *  Created on: 25.06.2020
 *      Author: Kevin Küchler
 */

#include "EEBTPQueueDiscItem.h"

namespace ns3
{
	EEBTPQueueDiscItem::EEBTPQueueDiscItem(Ptr<Packet> packet, const Address &addr, uint16_t proto) : QueueDiscItem(packet, addr, proto)
	{
	}

	EEBTPQueueDiscItem::~EEBTPQueueDiscItem() {}

	void EEBTPQueueDiscItem::AddHeader() {}

	uint32_t EEBTPQueueDiscItem::GetSize() const
	{
		return GetPacket()->GetSize();
	}

	bool EEBTPQueueDiscItem::Mark()
	{
		return true;
	}

	void EEBTPQueueDiscItem::Print(std::ostream &os) const
	{
		os << GetPacket();
	}
}
