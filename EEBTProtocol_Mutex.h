/*
 * SimpleBroadcastProtocol.h
 *
 *  Created on: 17.06.2020
 *      Author: krassus
 */

#ifndef BROADCAST_EEBTPPROTOCOLMUTEX_H_
#define BROADCAST_EEBTPPROTOCOLMUTEX_H_

#include "map"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"

#include "GameState.h"
#include "SeqNoCache.h"
#include "EEBTPHeader.h"
#include "EEBTProtocol.h"

namespace ns3
{
	class EEBTProtocolMutex : public EEBTProtocol
	{
	public:
		EEBTProtocolMutex();
		virtual ~EEBTProtocolMutex();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;

		void Install(Ptr<WifiNetDevice> netDevice, Ptr<CycleWatchDog> cwd);

		//void Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, double txPower);
		//void Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, uint16_t seqNo, double txPower);
		void Send(Ptr<GameState> gs, Ptr<EEBTPNode> receiver, Mac48Address originator, Mac48Address newOriginator, Mac48Address childLockFinishedOrg);
		void Send(Ptr<GameState> gs, Ptr<EEBTPNode> receiver, Mac48Address originator, Mac48Address newOriginator, Mac48Address childLockFinishedOrg, uint16_t seqNo, double txPower, Ptr<MutexSendEvent> event);
		void Send(Ptr<GameState> gs, EEBTPHeader header, Mac48Address recipient, double txPower, bool isRetransmission);

		void Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType);

	protected:
		void handleCycleCheck(Ptr<GameState> gs, Ptr<EEBTPNode> node, Ptr<EEBTPNode> originator, Ptr<EEBTPNode> newOriginator, Ptr<EEBTPNode> unused);
		void handleNeighborDiscovery(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		void handleChildRequest(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		void handleChildConfirmation(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		void handleChildRejection(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		void handleParentRevocation(Ptr<GameState> gs, Ptr<EEBTPNode> node);

		void disconnectOldParent(Ptr<GameState> gs);
		void contactNode(Ptr<GameState> gs, Ptr<EEBTPNode> node);

	private:
		bool lockChildNodes(Ptr<GameState> gs);
		void unlockChildNodes(Ptr<GameState> gs);

		void checkNodeLocks(Ptr<GameState> gs);
	};
}

#endif /* BROADCAST_EEBTPPROTOCOLMUTEX_H_ */
