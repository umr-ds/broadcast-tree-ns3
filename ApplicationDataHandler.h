/*
 * ApplicationDataHandler.h
 *
 *  Created on: 24.08.2020
 *      Author: krassus
 */

#ifndef BROADCAST_APPLICATIONDATAHANDLER_H_
#define BROADCAST_APPLICATIONDATAHANDLER_H_

#include "ns3/ptr.h"
#include "ns3/buffer.h"
#include "ns3/object.h"
#include "ns3/packet.h"

namespace ns3
{
	class ApplicationDataHandler : public Object
	{
	public:
		ApplicationDataHandler();
		virtual ~ApplicationDataHandler();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;

		bool handleApplicationData(Ptr<Packet> packet);

		uint32_t getLastSeqNo();
		void incrementSeqNo();

		std::vector<uint32_t> getMissingSeqNos();

		uint32_t getPacketCount();

	private:
		uint32_t currentSeqNo;
		uint32_t packetCount;

		std::vector<uint32_t> missingSeqNos;
	};
}

#endif /* BROADCAST_APPLICATIONDATAHANDLER_H_ */
