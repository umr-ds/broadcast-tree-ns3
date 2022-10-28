/*
 * SimpleBroadcastProtocol.cc
 *
 *  Created on: 02.06.2020
 *      Author: krassus
 */

#include <math.h>

#include "ns3/EEBTPTag.h"
#include "SeqNoCache.h"
#include "SendEvent.h"
#include "EEBTPQueueDiscItem.h"
#include "EEBTPHeader_SrcPath.h"
#include "ParentPathCheckEvent.h"
#include "EEBTProtocol_SrcPath.h"

#include "float.h"
#include "ns3/nstime.h"
#include "ns3/mac-low.h"
#include "ns3/log.h"
#include "ns3/integer.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/mobility-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/threshold-preamble-detection-model.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("EEBTProtocolSrcPath");
	NS_OBJECT_ENSURE_REGISTERED(EEBTProtocolSrcPath);

	EEBTProtocolSrcPath::EEBTProtocolSrcPath() : EEBTProtocol()
	{
		this->timeToWait = 0;
	}

	EEBTProtocolSrcPath::~EEBTProtocolSrcPath()
	{
	}

	TypeId EEBTProtocolSrcPath::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::EEBTProtocolSrcPath").SetParent<Object>().AddConstructor<EEBTProtocolSrcPath>();
		return tid;
	}

	TypeId EEBTProtocolSrcPath::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	/*
	 * Install method to install this protocol on the stack of a node
	 */
	void EEBTProtocolSrcPath::Install(Ptr<WifiNetDevice> netDevice, Ptr<CycleWatchDog> cwd)
	{
		this->device = netDevice;
		this->cycleWatchDog = cwd;
		this->random = CreateObject<UniformRandomVariable>();
		this->wifiPhy = this->device->GetMac()->GetWifiPhy();
		this->myAddress = Mac48Address::ConvertFrom(this->device->GetAddress());

		//Set the number of power levels to one to ensure the node sends with a constant power
		this->wifiPhy->SetNTxPower(1);

		//Set the max allowed transmission power according to the selected standard
		switch (this->wifiPhy->GetStandard())
		{
		case WIFI_PHY_STANDARD_80211a:
		case WIFI_PHY_STANDARD_80211n_5GHZ:
			this->maxAllowedTxPower = 23.0;
			break;
		case WIFI_PHY_STANDARD_80211b:
		case WIFI_PHY_STANDARD_80211g:
		case WIFI_PHY_STANDARD_80211n_2_4GHZ:
		default:
			this->maxAllowedTxPower = 20.0;
			break;
		}

		this->wifiPhy->SetTxPowerStart(this->maxAllowedTxPower);
		this->wifiPhy->SetTxPowerEnd(this->maxAllowedTxPower);

		//Register a callback for incoming packets to read the rxPower, SNR and noise levels
		this->packetManager = Create<EEBTPPacketManager>();
		this->packetManager->setDevice(this->device);
		this->wifiPhy->TraceConnectWithoutContext("PhyRxBegin", MakeCallback(&EEBTPPacketManager::onRxStart, this->packetManager));
		this->wifiPhy->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&EEBTPPacketManager::onRxEnd, this->packetManager));
		this->wifiPhy->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&EEBTPPacketManager::onTxStart, this->packetManager));
		this->wifiPhy->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&EEBTPPacketManager::onTxDrop, this->packetManager));
		this->wifiPhy->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&EEBTPPacketManager::onTxEnd, this->packetManager));
		this->wifiPhy->TraceConnectWithoutContext("MonitorSnifferRx", MakeCallback(&EEBTPPacketManager::onPacketRx, this->packetManager));
		this->wifiPhy->TraceConnectWithoutContext("MonitorSnifferTx", MakeCallback(&EEBTPPacketManager::onPacketTx, this->packetManager));
		this->device->GetMac()->TraceConnectWithoutContext("TxOkHeader", MakeCallback(&EEBTPPacketManager::onTxSuccessful, this->packetManager));
		this->device->GetMac()->TraceConnectWithoutContext("MacTxDrop", MakeCallback(&EEBTPPacketManager::onTxDropped, this->packetManager));
		this->device->GetMac()->TraceConnectWithoutContext("MacTx", MakeCallback(&EEBTPPacketManager::onTx, this->packetManager));
		this->device->GetMac()->TraceConnectWithoutContext("TxErrHeader", MakeCallback(&EEBTPPacketManager::onTxFailed, this->packetManager));

		this->ndInterval = this->device->GetMac()->GetSlot().GetMicroSeconds() * 2000;
		this->timeToWait = this->device->GetMac()->GetAckTimeout().GetMicroSeconds() * 100 * 1.5;

		TrafficControlHelper tch = TrafficControlHelper();
		tch.Install(this->device);

		this->tcl = this->device->GetNode()->GetObject<TrafficControlLayer>();
		this->device->GetNode()->RegisterProtocolHandler(MakeCallback(&TrafficControlLayer::Receive, this->tcl), EEBTProtocol::PROT_NUMBER, this->device);
		this->tcl->RegisterProtocolHandler(MakeCallback(&EEBTProtocolSrcPath::Receive, this), EEBTProtocol::PROT_NUMBER, this->device);

		//this->device->GetNode()->RegisterProtocolHandler(MakeCallback(&EEBTProtocolSrcPath::Receive, this), EEBTProtocol::PROT_NUMBER, this->device);
	}

	/*
	 * Receive
	 */
	void EEBTProtocolSrcPath::Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType)
	{
		EEBTPTag tag;
		EEBTPHeaderSrcPath header;
		Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(device);
		Mac48Address sender_addr = Mac48Address::ConvertFrom(sender);

		//Get packet header
		packet->PeekHeader(header);

		//Get packet tag
		tag = this->packetManager->getPacketTag(header.GetSequenceNumber());

		NS_LOG_DEBUG("[Node " << dev->GetNode()->GetId() << " / " << Now() << "]: [" << this->myAddress << "] received packet from " << sender_addr << " / SeqNo: " << header.GetSequenceNumber() << " / FRAME_TYPE: " << (int)header.GetFrameType() << " / txPower: " << header.GetTxPower() << "dBm / noise: " << tag.getNoise() << "dBm");
		NS_LOG_DEBUG("[Node " << dev->GetNode()->GetId() << "]: hTx: " << header.GetHighestMaxTxPower() << ", shTx: " << header.GetSecondHighestMaxTxPower());

		//Check for duplicated sequence number
		if (this->cache.checkForDuplicate(sender_addr, header.GetSequenceNumber()))
		{
			NS_LOG_DEBUG("Received duplicated frame from '" << sender_addr << "' with GID " << header.GetGameId() << " and SeqNo " << header.GetSequenceNumber());
			return;
		}

		//Get the GameState
		Ptr<GameState> gs = this->getGameState(header.GetGameId());

		//Update frame type seq no
		if (gs->checkLastFrameType(sender_addr, header.GetFrameType(), header.GetSequenceNumber()))
			gs->updateLastFrameType(sender_addr, header.GetFrameType(), header.GetSequenceNumber());
		else
		{
			NS_LOG_DEBUG("\tReceived a frame with the same type and a higher sequence number earlier. Ignoring this packet");
			return;
		}

		//Update frame type seq no
		gs->updateLastFrameType(sender_addr, header.GetFrameType(), header.GetSequenceNumber());

		//Check if node is in our neighbor list
		if (!gs->isNeighbor(sender_addr))
			gs->addNeighbor(sender_addr);

		//Get the EEBTPNode
		Ptr<EEBTPNode> node = gs->getNeighbor(sender_addr);

		if (header.GetFrameType() != APPLICATION_DATA)
		{
			node->setParentAddress(node->getAddress());
			node->setReachPower(this->calculateTxPower(tag.getSignal(), header.GetTxPower(), tag.getNoise(), tag.getMinSNR()));
			node->updateRxInfo(tag.getSignal(), tag.getNoise());
			node->setHighestMaxTxPower(header.GetHighestMaxTxPower());
			node->setSecondHighestMaxTxPower(header.GetSecondHighestMaxTxPower());
			node->setFinished(header.getGameFinishedFlag());
			gs->findHighestTxPowers();

			//If the node has a reachpower that is higher than our maximum allowed txPower
			if (node->getReachPower() > this->maxAllowedTxPower)
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: ReachPower for node [" << node->getAddress() << "] is too high: " << node->getReachPower() << "dBm > " << this->maxAllowedTxPower << "dBm; FRAME_TYPE: " << (uint)header.GetFrameType());

				//Mark node with the reach problem flag
				node->hasReachPowerProblem(true);

				//If this node is a child of us
				if (gs->isChild(node) && header.GetFrameType() != PARENT_REVOCATION)
				{
					gs->updateLastFrameType(sender_addr, PARENT_REVOCATION, header.GetSequenceNumber());

					//Handle as parent revocation
					this->handleParentRevocation(gs, node);

					//And reject the connection
					this->Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
					return;
				}
			}
			else
			{
				node->hasReachPowerProblem(false);
				//NS_LOG_DEBUG("ReachPower for node [" << node->getAddress() << "]: " << node->getReachPower() << "dBm | hTx: " << node->getHighestMaxTxPower() << "dBm, shTx: " << node->getSecondHighestMaxTxPower() << "dBm");
			}

			//If node had receiving problems
			if (header.hadReceivingProblems())
			{
				node->hasReachPowerProblem(true);
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "/" << Now() << "]: Recipient had receive problems. New frame type is " << (uint)(header.GetFrameType() & 0b01111111));

				//and node is a child of us
				if (gs->isChild(node) && header.GetFrameType() != PARENT_REVOCATION)
				{
					//and its reachpower is too high
					if (node->getReachPower() > this->maxAllowedTxPower)
					{
						gs->updateLastFrameType(sender_addr, PARENT_REVOCATION, header.GetSequenceNumber());

						//Handle as parent revocation
						this->handleParentRevocation(gs, node);

						//And reject the connection
						this->Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
						return;
					}
					else
					{
						//send out updated information
						gs->resetUnchangedCounter();
						EEBTProtocol::Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);
					}
				}
			}
		}

		switch (header.GetFrameType())
		{
		case CYCLE_CHECK:
			if (header.getSrcPath().size() > 0)
				node->setSrcPath(header.getSrcPath());
			this->handleCycleCheck(gs, node);
			break;
		case NEIGHBOR_DISCOVERY:
			//Update path to source of node
			if (header.getSrcPath().size() > 0)
				node->setSrcPath(header.getSrcPath());
			this->handleNeighborDiscovery(gs, node);
			break;
		case CHILD_REQUEST:
			this->handleChildRequest(gs, node);
			break;
		case CHILD_CONFIRMATION:
			//Update path to source of node
			if (header.getSrcPath().size() > 0)
				node->setSrcPath(header.getSrcPath());
			this->handleChildConfirmation(gs, node);
			break;
		case CHILD_REJECTION:
			this->handleChildRejection(gs, node);
			break;
		case PARENT_REVOCATION:
			this->handleParentRevocation(gs, node);
			break;
		case END_OF_GAME:
			this->handleEndOfGame(gs, node);
			break;
		case APPLICATION_DATA:
			this->handleApplicationData(gs, node, packet->Copy());
			break;
		default:
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Invalid frame type: " << (int)header.GetFrameType());
		}

		node->resetPathChanged();
		node->resetReachPowerChanged();

		//Print new line to separate events
		NS_LOG_DEBUG("\n");
	}

	/*
	 * Send methods
	 */
	void EEBTProtocolSrcPath::Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, double txPower)
	{
		EEBTPHeaderSrcPath header;
		header.SetFrameType(ft);

		if (ft == CHILD_REJECTION && gs->isChild(recipient))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Node [" << recipient << "] is a child of mine, but it is marked as a child...");
			gs->removeChild(gs->getNeighbor(recipient));
		}

		this->Send(gs, header, recipient, txPower, false);
	}

	//Send method for SendEvent to check if a packet got lost
	void EEBTProtocolSrcPath::Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, uint16_t seqNo, double txPower, Ptr<SendEvent> event)
	{
		if (ft != NEIGHBOR_DISCOVERY)
		{
			Ptr<EEBTPNode> node = gs->getNeighbor(recipient);
			if (node == 0)
				node = Create<EEBTPNode>(recipient, this->maxAllowedTxPower);

			if (ft == CHILD_REQUEST && gs->getContactedParent() != node)
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " will not be retransmitted since the recipient is not our contacted parent");
				return;
			}

			if (this->packetManager->isPacketAcked(seqNo))
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has been acked. No retransmission, time = " << Now());
				this->packetManager->deleteSeqNoEntry(seqNo);
				return;
			}
			else if (this->packetManager->isPacketLost(seqNo) || event->getNTimes() > 20)
			{
				if (event->getNTimes() > 20)
				{
					if (ft == CHILD_REQUEST && txPower >= this->maxAllowedTxPower)
					{
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has not been acked yet. Cancle..., time = " << Now());
						this->handleChildRejection(gs, gs->getNeighbor(recipient));
					}
					else
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has not been acked yet. Retransmitting..., time = " << Now());
				}
				else
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has been lost. Retransmitting..., time = " << Now());
				this->packetManager->deleteSeqNoEntry(seqNo);

				EEBTPHeaderSrcPath header;
				header.SetFrameType(ft);
				header.SetSequenceNumber(seqNo);
				this->Send(gs, header, recipient, txPower + 1, true);
			}
			else
			{
				Time ttw = this->device->GetMac()->GetAckTimeout() * 200;
				Simulator::Schedule(ttw, event);
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Rescheduled SendEvent(" << seqNo << ") for " << (Now() + ttw) << ", now = " << Now());
			}
		}
		else
		{
			//NS_FATAL_ERROR("[Node " << this->device->GetNode()->GetId() << "]: Invalid frametype to send: " << (uint)ft);
			this->Send(gs, ft, recipient, txPower);
		}
	}

	void EEBTProtocolSrcPath::Send(Ptr<GameState> gs, EEBTPHeaderSrcPath header, Mac48Address recipient, double txPower, bool isRetransmission)
	{
		//Adjust the transmission power
		header.setReceivingProblems(false);
		if (txPower > this->maxAllowedTxPower)
		{
			txPower = this->maxAllowedTxPower;
			header.setReceivingProblems(true);
		}
		gs->findHighestTxPowers();

		//Set sequence number
		if (!isRetransmission)
			this->cache.injectSeqNo(&header);

		if (header.GetFrameType() == CYCLE_CHECK || header.GetFrameType() == NEIGHBOR_DISCOVERY || header.GetFrameType() == CHILD_CONFIRMATION)
		{
			if (header.GetFrameType() == NEIGHBOR_DISCOVERY)
			{
				if (gs->gameFinished())
					gs->resetNeighborDiscoveryEvent();
				else if (!this->checkNeigborDiscoverySendEvent(gs))
					return;
			}

			std::vector<Mac48Address> path;
			if (gs->getParent() != 0 || gs->getContactedParent() != 0)
			{
				if (gs->getParent() != 0)
					path = gs->getParent()->getSrcPath();
				else if (gs->getContactedParent() != 0)
					path = gs->getContactedParent()->getSrcPath();
				for (uint i = 0; i < path.size(); i++)
					header.addNodeToPath(path[i]);

				header.addNodeToPath(this->myAddress);
			}
			else if (gs->isInitiator())
				header.addNodeToPath(this->myAddress);
		}

		if (header.GetFrameType() == CYCLE_CHECK || header.GetFrameType() == CHILD_REQUEST || header.GetFrameType() == CHILD_CONFIRMATION ||
			header.GetFrameType() == CHILD_REJECTION || header.GetFrameType() == PARENT_REVOCATION || header.GetFrameType() == END_OF_GAME)
		{
			Time ttw = this->device->GetMac()->GetAckTimeout() * 100;
			EventId id = Simulator::Schedule(ttw, Create<SendEvent>(gs, this, (FRAME_TYPE)header.GetFrameType(), recipient, txPower, header.GetSequenceNumber()));
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Scheduled SendEvent(" << (uint)header.GetFrameType() << ") for " << (Now() + ttw) << ". EventID = " << id.GetUid());
		}

		//Packet size must be greater than 0 to prevent this error in visualized mode:
		//	assert failed. cond="m_current >= m_dataStart && m_current < m_dataEnd"
		Ptr<Packet> packet = Create<Packet>(1);

		header.SetGameId(gs->getGameID());
		header.SetTxPower(txPower);

		header.SetHighestMaxTxPower(gs->getHighestTxPower());
		header.SetSecondHighestMaxTxPower(gs->getSecondHighestTxPower());

		header.setGameFinishedFlag((gs->hasChilds() && gs->allChildsFinished()) || gs->gameFinished());

		packet->AddHeader(header);

		/*Ptr<EEBTPQueueDiscItem> qdi = Create<EEBTPQueueDiscItem>(packet, recipient, EEBTProtocol::PROT_NUMBER);
		this->tcl->Send(this->device, qdi);*/

		EEBTPTag tag;
		tag.setGameID(gs->getGameID());
		tag.setFrameType(header.GetFrameType());
		tag.setSequenceNumber(header.GetSequenceNumber());
		tag.setTxPower(header.GetTxPower());
		packet->AddPacketTag(tag);

		this->packetManager->sendPacket(packet, recipient);

		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: EEBTProtocolSrcPath::Send(): " << this->myAddress << " => " << recipient << " / SeqNo: " << header.GetSequenceNumber() << " / FRAME_TYPE: " << (uint)header.GetFrameType() << " / txPower: " << header.GetTxPower() << "/" << this->wifiPhy->GetTxPowerStart() << "|" << this->wifiPhy->GetTxPowerEnd() << "dBm");
		NS_LOG_DEBUG("\thTx: " << header.GetHighestMaxTxPower() << ", shTx: " << header.GetSecondHighestMaxTxPower() << ", rounds: " << gs->getUnchangedCounter() << "/" << ((gs->getNNeighbors() * 0.5) + 2));
		if (gs->getParent() != 0)
		{
			std::stringstream str;
			std::vector<Mac48Address> path = gs->getParent()->getSrcPath();
			for (auto addr : path)
			{
				str << addr;
				str << " <= ";
			}
			std::string s = str.str();
			if (s.size() > 4)
				s.erase(s.end() - 4, s.end());
			NS_LOG_DEBUG("\tPath: " << s);
		}
	}

	/*
	 * Handle the cycle check (FrameType 0)
	 */
	void EEBTProtocolSrcPath::handleCycleCheck(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		if (gs->isChild(node))
		{
			this->Send(gs, CYCLE_CHECK, node->getAddress(), node->getReachPower());
		}
		else if (node == gs->getParent())
		{
			gs->setLastParentUpdate(Now());
			if (!gs->isInitiator() && this->checkParentPath(gs) && node->pathChanged() &&
				(gs->getNeighborDiscoveryEvent() == 0 && gs->getContactedParent() == 0))
			{
				gs->resetParentUnchangedCounter();
				this->Send(gs, NEIGHBOR_DISCOVERY, Mac48Address::GetBroadcast(), this->maxAllowedTxPower);
				if (gs->gameFinished())
					this->Send(gs, NEIGHBOR_DISCOVERY, Mac48Address::GetBroadcast(), this->maxAllowedTxPower);
			}
			else
				gs->incrementParentUnchangedCounter();

			if (gs->getPPCEvent() != 0)
				gs->getPPCEvent()->Cancel();
			gs->setPPCEvent(Create<ParentPathCheckEvent>(gs, this));
			Simulator::Schedule((Now() + MilliSeconds(this->timeToWait)), gs->getPPCEvent());
		}
	}

	/*
	 * Handle neighbor discovery (FrameType 1)
	 */
	void EEBTProtocolSrcPath::handleNeighborDiscovery(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//NS_LOG_DEBUG(this->device->GetNode()->GetId() << " => EEBTProtocolSrcPath::handleNeighborDiscovery()");

		if (!gs->isInitiator() && this->checkParentPath(gs))
		{
			EEBTProtocol::handleNeighborDiscovery(gs, node);

			if (gs->getParent() == node)
			{
				gs->setLastParentUpdate(Now());

				//If the node is our parent, our parent's path has changed and we do not send neighbor discovery frames anymore
				if (gs->getContactedParent() == 0)
				{
					if (node->pathChanged())
					{
						gs->resetParentUnchangedCounter();
						if (gs->getPPCEvent() == 0)
							gs->setPPCEvent(Create<ParentPathCheckEvent>(gs, this));
						else if (!gs->getPPCEvent()->IsCancelled())
							Simulator::Schedule((Now() + MilliSeconds(this->timeToWait)), gs->getPPCEvent());
					}
					else
						gs->incrementParentUnchangedCounter();
				}
			}
		}
	}

	void EEBTProtocolSrcPath::handleChildRequest(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		Ptr<EEBTPNode> me = Create<EEBTPNode>(this->myAddress, 0);
		if (gs->getParent() != 0)
			me->setSrcPath(gs->getParent()->getSrcPath());
		else if (gs->getContactedParent() != 0)
			me->setSrcPath(gs->getContactedParent()->getSrcPath());
		if (me->isOnPath(node->getAddress()))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child request from [" << node->getAddress() << "] dismissed because it is on my parents path to the source node");
			return this->Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
		}
		else
			EEBTProtocol::handleChildRequest(gs, node);
	}

	/*
	 * Handle child confirmation (FrameType 3)
	 * Sent by parent, received by child
	 * if parent accepts the child request
	 */
	void EEBTProtocolSrcPath::handleChildConfirmation(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//NS_LOG_DEBUG(this->device->GetNode()->GetId() << " => EEBTProtocolSrcPath::handleChildConfirmation()");

		if (gs->checkLastFrameType(node->getAddress(), CHILD_CONFIRMATION, gs->getLastSeqNo(node->getAddress(), CHILD_REJECTION)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child confirmation from [" << node->getAddress() << "] dismissed because we received a CHILD_REJECTION with a higher sequence number earlier.");
			return;
		}

		//If we have a parent... (!Assertion)
		if (gs->getParent() != 0)
		{
			//...and we did not contact a new one, send parent revocation to sender
			if (gs->getContactedParent() != node)
			{
				if (gs->getParent() == node)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child confirmation from [" << node->getAddress() << "] ignored because we already connected to that node");
					return;
				}
				else
					return this->Send(gs, PARENT_REVOCATION, node->getAddress(), node->getReachPower());
			}
			else if (gs->gameFinished()) //...and the game is already finished, disconnect from old parent
			{
				this->disconnectOldParent(gs);
			}
		}
		else if (node != gs->getContactedParent())
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child confirmation from [" << node->getAddress() << "] dismissed because we did not ask him to be our parent");
			return this->Send(gs, PARENT_REVOCATION, node->getAddress(), node->getReachPower());
		}

		//Set new parent
		gs->setParent(node);
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Connected to [" << node->getAddress() << "]");
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: rP(" << node->getAddress() << ") = " << node->getReachPower() << ", hTx(" << node->getAddress() << ") = " << node->getHighestMaxTxPower() << ", shTx(" << node->getAddress() << ") = " << node->getSecondHighestMaxTxPower());
		for (uint i = 0; i < gs->getNNeighbors(); i++)
		{
			Ptr<EEBTPNode> neighbor = gs->getNeighbor(i);
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]:\trP(" << neighbor->getAddress() << ") = " << neighbor->getReachPower() << ", hTx(" << neighbor->getAddress() << ") = " << neighbor->getHighestMaxTxPower() << ", shTx(" << neighbor->getAddress() << ") = " << neighbor->getSecondHighestMaxTxPower());
		}
		node->incrementConnCounter();

		this->cycleWatchDog->checkForCycles(gs->getGameID(), this->device);

		//Reset contacted parent
		gs->setContactedParent(0);

		if (gs->doIncrAfterConfirm())
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Incrementing unchanged counter");
			gs->incrementUnchangedCounter();
		}

		if (gs->getUnchangedCounter() < EEBTProtocol::MAX_UNCHANGED_ROUNDS)
		{
			//Check if we note a possible better parent in the past
			this->contactCheapestNeighbor(gs);
		}

		//If we did not contact another node...
		if (gs->getContactedParent() == 0 && this->checkParentPath(gs))
		{
			node->resetConnCounter();
			gs->setEmptyPathOnConnect(node->getSrcPath().size() == 0);

			if (node->hasFinished())
				this->handleEndOfGame(gs, node);

			//If we finished our game earlier, send END_OF_GAME to our parent
			if (gs->gameFinished())
				this->Send(gs, END_OF_GAME, gs->getParent()->getAddress(), gs->getParent()->getReachPower());

			gs->setLastParentUpdate(Now());
			gs->resetParentUnchangedCounter();
			if (gs->getPPCEvent() == 0)
				gs->setPPCEvent(Create<ParentPathCheckEvent>(gs, this));
			Simulator::Schedule((Now() + MilliSeconds(this->timeToWait)), gs->getPPCEvent());

			//Reset the unchanged counter, since our topology changed
			if (!gs->doIncrAfterConfirm() && gs->getUnchangedCounter() < EEBTProtocol::MAX_UNCHANGED_ROUNDS)
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Resetting unchanged counter");
				gs->resetUnchangedCounter();
			}

			gs->setDoIncrAfterConfirm(false);

			this->Send(gs, NEIGHBOR_DISCOVERY, Mac48Address::GetBroadcast(), this->maxAllowedTxPower);

			gs->resetRejectionCounter();
		}
	}

	/*
	 * Helper methods
	 */

	/*
	 * This method handles the CHILD_REQUEST to new parents
	 */
	void EEBTProtocolSrcPath::contactNode(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//First, check if we are on src path of the node
		if (node->isOnPath(this->myAddress))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: We are on the source path of node [" << node->getAddress() << "]");
			return;
		}

		EEBTProtocol::contactNode(gs, node);
	}

	bool EEBTProtocolSrcPath::checkParentPath(Ptr<GameState> gs)
	{
		Ptr<EEBTPNode> parent = gs->getParent();

		//If we have a parent and we are on the path of our parent => Cycle!
		if (parent != 0)
		{
			std::stringstream str;
			std::vector<Mac48Address> path = parent->getSrcPath();
			for (Mac48Address addr : path)
			{
				str << addr;
				str << " <= ";
			}
			std::string s = str.str();
			if (s.size() > 4)
				s.erase(s.end() - 4, s.end());
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Parent [" << parent->getAddress() << "] path: " << s);

			if (parent->isOnPath(this->myAddress) || (gs->hadEmptyPathOnConnect() && parent->getSrcPath().size() == 0))
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: I am on the path of my parent [" << parent->getAddress() << "] or the path is still empty");

				gs->resetParentUnchangedCounter();
				gs->resetNeighborDiscoveryEvent();

				this->disconnectOldParent(gs);

				this->contactCheapestNeighbor(gs);

				if (gs->getContactedParent() == 0)
				{
					//If this also fails, disconnect child nodes and try again
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unable to connect to any other node. Disconnecting children...");

					this->disconnectAllChildNodes(gs);

					gs->resetBlacklist();

					this->contactCheapestNeighbor(gs);

					if (gs->getContactedParent() == 0)
					{
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unable to connect to any node. Waiting for new neighbor discovery frames...");
						gs->setRejectionCounter((gs->getNNeighbors() * 2) + 1);
					}
				}

				//Reset doIncrementAfterConfirm
				gs->setDoIncrAfterConfirm(false);

				//Reset unchanged counter
				gs->resetUnchangedCounter();

				return false;
			}
		}
		gs->setEmptyPathOnConnect(false);
		return true;
	}

	void EEBTProtocolSrcPath::checkParentPathStatus(Ptr<GameState> gs)
	{
		//If we are contacting another parent, do not request information
		if (gs->getContactedParent() != 0)
			return;

		Ptr<EEBTPNode> parent = gs->getParent();
		if (parent != 0)
		{
			if (gs->getParentUnchangedCounter() <= 5)
			{
				//If we did not get any information from our parent in the last X milliseconds, request the path to source
				if (Now() > (gs->getLastParentUpdate() + MilliSeconds(this->timeToWait)))
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Parent path information are too old. Requesting information... (" << gs->getParentUnchangedCounter() << " / 10)");
					gs->getPPCEvent()->Cancel();
					this->Send(gs, CYCLE_CHECK, parent->getAddress(), parent->getReachPower());
				}
				else if (gs->getPPCEvent() != 0)
					Simulator::Schedule((Now() + MilliSeconds(this->timeToWait)), gs->getPPCEvent());
			}
		}
	}
}
