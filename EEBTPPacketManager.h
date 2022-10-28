/*
 * EEBTPTxQueue.h
 *
 *  Created on: 15.08.2020
 *      Author: krassus
 */

#ifndef BROADCAST_EEBTPTXQUEUE_H_
#define BROADCAST_EEBTPTXQUEUE_H_

#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/energy-module.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"
#include "ns3/llc-snap-header.h"
#include "ns3/wifi-radio-energy-model-helper.h"

#include "ns3/EEBTPTag.h"

namespace ns3
{
	class EEBTPPacketManager : public Object
	{
	public:
		EEBTPPacketManager();
		~EEBTPPacketManager();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;

		void setDevice(Ptr<WifiNetDevice> device);

		void sendPacket(Ptr<Packet> pkt, Mac48Address recipient);

		void onRxStart(Ptr<const Packet> packet);
		void onRxEnd(Ptr<const Packet> packet);

		void onTx(Ptr<const Packet> packet);
		void onTxStart(Ptr<const Packet> packet, double txPowerW);
		void onTxDrop(Ptr<const Packet> packet);
		void onTxEnd(Ptr<const Packet> packet);

		void onPacketTx(Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu);
		void onPacketRx(Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise);

		void onTxFailed(const WifiMacHeader &header);
		void onTxDropped(Ptr<const Packet> packet);
		void onTxSuccessful(const WifiMacHeader &header);

		void onTxFinalRtsFailed(Mac48Address address);
		void onTxFinalDataFailed(Mac48Address address);

		bool isPacketAcked(uint16_t seqNo);
		bool isPacketLost(uint16_t seqNo);
		void deleteSeqNoEntry(uint16_t seqNo);

		uint16_t getSeqNoByMacSeqNo(uint16_t macSeqNo);

		EEBTPTag createPacketTag(Ptr<Packet> packet, WifiTxVector txVector, SignalNoiseDbm signalNoise);
		EEBTPTag getPacketTag(uint16_t seqNo);

		double getTotalEnergyConsumed(uint64_t gid);
		double getEnergyByRecvFrame(uint64_t gid, uint8_t ft);
		double getEnergyBySentFrame(uint64_t gid, uint8_t ft);

		uint32_t getDataRecv(uint64_t gid);
		uint32_t getDataSent(uint64_t gid);

		uint32_t getDataRecvByFrame(uint64_t gid, uint8_t ft);
		uint32_t getDataSentByFrame(uint64_t gid, uint8_t ft);

		uint32_t getFrameTypeRecv(uint64_t gid, uint8_t ft);
		uint32_t getFrameTypeSent(uint64_t gid, uint8_t ft);

	private:
		Ptr<WifiNetDevice> device;
		Ptr<TrafficControlLayer> tcl;
		Ptr<EnergySource> energySource;

		std::map<uint16_t, uint16_t> packets;
		std::map<Mac48Address, uint16_t> receiver;
		std::map<uint16_t, bool> packetsAcked;
		std::map<uint16_t, bool> packetsLost;
		std::map<uint16_t, EEBTPTag> packetTag;
		std::map<Mac48Address, std::vector<uint16_t>> addrSeqCache;

		double energyAtStart;
		uint16_t seqNoAtStart;

		std::map<uint64_t, std::map<uint8_t, double>> frameEnergySent;
		std::map<uint64_t, std::map<uint8_t, double>> frameEnergyRecv;
		std::map<uint64_t, uint32_t> dataSent;
		std::map<uint64_t, uint32_t> dataRecv;
		std::map<uint64_t, std::map<uint8_t, uint32_t>> frameDataSent;
		std::map<uint64_t, std::map<uint8_t, uint32_t>> frameDataRecv;
		std::map<uint64_t, std::map<uint8_t, uint32_t>> frameTypesSent;
		std::map<uint64_t, std::map<uint8_t, uint32_t>> frameTypesRecv;
	};
}

#endif /* BROADCAST_EEBTPTXQUEUE_H_ */
