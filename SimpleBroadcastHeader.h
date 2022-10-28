/*
 * SimpleBroadcastHeader.h
 *
 *  Created on: 04.05.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_SIMPLEBROADCASTHEADER_H_
#define BROADCAST_SIMPLEBROADCASTHEADER_H_

#include "ns3/header.h"
#include "ns3/mac48-address.h"

namespace ns3
{
	class SimpleBroadcastHeader : public Header
	{
	public:
		SimpleBroadcastHeader(uint8_t hopCount = 1);
		virtual ~SimpleBroadcastHeader();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;
		virtual void Print(std::ostream &os) const;
		virtual void Serialize(Buffer::Iterator start) const;
		virtual uint32_t Deserialize(Buffer::Iterator start);
		virtual uint32_t GetSerializedSize() const;

		void SetOriginator(Mac48Address address);
		Mac48Address GetOriginator();

		uint8_t GetHopCount();
		void SetHopCount(uint8_t hopCount);
		void DecrementHopCount();

		uint32_t GetSequenceNumber();
		void SetSequenceNumber(uint32_t seqNo);

	private:
		uint32_t seqNo;
		uint8_t hopCount;
		Mac48Address originator;
	};
}

#endif /* BROADCAST_SIMPLEBROADCASTHEADER_H_ */
