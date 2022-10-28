/*
 *	EEBTProtocol.h
 *
 *  Created on: 02.06.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_EEBTPPROTOCOL_H_
#define BROADCAST_EEBTPPROTOCOL_H_

#include "map"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/energy-module.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/wifi-radio-energy-model-helper.h"

#include "GameState.h"
#include "SeqNoCache.h"
#include "EEBTPHeader.h"
#include "CycleWatchDog.h"
#include "EEBTPPacketManager.h"

namespace ns3
{
	class GameState;
	class EEBTPNode;
	class CycleWatchDog;

	enum FRAME_TYPE : uint8_t
	{
		CYCLE_CHECK = 0,
		NEIGHBOR_DISCOVERY = 1,
		CHILD_REQUEST = 2,
		CHILD_CONFIRMATION = 3,
		CHILD_REJECTION = 4,
		PARENT_REVOCATION = 5,
		END_OF_GAME = 6,
		APPLICATION_DATA = 7
	};

	class EEBTProtocol : public Object
	{
	public:
		EEBTProtocol();
		virtual ~EEBTProtocol();

		static TypeId GetTypeId();
		static const uint16_t PROT_NUMBER;
		static const uint32_t MAX_UNCHANGED_ROUNDS;
		friend std::ostream &operator<<(std::ostream &os, EEBTProtocol &prot);
		virtual void Print(std::ostream &os) const;
		virtual TypeId GetInstanceTypeId() const;

		Ptr<NetDevice> GetDevice();

		virtual void Send(Ptr<GameState> gs, EEBTPHeader header, Mac48Address recipient, double txPower);
		virtual void Send(Ptr<GameState> gs, EEBTPHeader header, Mac48Address recipient, double txPower, bool isRetransmission);

		virtual void Send(Ptr<GameState> gs, FRAME_TYPE ft, double txPower);
		virtual void Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, double txPower);
		virtual void Send(Ptr<GameState> gs, Mac48Address originator, Mac48Address newParent, Mac48Address oldParent);

		virtual void Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, uint16_t seqNo, double txPower, Ptr<SendEvent> event);
		virtual void Send(Ptr<GameState> gs, Mac48Address originator, Mac48Address newParent, Mac48Address oldParent, uint16_t seqNo, double txPower, Ptr<CCSendEvent> event);

		void Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType);

		virtual void Install(Ptr<WifiNetDevice> netDevice, Ptr<CycleWatchDog> cwd);

		Ptr<GameState> getGameState(uint64_t gid);
		virtual void removeGameState(uint64_t gid);
		virtual Ptr<GameState> initGameState(uint64_t gid);

		int maxPackets;

		double getEnergyForConstruction(uint64_t gid);
		double getEnergyForApplicationData(uint64_t gid);
		double getEnergyByFrameType(uint64_t gid, FRAME_TYPE ft);

		Ptr<CycleWatchDog> getCycleWatchDog();
		Ptr<EEBTPPacketManager> getPacketManager();

		void finishGame(Ptr<GameState> gs);
		virtual void contactCheapestNeighbor(Ptr<GameState> gs);

		void sendApplicationData(Ptr<GameState> gs, uint16_t seqNo);
		void sendApplicationData(Ptr<GameState> gs, Ptr<Packet> packet);

	protected:
		double maxAllowedTxPower;

		int sendCounter;
		int dataLength;

		int64_t ndInterval;

		Ptr<WifiPhy> wifiPhy;
		Ptr<WifiNetDevice> device;
		Ptr<TrafficControlLayer> tcl;
		Ptr<CycleWatchDog> cycleWatchDog;

		SeqNoCache cache;

		Mac48Address myAddress;
		Ptr<UniformRandomVariable> random;

		void updateEnergyConsumption(FRAME_TYPE ft, double energy);

		virtual void handleCycleCheck(Ptr<GameState> gs, Ptr<EEBTPNode> node, Ptr<EEBTPNode> originator, Ptr<EEBTPNode> newParent, Ptr<EEBTPNode> oldParent);
		virtual void handleNeighborDiscovery(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		virtual void handleChildRequest(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		virtual void handleChildConfirmation(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		virtual void handleChildRejection(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		virtual void handleParentRevocation(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		virtual void handleEndOfGame(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		virtual void handleApplicationData(Ptr<GameState> gs, Ptr<EEBTPNode> node, Ptr<Packet> packet);

		std::vector<Ptr<GameState>> games;
		Ptr<EEBTPPacketManager> packetManager;

		/*
			 * Helper methods
			 */
		void resetNeighborDiscoveryEvent();
		bool checkNeigborDiscoverySendEvent(Ptr<GameState> gs);

		void disconnectAllChildNodes(Ptr<GameState> gs);

		void disconnectOldParent(Ptr<GameState> gs);
		virtual void contactNode(Ptr<GameState> gs, Ptr<EEBTPNode> node);

		double calculateTxPower(double rxPower, double txPower, double noise, double minSNR);
	};
}

#endif /* BROADCAST_EEBTPPROTOCOL_H_ */
