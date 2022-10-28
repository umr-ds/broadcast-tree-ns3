/*
 * EEBTPTxQueue.cc
 *
 *  Created on: 15.08.2020
 *      Author: krassus
 */

#include "ns3/log.h"
#include "ns3/wifi-utils.h"
#include "ns3/core-module.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/traffic-control-helper.h"

#include "ns3/EEBTPTag.h"
#include "EEBTProtocol.h"
#include "EEBTPQueueDiscItem.h"
#include "EEBTPPacketManager.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("EEBTPPacketManager");
	NS_OBJECT_ENSURE_REGISTERED(EEBTPPacketManager);

	/*
	 * Implementation EEBTPNode
	 */
	EEBTPPacketManager::EEBTPPacketManager()
	{
		this->seqNoAtStart = 0;
		this->energyAtStart = 0;
	}

	EEBTPPacketManager::~EEBTPPacketManager()
	{
		this->addrSeqCache.clear();
		this->dataRecv.clear();
		this->dataSent.clear();
		this->frameDataRecv.clear();
		this->frameDataSent.clear();
		this->frameEnergyRecv.clear();
		this->frameEnergySent.clear();
		this->frameTypesRecv.clear();
		this->frameTypesSent.clear();
		this->packetTag.clear();
		this->packets.clear();
		this->packetsAcked.clear();
		this->packetsLost.clear();
		this->receiver.clear();
	}

	TypeId EEBTPPacketManager::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::GameState")
								.SetParent<Object>()
								.AddConstructor<EEBTPPacketManager>();
		return tid;
	}

	TypeId EEBTPPacketManager::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	void EEBTPPacketManager::setDevice(Ptr<WifiNetDevice> device)
	{
		this->device = device;
		this->tcl = this->device->GetNode()->GetObject<TrafficControlLayer>();
		this->energySource = this->device->GetNode()->GetObject<EnergySourceContainer>()->Get(0);
	}

	void EEBTPPacketManager::sendPacket(Ptr<Packet> packet, Mac48Address recipient)
	{
		EEBTPTag tag;
		packet->PeekPacketTag(tag);

		NS_LOG_DEBUG("EEBTPPacketManager::sendPacket() => " << tag.getTxPower() << "dBm");

		for (uint i = 0; i < this->addrSeqCache[recipient].size(); i++)
		{
			uint16_t seqNo = this->addrSeqCache[recipient][i];
			if (this->packetsLost[seqNo] || this->packetsAcked[seqNo])
			{
				this->addrSeqCache[recipient].erase(this->addrSeqCache[recipient].begin() + i);
				i--;
			}
		}
		this->addrSeqCache[recipient].push_back(tag.getSequenceNumber());

		Ptr<EEBTPQueueDiscItem> qdi = Create<EEBTPQueueDiscItem>(packet, recipient, EEBTProtocol::PROT_NUMBER);
		this->tcl->Send(this->device, qdi);
		//this->device->Send(packet, recipient, EEBTProtocol::PROT_NUMBER);
	}

	/*
	 * RX hooks
	 */
	void EEBTPPacketManager::onRxStart(Ptr<const Packet> packet)
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
			if (lhdr.GetType() == EEBTProtocol::PROT_NUMBER)
			{
				EEBTPHeader ehdr; //=> We can use the regular EEBTPHeader, since we only need the frame type
				ehdr.setShort(true);
				pkt->RemoveHeader(ehdr);

				this->seqNoAtStart = hdr.GetSequenceNumber();
				this->energyAtStart = this->energySource->GetRemainingEnergy();

				if (hdr.GetAddr1() == this->device->GetAddress() || hdr.GetAddr1() == Mac48Address::GetBroadcast())
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: On start of RX (" << Now() << "): MACSeqNo = " << this->seqNoAtStart << " / " << ehdr.GetSequenceNumber() << ", energy = " << this->energyAtStart);
			}
		}
	}

	void EEBTPPacketManager::onPacketRx(Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise)
	{
		//Copy packet
		Ptr<Packet> pkt = packet->Copy();

		WifiMacHeader hdr;
		pkt->RemoveHeader(hdr);

		//If this packet contains data for upper layer protocols
		if (hdr.IsData())
		{
			//Get protocol ID from snap header
			LlcSnapHeader lhdr;
			pkt->RemoveHeader(lhdr);

			//If this packet refers to the EEBTProtocol
			if (lhdr.GetType() == EEBTProtocol::PROT_NUMBER)
			{
				EEBTPHeader ehdr; //=> We can use the regular EEBTPHeader, since we only need the frame type
				ehdr.setShort(true);
				pkt->RemoveHeader(ehdr);

				if (hdr.GetAddr1() == this->device->GetAddress() || hdr.GetAddr1() == Mac48Address::GetBroadcast())
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onPacketRx() => MACSeqNo = " << hdr.GetSequenceNumber() << " / " << ehdr.GetSequenceNumber() << ", time = " << Now());

					//Add the packet tag to get information to the upper layer
					this->packetTag[ehdr.GetSequenceNumber()] = this->createPacketTag(pkt, txVector, signalNoise);
				}
			}
			else if (hdr.GetAddr1() == this->device->GetAddress() || hdr.GetAddr1() == Mac48Address::GetBroadcast())
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onPacketRx() => MACSeqNo = " << hdr.GetSequenceNumber() << " / N/A, time = " << Now());
		}
	}

	void EEBTPPacketManager::onRxEnd(Ptr<const Packet> packet)
	{
		//Copy packet
		Ptr<Packet> pkt = packet->Copy();

		WifiMacHeader hdr;
		pkt->RemoveHeader(hdr);

		//If this packet contains data for upper layer protocols
		if (hdr.IsData())
		{
			//Get protocol ID from snap header
			LlcSnapHeader lhdr;
			pkt->RemoveHeader(lhdr);

			//If this packet refers to the EEBTProtocol
			if (lhdr.GetType() == EEBTProtocol::PROT_NUMBER)
			{
				EEBTPHeader ehdr; //=> We can use the regular EEBTPHeader, since we only need the frame type
				ehdr.setShort(true);
				pkt->RemoveHeader(ehdr);

				//Update statistics
				this->dataRecv[ehdr.GetGameId()] += packet->GetSize();
				this->frameTypesRecv[ehdr.GetGameId()][ehdr.GetFrameType()]++;
				this->frameDataRecv[ehdr.GetGameId()][ehdr.GetFrameType()] += packet->GetSize();
				this->frameEnergyRecv[ehdr.GetGameId()][ehdr.GetFrameType()] += (this->energyAtStart - this->energySource->GetRemainingEnergy());

				if (hdr.GetAddr1() == this->device->GetAddress() || hdr.GetAddr1() == Mac48Address::GetBroadcast())
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Reception of " << hdr.GetSequenceNumber() << " / " << ehdr.GetSequenceNumber() << " finished. FRAME_TYPE: " << (uint32_t)ehdr.GetFrameType() << " Time: " << Now());
			}
		}
	}

	/*
	 * TX hooks
	 * 	- onTxStart: Is called when the transmission of a packet has started
	 * 	- onTxDrop: Is called when the transmission of a packet has failed
	 * 	- onTxEnd: Is called when the transmission of a packet has completed
	 */
	void EEBTPPacketManager::onTxStart(Ptr<const Packet> packet, double txPowerW)
	{
		WifiMacHeader hdr;
		packet->PeekHeader(hdr);

		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			this->energyAtStart = this->energySource->GetRemainingEnergy();

			std::map<uint16_t, uint16_t>::iterator it = this->packets.find(hdr.GetSequenceNumber());
			if (it == this->packets.end())
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Transmission of new packet with MACSeqNo = " << hdr.GetSequenceNumber() << " and EEBTPSeqNo = " << tag.getSequenceNumber() << " started at " << Now());
				this->packets[hdr.GetSequenceNumber()] = tag.getSequenceNumber();
				this->packetsLost[tag.getSequenceNumber()] = false;
				this->packetsAcked[tag.getSequenceNumber()] = false;
			}
			else
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Transmission of packet with MACSeqNo = " << hdr.GetSequenceNumber() << " and EEBTPSeqNo = " << tag.getSequenceNumber() << " started at " << Now());
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Transmission of unknown packet with MACSeqNo = " << hdr.GetSequenceNumber() << " started at " << Now());
		}
	}

	void EEBTPPacketManager::onTxDrop(Ptr<const Packet> packet)
	{
		WifiMacHeader hdr;
		packet->PeekHeader(hdr);

		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with MACSeqNo = " << hdr.GetSequenceNumber() << " and EEBTPSeqNo = " << tag.getSequenceNumber() << " has been dropped at " << Now());
			this->packetsLost[tag.getSequenceNumber()] = true;
			this->packetsAcked[tag.getSequenceNumber()] = false;
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unknown packet with MACSeqNo = " << hdr.GetSequenceNumber() << " has been dropped at " << Now());
		}
	}

	void EEBTPPacketManager::onTxEnd(Ptr<const Packet> packet)
	{
		WifiMacHeader hdr;
		packet->PeekHeader(hdr);

		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with MACSeqNo = " << hdr.GetSequenceNumber() << " and EEBTPSeqNo = " << tag.getSequenceNumber() << " has been transmitted at " << Now());

			if (tag.getFrameType() == APPLICATION_DATA)
				NS_LOG_DEBUG("onTxEnd() => packetSize = " << packet->GetSize() << ", energy = " << (this->energyAtStart - this->energySource->GetRemainingEnergy()));

			this->dataSent[tag.getGameID()] += packet->GetSize();
			this->frameTypesSent[tag.getGameID()][tag.getFrameType()]++;
			this->frameDataSent[tag.getGameID()][tag.getFrameType()] += packet->GetSize();
			this->frameEnergySent[tag.getGameID()][tag.getFrameType()] += (this->energyAtStart - this->energySource->GetRemainingEnergy());
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unknown packet with MACSeqNo = " << hdr.GetSequenceNumber() << " has been transmitted at " << Now());
		}
	}

	void EEBTPPacketManager::onPacketTx(Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu)
	{
		WifiMacHeader hdr;
		packet->PeekHeader(hdr);

		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onPacketTx() => MACSeqNo = " << hdr.GetSequenceNumber() << ", EEBTPSeqNo = " << tag.getSequenceNumber() << ", time = " << Now() << ", energy = " << this->energySource->GetRemainingEnergy());
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onPacketTx() => MACSeqNo = " << hdr.GetSequenceNumber() << ", time = " << Now());
		}
	}

	void EEBTPPacketManager::onTx(Ptr<const Packet> packet)
	{
		LlcSnapHeader lhdr;
		packet->PeekHeader(lhdr);

		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTx() => EEBTPSeqNo = " << tag.getSequenceNumber() << ", time = " << Now());
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTx() => time = " << Now());
		}
	}

	void EEBTPPacketManager::onTxFinalRtsFailed(Mac48Address address)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxFinalRtsFailed(" << address << ")");
		for (uint16_t seqNo : this->addrSeqCache[address])
		{
			if (!(this->packetsLost[seqNo] || this->packetsAcked[seqNo]))
			{
				this->packetsLost[seqNo] = true;
				this->packetsAcked[seqNo] = false;
			}
		}
		this->addrSeqCache[address].clear();
	}

	void EEBTPPacketManager::onTxFinalDataFailed(Mac48Address address)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: onTxFinalDataFailed(" << address << ")");
	}

	/*
	 * Hooks to the MAC layer
	 */
	void EEBTPPacketManager::onTxFailed(const WifiMacHeader &header)
	{
		std::map<uint16_t, uint16_t>::iterator it = this->packets.find(header.GetSequenceNumber());
		if (it != this->packets.end())
		{
			uint16_t seqNo = it->second;
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Transmission of MacSeqNo = " << header.GetSequenceNumber() << " / " << seqNo << " FAILED. Time: " << Now());
			this->packetsAcked[seqNo] = false;
			this->packetsLost[seqNo] = true;
		}
		else
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Transmission of MacSeqNo = " << header.GetSequenceNumber() << " FAILED, but is not known. Time: " << Now());
	}

	void EEBTPPacketManager::onTxDropped(Ptr<const Packet> packet)
	{
		WifiMacHeader hdr;
		packet->PeekHeader(hdr);

		EEBTPTag tag;
		if (packet->PeekPacketTag(tag))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with MACSeqNo = " << hdr.GetSequenceNumber() << " and EEBTPSeqNo = " << tag.getSequenceNumber() << " has been dropped on MAC layer at " << Now());
			this->packetsLost[tag.getSequenceNumber()] = true;
			this->packetsAcked[tag.getSequenceNumber()] = false;
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unknown packet with MACSeqNo = " << hdr.GetSequenceNumber() << " has been dropped on MAC layer at " << Now());
		}
	}

	void EEBTPPacketManager::onTxSuccessful(const WifiMacHeader &header)
	{
		std::map<uint16_t, uint16_t>::iterator it = this->packets.find(header.GetSequenceNumber());
		if (it != this->packets.end())
		{
			uint16_t seqNo = it->second;
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Transmission of MacSeqNo = " << header.GetSequenceNumber() << " / " << seqNo << " successful. Time: " << Now());
			this->packetsAcked[seqNo] = true;
			this->packetsLost[seqNo] = false;
		}
		else
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Transmission of MacSeqNo = " << header.GetSequenceNumber() << " successful, but is not known. Time: " << Now());
	}

	bool EEBTPPacketManager::isPacketAcked(uint16_t seqNo)
	{
		std::map<uint16_t, bool>::iterator it = this->packetsAcked.find(seqNo);
		if (it != this->packetsAcked.end())
		{
			return it->second;
		}
		return false;
	}

	bool EEBTPPacketManager::isPacketLost(uint16_t seqNo)
	{
		std::map<uint16_t, bool>::iterator it = this->packetsLost.find(seqNo);
		if (it != this->packetsLost.end())
		{
			return it->second;
		}
		return false;
	}

	void EEBTPPacketManager::deleteSeqNoEntry(uint16_t seqNo)
	{
		std::map<uint16_t, uint16_t>::iterator it;
		for (it = this->packets.begin(); it != this->packets.end(); it++)
		{
			if (it->second == seqNo)
			{
				this->packets.erase(it);

				std::map<uint16_t, bool>::iterator ix;
				ix = this->packetsLost.find(seqNo);
				this->packetsLost.erase(ix);
				ix = this->packetsAcked.find(seqNo);
				this->packetsAcked.erase(ix);

				break;
			}
		}
	}

	/*
	 * Helper method for the signal info packet tag
	 */
	EEBTPTag EEBTPPacketManager::createPacketTag(Ptr<Packet> packet, WifiTxVector txVector, SignalNoiseDbm signalNoise)
	{
		double minSNR = 15;
		WifiMode wm = txVector.GetMode();
		WifiCodeRate wcr = wm.GetCodeRate();
		uint16_t cSize = wm.GetConstellationSize();
		if (cSize == 2)
		{
			if (wcr == WIFI_CODE_RATE_1_2)
				minSNR = 5;
			else if (wcr == WIFI_CODE_RATE_3_4)
				minSNR = 8;
		}
		else if (cSize == 4)
		{
			if (wcr == WIFI_CODE_RATE_1_2)
				minSNR = 10;
			else if (wcr == WIFI_CODE_RATE_3_4)
				minSNR = 13;
		}
		else if (cSize == 16)
		{
			if (wcr == WIFI_CODE_RATE_1_2)
				minSNR = 16;
			else if (wcr == WIFI_CODE_RATE_3_4)
				minSNR = 19;
		}
		else if (cSize == 64)
		{
			if (wcr == WIFI_CODE_RATE_2_3)
				minSNR = 22;
			else if (wcr == WIFI_CODE_RATE_3_4)
				minSNR = 25;
		}
		else
		{
			NS_LOG_ERROR("Could not determine min SNR from WCR = " << wcr << " and cSize = " << cSize);
		}

		//NS_LOG_DEBUG("noise = " << signalNoise.noise << ", signal = " << signalNoise.signal << ", minSNR = " << minSNR);

		EEBTPTag tag;
		tag.setNoise(signalNoise.noise);
		tag.setSignal(signalNoise.signal);
		tag.setMinSNR(minSNR);
		return tag;
	}

	EEBTPTag EEBTPPacketManager::getPacketTag(uint16_t seqNo)
	{
		EEBTPTag tag = this->packetTag[seqNo];
		this->packetTag.erase(this->packetTag.find(seqNo));
		return tag;
	}

	uint16_t EEBTPPacketManager::getSeqNoByMacSeqNo(uint16_t macSeqNo)
	{
		std::map<uint16_t, uint16_t>::iterator it = this->packets.find(macSeqNo);
		if (it != this->packets.end())
			return it->second;
		return 0;
	}

	/*
	 * Getter for statistics
	 */
	double EEBTPPacketManager::getTotalEnergyConsumed(uint64_t gid)
	{
		double totalEnergy = 0;
		for (uint8_t i = 0; i < 8; i++)
		{
			totalEnergy += this->getEnergyByRecvFrame(gid, i);
			totalEnergy += this->getEnergyBySentFrame(gid, i);
		}
		return totalEnergy;
	}

	double EEBTPPacketManager::getEnergyByRecvFrame(uint64_t gid, uint8_t ft)
	{
		return this->frameEnergyRecv[gid][ft];
	}

	double EEBTPPacketManager::getEnergyBySentFrame(uint64_t gid, uint8_t ft)
	{
		return this->frameEnergySent[gid][ft];
	}

	uint32_t EEBTPPacketManager::getDataRecv(uint64_t gid)
	{
		return this->dataRecv[gid];
	}

	uint32_t EEBTPPacketManager::getDataSent(uint64_t gid)
	{
		return this->dataSent[gid];
	}

	uint32_t EEBTPPacketManager::getDataRecvByFrame(uint64_t gid, uint8_t ft)
	{
		return this->frameDataRecv[gid][ft];
	}

	uint32_t EEBTPPacketManager::getDataSentByFrame(uint64_t gid, uint8_t ft)
	{
		return this->frameDataSent[gid][ft];
	}

	uint32_t EEBTPPacketManager::getFrameTypeRecv(uint64_t gid, uint8_t ft)
	{
		return this->frameTypesRecv[gid][ft];
	}

	uint32_t EEBTPPacketManager::getFrameTypeSent(uint64_t gid, uint8_t ft)
	{
		return this->frameTypesSent[gid][ft];
	}
}
