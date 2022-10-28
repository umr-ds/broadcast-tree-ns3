/*
 * SimpleBroadcastHeader.cc
 *
 *  Created on: 04.05.2020
 *      Author: Kevin Küchler
 */

#include "SimpleBroadcastHeader.h"

namespace ns3
{
	NS_OBJECT_ENSURE_REGISTERED(SimpleBroadcastHeader);

	SimpleBroadcastHeader::SimpleBroadcastHeader(uint8_t hopCount)
	{
		this->seqNo = 0;
		this->hopCount = hopCount;
		this->originator = Mac48Address::Allocate();
	}

	SimpleBroadcastHeader::~SimpleBroadcastHeader()
	{
	}

	TypeId SimpleBroadcastHeader::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::SimpleBroadcastHeader").SetParent<Header>().AddConstructor<SimpleBroadcastHeader>();
		return tid;
	}

	TypeId SimpleBroadcastHeader::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	void SimpleBroadcastHeader::Print(std::ostream &os) const
	{
		os << "originator=" << originator.operator ns3::Address();
	}

	uint32_t SimpleBroadcastHeader::GetSerializedSize() const
	{
		return 11; // 6 bytes for the originator address and 1 byte for the hop count
	}

	void SimpleBroadcastHeader::Serialize(Buffer::Iterator start) const
	{
		//Write the address of the originator
		uint8_t addr[6];
		this->originator.CopyTo(addr);
		start.Write(addr, 6);

		//Write sequence number
		start.WriteU32(this->seqNo);

		//Write the current hop count
		start.WriteU8(this->hopCount);
	}

	uint32_t SimpleBroadcastHeader::Deserialize(Buffer::Iterator start)
	{
		//Read address of the originator
		uint8_t addr[6];
		start.Read(addr, 6);
		this->originator = Mac48Address::Allocate();
		this->originator.CopyFrom(addr);

		//Read the sequence number
		this->seqNo = start.ReadU32();

		//Read the current hop count
		this->hopCount = start.ReadU8();
		return 11;
	}

	/*
	 * Getter and Setter for the address of the originator
	 */
	Mac48Address SimpleBroadcastHeader::GetOriginator()
	{
		return this->originator;
	}

	void SimpleBroadcastHeader::SetOriginator(Mac48Address address)
	{
		originator = address;
	}

	/*
	 * Getter and Setter for the hop count
	 * Decrement function for more comfort
	 */
	uint8_t SimpleBroadcastHeader::GetHopCount()
	{
		return this->hopCount;
	}

	void SimpleBroadcastHeader::SetHopCount(uint8_t hopCount)
	{
		this->hopCount = hopCount;
	}

	void SimpleBroadcastHeader::DecrementHopCount()
	{
		this->hopCount--;
	}

	/*
	 * Getter and Setter for the sequence number
	 */
	uint32_t SimpleBroadcastHeader::GetSequenceNumber()
	{
		return this->seqNo;
	}

	void SimpleBroadcastHeader::SetSequenceNumber(uint32_t seqNo)
	{
		this->seqNo = seqNo;
	}
}
