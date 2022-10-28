/*
 * ApplicationDataHandler.cc
 *
 *  Created on: 24.08.2020
 *      Author: krassus
 */

#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/integer.h"

#include "EEBTPDataHeader.h"
#include "ApplicationDataHandler.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("ApplicationDataHandler");
	NS_OBJECT_ENSURE_REGISTERED(ApplicationDataHandler);

	ApplicationDataHandler::ApplicationDataHandler()
	{
		this->packetCount = 0;
		this->currentSeqNo = 0;
	}

	ApplicationDataHandler::~ApplicationDataHandler()
	{
	}

	TypeId ApplicationDataHandler::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::ApplicationDataHandler")
								.SetParent<Object>()
								.AddConstructor<ApplicationDataHandler>();
		return tid;
	}

	TypeId ApplicationDataHandler::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	/*
	 * Application data handler
	 */
	bool ApplicationDataHandler::handleApplicationData(Ptr<Packet> packet)
	{
		EEBTPDataHeader header;
		packet->PeekHeader(header);

		if (header.GetSequenceNumber() > this->currentSeqNo + 16)
		{
			NS_LOG_DEBUG("Dropping data packet with seqNo " << header.GetSequenceNumber() << " because it is out of our sliding window (" << this->currentSeqNo << " -> " << (this->currentSeqNo + 16) << ")");
		}
		else
		{
			bool isMissing = false;
			for (uint i = 0; i < this->missingSeqNos.size(); i++)
			{
				if (this->missingSeqNos[i] == header.GetSequenceNumber())
				{
					isMissing = true;
					this->missingSeqNos.erase(this->missingSeqNos.begin() + i);
					break;
				}
			}

			if (!isMissing)
			{
				if (this->currentSeqNo < header.GetSequenceNumber())
				{
					NS_LOG_DEBUG("Received data packet with dSeqNo = " << header.GetSequenceNumber());
					this->packetCount++;

					this->currentSeqNo++;
					while (this->currentSeqNo != header.GetSequenceNumber())
					{
						this->missingSeqNos.push_back(this->currentSeqNo);
						this->currentSeqNo++;
					}
				}
				else
				{
					NS_LOG_DEBUG("Dropping data packet with dSeqNo = " << header.GetSequenceNumber() << " because it is a duplicate");
					return false;
				}
			}
			else
				NS_LOG_DEBUG("Received missing data packet with dSeqNo = " << header.GetSequenceNumber());

			return true;
		}
		return false;
	}

	/*Ptr<Packet> ApplicationDataHandler::getLastPacket()
	{
		return *(this->packets.end()-1);
	}

	Ptr<Packet> ApplicationDataHandler::getPacketBySeqNo(uint32_t seqNo)
	{
		EEBTPDataHeader hdr;
		for(Ptr<Packet> pkt : this->packets)
		{
			pkt->PeekHeader(hdr);
			if(hdr.GetSequenceNumber() == seqNo)
				return pkt;
		}
		return 0;
	}*/

	std::vector<uint32_t> ApplicationDataHandler::getMissingSeqNos()
	{
		return this->missingSeqNos;
	}

	/*
	 * For the initiator to count sequence numbers
	 */
	uint32_t ApplicationDataHandler::getLastSeqNo()
	{
		return this->currentSeqNo;
	}

	void ApplicationDataHandler::incrementSeqNo()
	{
		this->currentSeqNo++;
	}

	uint32_t ApplicationDataHandler::getPacketCount()
	{
		return this->packetCount;
	}
}
