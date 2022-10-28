/*
 * SimpleBroadcastProtocol.h
 *
 *  Created on: 06.05.2020
 *      Author: krassus
 */

#ifndef BROADCAST_SIMPLEBROADCASTPROTOCOL_H_
#define BROADCAST_SIMPLEBROADCASTPROTOCOL_H_

#include "map"
#include "ns3/wifi-phy.h"
#include "ns3/energy-module.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/traffic-control-layer.h"

#include "SimpleBroadcastHeader.h"

namespace ns3
{
	class SimpleBroadcastProtocol : public Object
	{
	public:
		SimpleBroadcastProtocol();
		virtual ~SimpleBroadcastProtocol();

		static TypeId GetTypeId();
		static const uint16_t PROT_NUMBER;
		virtual TypeId GetInstanceTypeId() const;

		void Send(uint16_t hopCount, uint32_t seqNo);
		void Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType);

		double getRecvEnergy();
		double getSentEnergy();
		uint32_t getRecvPackets();
		uint32_t getSentPackets();
		uint32_t getUniquePackets();
		uint32_t getRecvData();
		uint32_t getSentData();

		void Install(Ptr<WifiNetDevice> netDevice);

		void onRxStart(Ptr<const Packet> packet);
		void onRxEnd(Ptr<const Packet> packet);

		void onTxStart(Ptr<const Packet> packet, double txPowerW);
		void onTxDrop(Ptr<const Packet> packet);
		void onTxEnd(Ptr<const Packet> packet);

		void onTxFailed(const WifiMacHeader &header);
		void onTxDropped(Ptr<const Packet> packet);
		void onTxSuccessful(const WifiMacHeader &header);

		void onEnergyChanged(double oldValue, double newValue);
		void onPhyStateChanged(Time start, Time duration, WifiPhyState state);

	private:
		std::map<Address, std::vector<uint32_t>> cache;
		Ptr<WifiNetDevice> device;
		Ptr<TrafficControlLayer> tcl;
		double maxTxPower;

		Ptr<EnergySource> energySource;
		double energyAtStart;

		double energyRecv;
		double energySent;

		uint32_t uniquePackets;
		uint32_t packetsRecv;
		uint32_t packetsSent;

		uint32_t dataRecv;
		uint32_t dataSent;
	};
}

#endif /* BROADCAST_SIMPLEBROADCASTPROTOCOL_H_ */
