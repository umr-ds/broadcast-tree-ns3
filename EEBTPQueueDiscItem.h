/*
 * EEBTPQueueDiscItem.h
 *
 *  Created on: 24.06.2020
 *      Author: krassus
 */

#ifndef BROADCAST_EEBTPQUEUEDISCITEM_H_
#define BROADCAST_EEBTPQUEUEDISCITEM_H_

#include "ns3/packet.h"
#include "ns3/queue-item.h"

namespace ns3
{
	class EEBTPQueueDiscItem : public QueueDiscItem
	{
	public:
		EEBTPQueueDiscItem(Ptr<Packet> p, const Address &addr, uint16_t protocol);
		virtual ~EEBTPQueueDiscItem();

		virtual bool Mark();
		virtual void AddHeader();
		virtual uint32_t GetSize() const;

		virtual void Print(std::ostream &os) const;
	};
}

#endif /* BROADCAST_EEBTPQUEUEDISCITEM_H_ */
