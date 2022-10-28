/*
 * EEBTPHeader.cc
 *
 *  Created on: 24.08.2020
 *      Author: Kevin Küchler
 */

#include "ns3/integer.h"

#include "EEBTPDataHeader.h"

namespace ns3
{
	NS_OBJECT_ENSURE_REGISTERED(EEBTPDataHeader);

	EEBTPDataHeader::EEBTPDataHeader()
	{
		this->seqNo = 0;
		this->dataLen = 0;
	}

	EEBTPDataHeader::~EEBTPDataHeader()
	{
	}

	TypeId EEBTPDataHeader::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::EEBTPDataHeader")
								.SetParent<Header>()
								.AddConstructor<EEBTPDataHeader>()
								.AddAttribute("seqNo", "The sequence number of this packet",
											  IntegerValue(0),
											  MakeIntegerAccessor(&EEBTPDataHeader::seqNo),
											  MakeIntegerChecker<int>());
		return tid;
	}

	TypeId EEBTPDataHeader::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	void EEBTPDataHeader::Print(std::ostream &os) const
	{
	}

	uint32_t EEBTPDataHeader::GetSerializedSize() const
	{
		uint32_t sSize = 8;

		return sSize;
	}

	void EEBTPDataHeader::Serialize(Buffer::Iterator start) const
	{
		start.WriteU32(this->seqNo);

		start.WriteU32(this->dataLen);
	}

	uint32_t EEBTPDataHeader::Deserialize(Buffer::Iterator start)
	{
		uint32_t bytesRead = 8;

		this->seqNo = start.ReadU32();

		this->dataLen = start.ReadU32();

		return bytesRead;
	}

	/*
	 * Getter and Setter for the sequence number
	 */
	uint32_t EEBTPDataHeader::GetSequenceNumber()
	{
		return this->seqNo;
	}

	void EEBTPDataHeader::SetSequenceNumber(uint32_t seqNo)
	{
		this->seqNo = seqNo;
	}

	/*
	 * Getter and Setter for the sequence number
	 */
	uint32_t EEBTPDataHeader::GetDataLength()
	{
		return this->dataLen;
	}

	void EEBTPDataHeader::SetDataLength(uint32_t len)
	{
		this->dataLen = len;
	}
}
