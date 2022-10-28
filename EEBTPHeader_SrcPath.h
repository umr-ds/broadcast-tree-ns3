/*
 * EEBTPHeader_SrcPath.h
 *
 *  Created on: 21.07.2020
 *      Author: krassus
 */

#ifndef BROADCAST_EEBTPHEADER_SRCPATH_H_
#define BROADCAST_EEBTPHEADER_SRCPATH_H_

#include "ns3/header.h"
#include "ns3/mac48-address.h"

#include "EEBTPHeader.h"

namespace ns3
{
	class EEBTPHeaderSrcPath : public EEBTPHeader
	{
	public:
		EEBTPHeaderSrcPath();
		virtual ~EEBTPHeaderSrcPath();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;
		virtual void Print(std::ostream &os) const;
		virtual void Serialize(Buffer::Iterator start) const;
		virtual uint32_t Deserialize(Buffer::Iterator start);
		virtual uint32_t GetSerializedSize() const;

		void addNodeToPath(Mac48Address addr);
		std::vector<Mac48Address> getSrcPath();

	private:
		std::vector<Mac48Address> path;
	};
}

#endif /* BROADCAST_EEBTPHEADER_SRCPATH_H_ */
