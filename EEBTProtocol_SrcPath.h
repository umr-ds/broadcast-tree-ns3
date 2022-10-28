/*
 * SimpleBroadcastProtocol.h
 *
 *  Created on: 17.06.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_EEBTPPROTOCOL_SRCPATH_H_
#define BROADCAST_EEBTPPROTOCOL_SRCPATH_H_

#include "map"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"

#include "GameState.h"
#include "SeqNoCache.h"
#include "EEBTPHeader.h"
#include "EEBTProtocol.h"
#include "EEBTPHeader_SrcPath.h"

namespace ns3
{
	class GameState;
	class EEBTPNode;
	class CycleWatchDog;

	class EEBTProtocolSrcPath : public EEBTProtocol
	{
	public:
		EEBTProtocolSrcPath();
		virtual ~EEBTProtocolSrcPath();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;

		void Install(Ptr<WifiNetDevice> netDevice, Ptr<CycleWatchDog> cwd);

		void Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, double txPower);
		void Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, uint16_t seqNo, double txPower, Ptr<SendEvent> event);
		void Send(Ptr<GameState> gs, EEBTPHeaderSrcPath header, Mac48Address recipient, double txPower, bool isRetransmission);

		void Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType);

		void checkParentPathStatus(Ptr<GameState> gs);

	protected:
		void handleCycleCheck(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		void handleNeighborDiscovery(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		void handleChildRequest(Ptr<GameState> gs, Ptr<EEBTPNode> node);
		void handleChildConfirmation(Ptr<GameState> gs, Ptr<EEBTPNode> node);

		void contactNode(Ptr<GameState> gs, Ptr<EEBTPNode> node);

		bool checkParentPath(Ptr<GameState> gs);

	private:
		uint32_t timeToWait;
	};
}

#endif /* BROADCAST_EEBTPPROTOCOL_SRCPATH_H_ */
