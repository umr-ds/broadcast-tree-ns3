/*
 * SimpleBroadcastProtocol.cc
 *
 *  Created on: 06.05.2020
 *      Author: krassus
 */

#include "ns3/log.h"
#include "ns3/integer.h"
#include "ns3/callback.h"
#include "ns3/EEBTPTag.h"
#include "ns3/wifi-mac.h"
#include "ns3/core-module.h"
#include "ns3/wifi-utils.h"
#include "ns3/llc-snap-header.h"
#include "ns3/traffic-control-helper.h"

#include "EEBTPQueueDiscItem.h"
#include "SimpleBroadcastProtocol.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("SimpleBroadcastProtocol");
	NS_OBJECT_ENSURE_REGISTERED(SimpleBroadcastProtocol);

	const uint16_t SimpleBroadcastProtocol::PROT_NUMBER = 150;

	SimpleBroadcastProtocol::SimpleBroadcastProtocol()
	{
		this->maxTxPower = 23.0;

		this->energyAtStart = 0;

		this->energyRecv = 0;
		this->energySent = 0;

		this->dataRecv = 0;
		this->dataSent = 0;

		this->packetsRecv = 0;
		this->packetsSent = 0;
		this->uniquePackets = 0;
	}

	SimpleBroadcastProtocol::~SimpleBroadcastProtocol()
	{
	}

	TypeId SimpleBroadcastProtocol::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::SimpleBroadcastProtocol").SetParent<Object>().AddConstructor<SimpleBroadcastProtocol>();
		return tid;
	}

	TypeId SimpleBroadcastProtocol::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	/*
	 * Receive method
	 * Every packet with the protocol ID 'PROT_NUMBER' will be redirected to this method
	 * This methods takes
	 * 	- device:   the NetDevice on which the packet arrived
	 * 	- packet:   the actual Packet
	 * 	- pID:	    the protocol ID
	 * 	- sender:   the sender of this packet
	 * 	- receiver:	the receiver of this packet
	 * 	- ptype:    the type of this packet
	 */
	void SimpleBroadcastProtocol::Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType)
	{
		Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(device);

		EEBTPTag tag;
		Ptr<Packet> pkt = packet->Copy();
		SimpleBroadcastHeader header;
		pkt->RemoveHeader(header);
		pkt->RemovePacketTag(tag);

		NS_LOG_DEBUG("[Node " << dev->GetNode()->GetId() << "]: Received a packet from [" << Mac48Address::ConvertFrom(sender) << "] with [" << header.GetOriginator() << "] as the originator / SeqNo: " << header.GetSequenceNumber() << " / time = " << Now());

		//Update hop count of the message
		header.DecrementHopCount();

		this->packetsRecv++;

		if (header.GetOriginator() == device->GetAddress()) //Check if we are the originator
			NS_LOG_DEBUG("We are the originator! Hence we are not allowed to re-broadcast it.");
		else
		{
			bool alreadyReceived = false;
			for (uint32_t i : this->cache[header.GetOriginator()])
			{
				if (i == header.GetSequenceNumber())
				{
					alreadyReceived = true;
					break;
				}
			}

			if (!alreadyReceived)
			{
				//Update cache
				this->cache[header.GetOriginator()].push_back(header.GetSequenceNumber());
				this->uniquePackets++;

				//Check if we have to re-broadcast the packet
				if (header.GetHopCount() > 0) //Check if the hop count is zero
				{
					NS_LOG_DEBUG("Hop count of the packet is 0! Hence we are not allowed to re-broadcast it.");
					//Create new packet
					Ptr<Packet> packetOut = pkt->Copy();

					//Set modified header
					packetOut->AddHeader(header);

					tag.setTxPower(this->maxTxPower);
					tag.setSequenceNumber(header.GetSequenceNumber());
					packetOut->AddPacketTag(tag);

					//Re-Broadcast packet
					//device->Send(packetOut, Mac48Address::GetBroadcast(), SimpleBroadcastProtocol::PROT_NUMBER);
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Send packet with [" << header.GetOriginator() << "] as the originator / SeqNo: " << header.GetSequenceNumber() << " / time = " << Now());
					Ptr<EEBTPQueueDiscItem> qdi = Create<EEBTPQueueDiscItem>(packetOut, Mac48Address::GetBroadcast(), SimpleBroadcastProtocol::PROT_NUMBER);
					this->tcl->Send(this->device, qdi);
					this->packetsSent++;
				}
			}
		}
	}

	void SimpleBroadcastProtocol::Send(uint16_t hopCount, uint32_t seqNo)
	{
		SimpleBroadcastHeader header = SimpleBroadcastHeader(hopCount);
		Mac48Address addr = Mac48Address::ConvertFrom(this->device->GetAddress());
		Ptr<Packet> packet = Create<Packet>(1000);

		header.SetOriginator(addr);
		header.SetSequenceNumber(seqNo);
		packet->AddHeader(header);

		NS_LOG_DEBUG("MaxTxPower: " << this->maxTxPower << "dBm");
		EEBTPTag tag;
		tag.setSequenceNumber(seqNo);
		tag.setTxPower(this->maxTxPower);
		packet->AddPacketTag(tag);

		//this->device->Send(packet, Mac48Address::GetBroadcast(), SimpleBroadcastProtocol::PROT_NUMBER);
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Send packet with [" << header.GetOriginator() << "] as the originator / SeqNo: " << header.GetSequenceNumber() << " / time = " << Now());
		Ptr<EEBTPQueueDiscItem> qdi = Create<EEBTPQueueDiscItem>(packet, Mac48Address::GetBroadcast(), SimpleBroadcastProtocol::PROT_NUMBER);
		this->tcl->Send(this->device, qdi);

		this->uniquePackets++;
		this->packetsSent++;
	}

	double SimpleBroadcastProtocol::getRecvEnergy()
	{
		return this->energyRecv;
	}

	double SimpleBroadcastProtocol::getSentEnergy()
	{
		return this->energySent;
	}

	uint32_t SimpleBroadcastProtocol::getRecvPackets()
	{
		return this->packetsRecv;
	}

	uint32_t SimpleBroadcastProtocol::getSentPackets()
	{
		return this->packetsSent;
	}

	uint32_t SimpleBroadcastProtocol::getUniquePackets()
	{
		return this->uniquePackets;
	}

	uint32_t SimpleBroadcastProtocol::getRecvData()
	{
		return this->dataRecv;
	}

	uint32_t SimpleBroadcastProtocol::getSentData()
	{
		return this->dataSent;
	}

	/*
	 * Install method to install this protocol on the stack of a node
	 */
	void SimpleBroadcastProtocol::Install(Ptr<WifiNetDevice> netDevice)
	{
		this->device = netDevice;
		this->energySource = this->device->GetNode()->GetObject<EnergySourceContainer>()->Get(0);

		Ptr<WifiPhy> wifiPhy = this->device->GetMac()->GetWifiPhy();

		wifiPhy->SetNTxPower(1);
		wifiPhy->SetTxPowerStart(this->maxTxPower);
		wifiPhy->SetTxPowerEnd(this->maxTxPower);

		wifiPhy->TraceConnectWithoutContext("PhyRxBegin", MakeCallback(&SimpleBroadcastProtocol::onRxStart, this));
		wifiPhy->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&SimpleBroadcastProtocol::onRxEnd, this));
		wifiPhy->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&SimpleBroadcastProtocol::onTxStart, this));
		wifiPhy->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&SimpleBroadcastProtocol::onTxDrop, this));
		wifiPhy->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&SimpleBroadcastProtocol::onTxEnd, this));

		PointerValue ptr;
		wifiPhy->GetAttribute("State", ptr);
		Ptr<WifiPhyStateHelper> stateHelper = DynamicCast<WifiPhyStateHelper>(ptr.Get<WifiPhyStateHelper>());
		stateHelper->TraceConnectWithoutContext("State", MakeCallback(&SimpleBroadcastProtocol::onPhyStateChanged, this));
		this->energySource->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&SimpleBroadcastProtocol::onEnergyChanged, this));

		this->device->GetMac()->TraceConnectWithoutContext("TxOkHeader", MakeCallback(&SimpleBroadcastProtocol::onTxSuccessful, this));
		this->device->GetMac()->TraceConnectWithoutContext("MacTxDrop", MakeCallback(&SimpleBroadcastProtocol::onTxDropped, this));
		this->device->GetMac()->TraceConnectWithoutContext("TxErrHeader", MakeCallback(&SimpleBroadcastProtocol::onTxFailed, this));

		TrafficControlHelper tch = TrafficControlHelper();
		tch.Install(this->device);

		this->tcl = this->device->GetNode()->GetObject<TrafficControlLayer>();
		this->device->GetNode()->RegisterProtocolHandler(MakeCallback(&TrafficControlLayer::Receive, this->tcl), SimpleBroadcastProtocol::PROT_NUMBER, this->device);
		this->tcl->RegisterProtocolHandler(MakeCallback(&SimpleBroadcastProtocol::Receive, this), SimpleBroadcastProtocol::PROT_NUMBER, this->device);
		//this->device->GetNode()->RegisterProtocolHandler(MakeCallback(&SimpleBroadcastProtocol::Receive, this), SimpleBroadcastProtocol::PROT_NUMBER, netDevice);
	}

	/*
	 *
	 */
	void SimpleBroadcastProtocol::onRxStart(Ptr<const Packet> packet)
	{
		Ptr<Packet> pkt = packet->Copy();

		WifiMacHeader hdr;
		pkt->RemoveHeader(hdr);

		//If this packet contains data for upper layer protocols
		if (hdr.IsData())
		{
			//Get protocol ID from snap header
			LlcSnapHeader lhdr;
			pkt->RemoveHeader(lhdr);

			//If this packet contains EEBTProtocol data
			if (lhdr.GetType() == SimpleBroadcastProtocol::PROT_NUMBER)
			{
				this->energyAtStart = this->energySource->GetRemainingEnergy();
			}
		}
	}

	void SimpleBroadcastProtocol::onRxEnd(Ptr<const Packet> packet)
	{
		Ptr<Packet> pkt = packet->Copy();

		WifiMacHeader hdr;
		pkt->RemoveHeader(hdr);

		//If this packet contains data for upper layer protocols
		if (hdr.IsData())
		{
			//Get protocol ID from snap header
			LlcSnapHeader lhdr;
			pkt->RemoveHeader(lhdr);

			//If this packet contains EEBTProtocol data
			if (lhdr.GetType() == SimpleBroadcastProtocol::PROT_NUMBER)
			{
				this->energyRecv += (this->energyAtStart - this->energySource->GetRemainingEnergy());
				this->dataRecv += pkt->GetSize();
			}
		}
	}

	void SimpleBroadcastProtocol::onTxStart(Ptr<const Packet> packet, double txPowerW)
	{
		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxStart() => packetSize = " << packet->GetSize() << ", energy = " << this->energySource->GetRemainingEnergy());
			this->energyAtStart = this->energySource->GetRemainingEnergy();
		}
	}

	void SimpleBroadcastProtocol::onTxDrop(Ptr<const Packet> packet)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxDrop() => packetSize = " << packet->GetSize() << ", energy = " << this->energySource->GetRemainingEnergy());
	}

	void SimpleBroadcastProtocol::onTxEnd(Ptr<const Packet> packet)
	{
		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxEnd() => packetSize = " << packet->GetSize() << ", energy = " << (this->energyAtStart - this->energySource->GetRemainingEnergy()) << ", tx = " << this->device->GetPhy()->GetTxPowerEnd() << "dBm");
			this->dataSent += packet->GetSize();
			this->energySent += (this->energyAtStart - this->energySource->GetRemainingEnergy());
		}
	}

	void SimpleBroadcastProtocol::onTxFailed(const WifiMacHeader &header)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxFailed()");
	}

	void SimpleBroadcastProtocol::onTxDropped(Ptr<const Packet> packet)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxDropped()");
	}

	void SimpleBroadcastProtocol::onTxSuccessful(const WifiMacHeader &header)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxSuccessful()");
	}

	void SimpleBroadcastProtocol::onPhyStateChanged(Time start, Time duration, WifiPhyState state)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: WifiPhyState changed to " << state << " at " << start << " for " << duration);
	}

	void SimpleBroadcastProtocol::onEnergyChanged(double oldValue, double newValue)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Energy changed from " << oldValue << " to " << newValue << " at " << Now());
	}
}
