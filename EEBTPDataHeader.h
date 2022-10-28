/*
 * EEBTPHeader.h
 *
 *  Created on: 24.08.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_EEBTPDataHeader_H_
#define BROADCAST_EEBTPDataHeader_H_

#include "ns3/header.h"

namespace ns3
{
	class EEBTPDataHeader : public Header
	{
	public:
		EEBTPDataHeader();
		virtual ~EEBTPDataHeader();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;
		virtual void Print(std::ostream &os) const;
		virtual void Serialize(Buffer::Iterator start) const;
		virtual uint32_t Deserialize(Buffer::Iterator start);
		virtual uint32_t GetSerializedSize() const;

		uint32_t GetSequenceNumber();
		void SetSequenceNumber(uint32_t seqNo);

		uint32_t GetDataLength();
		void SetDataLength(uint32_t len);

	protected:
		uint32_t seqNo;
		uint32_t dataLen;
	};
}

#endif /* BROADCAST_EEBTPDataHeader_H_ */
