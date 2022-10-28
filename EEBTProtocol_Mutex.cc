/*
 * SimpleBroadcastProtocol.cc
 *
 *  Created on: 02.06.2020
 *      Author: krassus
 */

#include <math.h>

#include "ns3/EEBTPTag.h"
#include "SendEvent.h"
#include "SeqNoCache.h"
#include "Mutex_SendEvent.h"
#include "EEBTProtocol_Mutex.h"
#include "EEBTPQueueDiscItem.h"

#include "float.h"
#include "ns3/nstime.h"
#include "ns3/mac-low.h"
#include "ns3/log.h"
#include "ns3/integer.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/mobility-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/threshold-preamble-detection-model.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("EEBTProtocolMutex");
	NS_OBJECT_ENSURE_REGISTERED(EEBTProtocolMutex);

	EEBTProtocolMutex::EEBTProtocolMutex() : EEBTProtocol()
	{
	}

	EEBTProtocolMutex::~EEBTProtocolMutex()
	{
	}

	TypeId EEBTProtocolMutex::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::EEBTProtocolMutex").SetParent<EEBTProtocol>().AddConstructor<EEBTProtocolMutex>();
		return tid;
	}

	TypeId EEBTProtocolMutex::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	/*
	 * Install method to install this protocol on the stack of a node
	 */
	void EEBTProtocolMutex::Install(Ptr<WifiNetDevice> netDevice, Ptr<CycleWatchDog> cwd)
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
		this->device->GetMac()->GetWifiRemoteStationManager()->TraceConnectWithoutContext("MacTxFinalRtsFailed", MakeCallback(&EEBTPPacketManager::onTxFinalRtsFailed, this->packetManager));
		this->device->GetMac()->GetWifiRemoteStationManager()->TraceConnectWithoutContext("MacTxFinalDataFailed", MakeCallback(&EEBTPPacketManager::onTxFinalDataFailed, this->packetManager));

		this->ndInterval = this->device->GetMac()->GetSlot().GetMicroSeconds() * 2000;

		TrafficControlHelper tch = TrafficControlHelper();
		tch.Install(this->device);

		this->tcl = this->device->GetNode()->GetObject<TrafficControlLayer>();
		this->device->GetNode()->RegisterProtocolHandler(MakeCallback(&TrafficControlLayer::Receive, this->tcl), EEBTProtocol::PROT_NUMBER, this->device);
		this->tcl->RegisterProtocolHandler(MakeCallback(&EEBTProtocolMutex::Receive, this), EEBTProtocol::PROT_NUMBER, this->device);

		//this->device->GetNode()->RegisterProtocolHandler(MakeCallback(&EEBTProtocolMutex::Receive, this), EEBTProtocol::PROT_NUMBER, this->device);
	}

	void EEBTProtocolMutex::Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType)
	{
		EEBTPTag tag;
		EEBTPHeader header;
		Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(device);
		Mac48Address sender_addr = Mac48Address::ConvertFrom(sender);
		//Mac48Address receiver_addr = Mac48Address::ConvertFrom(receiver);

		//Get packet header
		packet->PeekHeader(header);

		//Get packet tag
		tag = this->packetManager->getPacketTag(header.GetSequenceNumber());

		NS_LOG_DEBUG("[Node " << dev->GetNode()->GetId() << " / " << Now() << "]: [" << this->myAddress << "] received packet from " << sender_addr << " / SeqNo: " << header.GetSequenceNumber() << " / FRAME_TYPE: " << (int)header.GetFrameType() << " / txPower: " << header.GetTxPower() << "dBm / noise: " << tag.getNoise() << "dBm");
		NS_LOG_DEBUG("\thTx: " << header.GetHighestMaxTxPower() << ", shTx: " << header.GetSecondHighestMaxTxPower());

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

		//Only for MUTEX
		if (header.getNeededLockUpdate())
		{
			gs->setNewParentWaitingForLock(true);
			gs->setNewParentWaitingLockOriginator(Create<EEBTPNode>(header.GetOriginator(), this->maxAllowedTxPower));
		}
		else
		{
			gs->setNewParentWaitingForLock(false);
			gs->setNewParentWaitingLockOriginator(0);
		}

		//Check if node is in our neighbor list
		if (!gs->isNeighbor(sender_addr))
			gs->addNeighbor(sender_addr);

		//Get the EEBTPNode
		Ptr<EEBTPNode> node = gs->getNeighbor(sender_addr);

		if (header.GetFrameType() != APPLICATION_DATA)
		{
			node->setParentAddress(header.GetParent());
			node->setReachPower(this->calculateTxPower(tag.getSignal(), header.GetTxPower(), tag.getNoise(), tag.getMinSNR()));
			node->updateRxInfo(tag.getSignal(), tag.getNoise());
			node->setHighestMaxTxPower(header.GetHighestMaxTxPower());
			node->setSecondHighestMaxTxPower(header.GetSecondHighestMaxTxPower());
			node->setFinished(header.getGameFinishedFlag());
			gs->findHighestTxPowers();

			//If the node has a reachpower that is higher than our maximum allowed txPower
			if (node->getReachPower() > this->maxAllowedTxPower)
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << " / " << Now() << "]: ReachPower for node [" << node->getAddress() << "] is too high: " << node->getReachPower() << "dBm > " << this->maxAllowedTxPower << "dBm; FRAME_TYPE: " << (uint)header.GetFrameType());

				//Mark node with the reach problem flag
				node->hasReachPowerProblem(true);

				//If this node is a child of us
				if (gs->isChild(node) && header.GetFrameType() != PARENT_REVOCATION)
				{
					gs->updateLastFrameType(sender_addr, PARENT_REVOCATION, header.GetSequenceNumber());

					//Handle as parent revocation
					this->handleParentRevocation(gs, node);

					//And reject the connection
					EEBTProtocol::Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
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
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << " / " << Now() << "]: Recipient had receive problems. New frame type is " << (uint)(header.GetFrameType() & 0b01111111));

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
						EEBTProtocol::Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
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
		{
			Ptr<EEBTPNode> originator = gs->getNeighbor(header.GetOriginator());
			Ptr<EEBTPNode> newOriginator = gs->getNeighbor(header.GetNewParent());
			Ptr<EEBTPNode> childLockFinishedOrg = gs->getNeighbor(header.GetOldParent());

			if (originator == 0)
				originator = Create<EEBTPNode>(header.GetOriginator(), this->maxAllowedTxPower);
			if (newOriginator == 0)
				newOriginator = Create<EEBTPNode>(header.GetNewParent(), this->maxAllowedTxPower);
			if (childLockFinishedOrg == 0)
				childLockFinishedOrg = Create<EEBTPNode>(header.GetOldParent(), this->maxAllowedTxPower);

			this->handleCycleCheck(gs, node, originator, newOriginator, childLockFinishedOrg);
			break;
		}
		case NEIGHBOR_DISCOVERY:
			this->handleNeighborDiscovery(gs, node);
			break;
		case CHILD_REQUEST:
			this->handleChildRequest(gs, node);
			break;
		case CHILD_CONFIRMATION:
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

		node->resetReachPowerChanged();

		//Print new line to separate events
		NS_LOG_DEBUG("\n");
	}

	//Send method for locking child nodes in subtree
	void EEBTProtocolMutex::Send(Ptr<GameState> gs, Ptr<EEBTPNode> receiver, Mac48Address originator, Mac48Address newOriginator, Mac48Address childLockFinishedOrg)
	{
		EEBTPHeader header = EEBTPHeader();
		header.SetFrameType(CYCLE_CHECK);

		header.SetOriginator(originator);
		header.SetNewParent(newOriginator);
		header.SetOldParent(childLockFinishedOrg);

		this->Send(gs, header, receiver->getAddress(), receiver->getReachPower(), false);
	}

	//Send method for CCSendevent to check if a packet needs retransmission or not
	void EEBTProtocolMutex::Send(Ptr<GameState> gs, Ptr<EEBTPNode> receiver, Mac48Address originator, Mac48Address newOriginator, Mac48Address childLockFinishedOrg, uint16_t seqNo, double txPower, Ptr<MutexSendEvent> event)
	{
		if (this->packetManager->isPacketAcked(seqNo))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has been acked. No retransmission, time = " << Now());
			this->packetManager->deleteSeqNoEntry(seqNo);
			return;
		}
		else if (this->packetManager->isPacketLost(seqNo) || event->getNTimes() > 20)
		{
			if (gs->isChild(receiver) || (gs->getParent() != 0 && receiver->getAddress() == gs->getParent()->getAddress()))
			{
				if (event->getNTimes() > 20)
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has not been acked yet. Retransmitting..., time = " << Now());
				else
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has been lost. Retransmitting..., time = " << Now());

				EEBTPHeader header = EEBTPHeader();
				header.SetFrameType(CYCLE_CHECK);

				header.SetOriginator(originator);
				header.SetNewParent(newOriginator);
				header.SetOldParent(childLockFinishedOrg);
				header.SetSequenceNumber(seqNo);

				this->Send(gs, header, receiver->getAddress(), receiver->getReachPower() + 1, true);
			}
			else
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has been lost but [" << receiver->getAddress() << "] is not a child of us anymore nor our parent, time = " << Now());
			this->packetManager->deleteSeqNoEntry(seqNo);
		}
		else
		{
			Time ttw = this->device->GetMac()->GetAckTimeout() * 200;
			Simulator::Schedule(ttw, event);
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Rescheduled MutexSendEvent(" << seqNo << ") for " << (Now() + ttw) << ", now = " << Now());
		}
	}

	void EEBTProtocolMutex::Send(Ptr<GameState> gs, EEBTPHeader header, Mac48Address recipient, double txPower, bool isRetransmission)
	{
		//If we send out a child confirmation...
		if (header.GetFrameType() == CHILD_CONFIRMATION && gs->isLocked())
		{
			//...set the 'needed lock update' flag is we are locked
			header.setNeededLockUpdate(gs->isLocked());
			header.SetOriginator(gs->getLockedBy());
		}

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

		if (header.GetFrameType() == NEIGHBOR_DISCOVERY) //Check the neighbor discovery event handler (SendEvent)
		{
			if (!this->checkNeigborDiscoverySendEvent(gs) || gs->isLocked())
				return;
		}

		if (header.GetFrameType() == CYCLE_CHECK)
		{
			Time ttw = this->device->GetMac()->GetAckTimeout() * 100;
			Simulator::Schedule(ttw, Create<MutexSendEvent>(gs, this, gs->getNeighbor(recipient), header.GetOriginator(), header.GetNewParent(), header.GetOldParent(), txPower, header.GetSequenceNumber()));
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Scheduled MutexSendEvent(" << header.GetSequenceNumber() << ") for " << (Now() + ttw));
		}
		else if (header.GetFrameType() == CHILD_REQUEST || header.GetFrameType() == CHILD_CONFIRMATION || header.GetFrameType() == CHILD_REJECTION ||
				 header.GetFrameType() == PARENT_REVOCATION || header.GetFrameType() == END_OF_GAME)
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

		if (gs->getParent() == 0)
			header.SetParent(Mac48Address::GetBroadcast());
		else
			header.SetParent(gs->getParent()->getAddress());

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

		//this->device->Send(packet, recipient, EEBTProtocol::PROT_NUMBER);
		this->packetManager->sendPacket(packet, recipient);

		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: EEBTProtocolMutex::Send(): " << this->myAddress << " => " << recipient << " / SeqNo: " << header.GetSequenceNumber() << " / FRAME_TYPE: " << (uint)header.GetFrameType() << " / txPower: " << header.GetTxPower() << "/" << this->wifiPhy->GetTxPowerStart() << "|" << this->wifiPhy->GetTxPowerEnd() << "dBm");
		NS_LOG_DEBUG("\thTx: " << header.GetHighestMaxTxPower() << ", shTx: " << header.GetSecondHighestMaxTxPower() << ", rounds: " << gs->getUnchangedCounter() << "/" << ((gs->getNNeighbors() * 0.5) + 2) << ", time = " << Now());
	}

	/*
	 * Handle the cycle check (FrameType 0)
	 */
	void EEBTProtocolMutex::handleCycleCheck(Ptr<GameState> gs, Ptr<EEBTPNode> node, Ptr<EEBTPNode> originator, Ptr<EEBTPNode> newOriginator, Ptr<EEBTPNode> childLockFinishedOrg)
	{
		NS_ASSERT_MSG(originator != 0, "[Node " << this->device->GetNode()->GetId() << "]: originator is null");

		//!Assertion
		if (gs->isInitiator())
		{
			//NS_LOG_DEBUG("Ignoring mutex from " << node->getAddress() << " because I am the initiator");
			NS_LOG_ERROR("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from [" << node->getAddress() << "] but I am the initiator and therefore cannot be a child of someone :O");
		}
		else if (gs->getParent() == node)
		{
			//If the mutex comes from our parent, we can get locked, an updated lock or unlocked.
			//If we are currently locked by ourself, we probably want to switch our parent
			if (gs->isLocked() && gs->getLockedBy() == this->myAddress)
			{
				//If the game already finished, we note the lock request and look after it when we got rejected
				if (gs->gameFinished())
				{
					//Node that our parent requested a mutex and carry on
					gs->setParentWaitingForLock(true);
					gs->setParentWaitingLockOriginator(originator);
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Note lock from parent [" << gs->getParent()->getAddress() << "] but I want to change my parent");
				}
				else
				{
					if (gs->getParent() == 0)
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Ignoring lock from parent [" << node->getAddress() << "] because I want to switch parents");
					else
						NS_LOG_ERROR("[Node " << this->device->GetNode()->GetId() << "]: The game is not finished and we have a parent [" << gs->getParent()->getAddress() << "] and are locked by ourself?!");
				}
				return;
			}

			//If the originator contains not the broadcast address, we received a new lock
			if (originator->getAddress() != Mac48Address::GetBroadcast())
			{
				if (originator->getAddress() == this->myAddress)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from our parent [" << gs->getParent()->getAddress() << "] with me as originator [" << originator->getAddress() << "]?! Ignoring lock...");
				}
				else
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from our parent [" << gs->getParent()->getAddress() << "] with new originator [" << originator->getAddress() << "]. Locking subtree...");

					//Set lock holder
					gs->setLockedBy(originator->getAddress());

					//...lock the subtree and respond if we have no child nodes
					this->lockChildNodes(gs);
					this->checkNodeLocks(gs);
				}
			}
			else if (newOriginator->getAddress() != Mac48Address::GetBroadcast())
			{
				if (newOriginator->getAddress() == this->myAddress)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from our parent [" << gs->getParent()->getAddress() << "] with me as new originator [" << originator->getAddress() << "]?! Ignoring lock...");
				}
				else
				{
					//Update our lock
					gs->setLockedBy(newOriginator->getAddress());

					//Update subtree
					this->lockChildNodes(gs);
					this->checkNodeLocks(gs);
				}
			}
			else
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from our parent [" << gs->getParent()->getAddress() << "] with no new originator. Unlocking subtree...");

				//Unlock the subtree
				this->unlockChildNodes(gs);

				//Broadcast neighbor discovery frames again
				EEBTProtocol::Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);
			}
		}
		else if (gs->getContactedParent() == node)
		{
			//If we received a mutex from the parent we want to connect to means that the child confirmation frame is late
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from parent we want to connect to [" << node->getAddress() << "]");
			gs->setNewParentWaitingForLock(true);
			gs->setNewParentWaitingLockOriginator(originator);
		}
		else if (gs->isChild(node))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from one of our child nodes [" << node->getAddress() << "] with lock holder [" << childLockFinishedOrg->getAddress() << "]");

			//If we get a mutex from a child of ours, check if the childLockFinishedOrg is correct
			if (childLockFinishedOrg->getAddress() == gs->getLockedBy())
			{
				//Add node to locked child node list
				gs->lock(node, true);

				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Waiting for:");
				for (int i = 0; i < gs->getNChilds(); i++)
				{
					if (!gs->isLocked(gs->getChild(i)))
						NS_LOG_DEBUG("\t" << gs->getChild(i)->getAddress());
				}

				//Check status of locks
				this->checkNodeLocks(gs);
			}
			else
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Ignoring lock with old originator because I am locked by [" << gs->getLockedBy() << "]");
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Received mutex from unknown [" << node->getAddress() << "]");
		}
	}

	/*
	 * Handle neighbor discovery (FrameType 1)
	 */
	void EEBTProtocolMutex::handleNeighborDiscovery(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: EEBTProtocolMutex::handleNeighborDiscovery()");
		if (!gs->isLocked())
		{
			if (gs->isInitiator())
				gs->resetUnchangedCounter();

			if (node->getReachPower() <= this->maxAllowedTxPower && !gs->isChild(node))
				gs->resetRejectionCounter();

			EEBTProtocol::handleNeighborDiscovery(gs, node);
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Ignoring neighbor discovery because I am locked by [" << gs->getLockedBy() << "]");
		}
	}

	/*
	 * Handle child request (FrameType 2)
	 * Sent by child, received by parent
	 * if a child wants to connect to the parent node
	 */
	void EEBTProtocolMutex::handleChildRequest(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		if (gs->checkLastFrameType(node->getAddress(), CHILD_REQUEST, gs->getLastSeqNo(node->getAddress(), PARENT_REVOCATION)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child request from [" << node->getAddress() << "] dismissed because we received a PARENT_REVOCATION with a higher sequence number earlier.");
			return;
		}

		//If we are not the initiator...
		if (!gs->isInitiator() && !gs->isChild(node))
		{
			//...and currently locked...
			if (gs->isLocked())
			{
				//...and the request comes from the lock holder, reject this request (=> Cycle detected)
				if (gs->getLockedBy() == node->getAddress())
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Reject child request from [" << node->getAddress() << "] because I am somehow connected to it");
					return EEBTProtocol::Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
				}
				else if (node == gs->getContactedParent()) //...and the node we want to connect to contacts us, reject it since this could end up in a deadlock
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Reject child request from [" << node->getAddress() << "] because this situation is a deadlock");
					return EEBTProtocol::Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
				}
				else if (gs->getNChildsLocked() >= gs->getNChilds() && !gs->isInitiator()) //...and we already sent a child request (all nodes are locked), reject it since this could also end up in a deadlock
				{
					if (gs->getLockedBy() == this->myAddress)
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Reject child request from [" << node->getAddress() << "] because I already sent a child request to [" << gs->getContactedParent()->getAddress() << "]");
					else
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Reject child request from [" << node->getAddress() << "] because I am locked by [" << gs->getLockedBy() << "]");
					return EEBTProtocol::Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
				}

				//Else we are locked but still waiting for all responses
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Processing child request from [" << node->getAddress() << "]");
			}
			else if (gs->getParent() == 0 && gs->getContactedParent() == 0) //...and we are not connected to the source node (should not happen)
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Reject child request from '" << node->getAddress() << "' because I am not connected to the source node");
				EEBTProtocol::Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());

				if (gs->getRejectionCounter() > (gs->getNNeighbors() * 2) && gs->getParent() == 0)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: I am still waiting for neighbor discovery frames since I failed " << gs->getRejectionCounter() << " times in a row to connect to a parent.");
					return;
				}
				return this->contactCheapestNeighbor(gs);
			}
		}

		EEBTProtocol::handleChildRequest(gs, node);
	}

	/*
	 * Handle child confirmation (FrameType 3)
	 * Sent by parent, received by child
	 * if parent accepts the child request
	 */
	void EEBTProtocolMutex::handleChildConfirmation(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
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
					return EEBTProtocol::Send(gs, PARENT_REVOCATION, node->getAddress(), node->getReachPower());
			}
			else if (gs->gameFinished()) //...and the game is already finished, disconnect from old parent
			{
				this->disconnectOldParent(gs);
			}
		}
		else if (node != gs->getContactedParent())
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child confirmation from [" << node->getAddress() << "] dismissed because we did not ask him to be our parent");
			return EEBTProtocol::Send(gs, PARENT_REVOCATION, node->getAddress(), node->getReachPower());
		}

		//Set new parent
		gs->setParent(node);
		node->incrementConnCounter();
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Connected to [" << node->getAddress() << "]");
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: rP(" << node->getAddress() << ") = " << node->getReachPower() << ", hTx(" << node->getAddress() << ") = " << node->getHighestMaxTxPower() << ", shTx(" << node->getAddress() << ") = " << node->getSecondHighestMaxTxPower());
		for (uint i = 0; i < gs->getNNeighbors(); i++)
		{
			Ptr<EEBTPNode> neighbor = gs->getNeighbor(i);
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]:\trP(" << neighbor->getAddress() << ") = " << neighbor->getReachPower() << ", hTx(" << neighbor->getAddress() << ") = " << neighbor->getHighestMaxTxPower() << ", shTx(" << neighbor->getAddress() << ") = " << neighbor->getSecondHighestMaxTxPower());
		}

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
			if (!gs->isNewParentWaitingForLock())
				this->contactCheapestNeighbor(gs);
		}

		//If we did not contact another node...
		if (gs->getContactedParent() == 0)
		{
			node->resetConnCounter();
			gs->resetRejectionCounter();

			if (node->hasFinished())
				this->handleEndOfGame(gs, node);

			//If we finished our game earlier, send END_OF_GAME to our parent
			if (gs->gameFinished())
				EEBTProtocol::Send(gs, END_OF_GAME, gs->getParent()->getAddress(), gs->getParent()->getReachPower());

			if (gs->isNewParentWaitingForLock())
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: New parent is waiting for his lock. Locked by [" << gs->getNewParentWaitingLockOriginator()->getAddress() << "]");
				gs->setLockedBy(Mac48Address::GetBroadcast());
				this->handleCycleCheck(gs, gs->getParent(), gs->getNewParentWaitingLockOriginator(), Create<EEBTPNode>(Mac48Address::GetBroadcast(), 23.0), Create<EEBTPNode>(Mac48Address::GetBroadcast(), 23.0));

				gs->setNewParentWaitingForLock(false);
				gs->setNewParentWaitingLockOriginator(0);
			}
			else
			{
				//...unlock child nodes and check waiting list
				this->unlockChildNodes(gs);

				//Reset the unchanged counter, since our topology changed
				if (!gs->doIncrAfterConfirm() && gs->getUnchangedCounter() < EEBTProtocol::MAX_UNCHANGED_ROUNDS)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Resetting unchanged counter");
					gs->resetUnchangedCounter();
				}

				gs->setDoIncrAfterConfirm(false);

				//Inform our neighbors
				EEBTProtocol::Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);
			}
		}
		else
		{
			if (!(gs->gameFinished() && gs->getParent() != 0))
			{
				gs->setNewParentWaitingForLock(false);
				gs->setNewParentWaitingLockOriginator(0);
			}
			else
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Not resetting waiting status for parent lock");
		}
	}

	/*
	 * Handle child rejection (FrameType 4)
	 * Sent by parent, received by child
	 * if a parent rejects a child request
	 */
	void EEBTProtocolMutex::handleChildRejection(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		if (gs->checkLastFrameType(node->getAddress(), CHILD_REJECTION, gs->getLastSeqNo(node->getAddress(), CHILD_CONFIRMATION)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child rejection from [" << node->getAddress() << "] dismissed because we received a CHILD_CONFIRMATION with a higher sequence number earlier.");
			return;
		}

		//Check if node is our parent
		if (node != gs->getParent() && node != gs->getContactedParent())
		{
			NS_LOG_DEBUG("Ignoring child rejection from [" << node->getAddress() << "] since it is not our parent!");
			return;
		}

		//If our parent sends us a CHILD_REVOCATION...
		if (node == gs->getParent())
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: My parent [" << node->getAddress() << "] disconnected me");

			//Check our last cycle and set the finish time
			std::vector<Ptr<CycleInfo>> cycles = this->cycleWatchDog->getCycles(gs->getGameID(), this->device->GetNode()->GetId());
			if (cycles.size() > 0)
			{
				Ptr<CycleInfo> ci = *(cycles.end() - 1);
				if (ci->getEndTime().GetNanoSeconds() == 0)
					ci->setEndTime(Now());
			}

			gs->setParent(0);

			//If we are locked (not by ourself), we need to update the lock since we must connect to a new parent
			if (gs->isLocked() && gs->getLockedBy() != this->myAddress)
			{
				//Set our lock temporarily to false
				gs->lock(false);

				//Reset lock list
				gs->resetChildLocks();

				gs->setContactedParent(0);
			}
		}
		else
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Failed to connect to [" << node->getAddress() << "]; noise = " << node->getNoise());
			node->incrementConnCounter();
		}

		gs->resetNeighborDiscoveryEvent();

		//Add contacted parent to blacklist (only if is was not a reach power problem)
		if (!node->hasReachPowerProblem())
			gs->updateBlacklist(node);

		//Reset the contacted parent
		gs->setContactedParent(0);

		if (gs->getRejectionCounter() > (gs->getNNeighbors() * 2) && gs->getParent() == 0)
		{
			gs->lock(false);
			gs->resetBlacklist();
			gs->resetChildLocks();
			this->disconnectAllChildNodes(gs);
			gs->setLockedBy(Mac48Address::GetBroadcast());

			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Failed " << (gs->getNNeighbors() * 2) << " times to connect to any node. Waiting for new neighbor discovery frames");
			return;
		}

		//Contact cheapest neighbor
		this->contactCheapestNeighbor(gs);

		if (gs->getContactedParent() == 0)
		{
			node->resetConnCounter();

			//If we are still connected to our parent while the game is finished...
			if (gs->gameFinished() && gs->getParent() != 0)
			{
				//And our parent is waiting for his lock, update subtree
				if (gs->isParentWaitingForLock())
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Parent is waiting for his lock");
					gs->setLockedBy(Mac48Address::GetBroadcast());
					this->handleCycleCheck(gs, gs->getParent(), Create<EEBTPNode>(Mac48Address::GetBroadcast(), 23.0), gs->getParentWaitingLockOriginator(), Create<EEBTPNode>(Mac48Address::GetBroadcast(), 23.0));
				}
				else
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Parent is not waiting for a lock.");
					this->unlockChildNodes(gs);
				}
			}
			else
			{
				//If the game is already finished and we did not disconnect our old parent
				if (gs->gameFinished() && gs->getParent() != 0)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: We already finished our game and did not disconnect from our old parent.");
					return;
				}
				else
				{
					//If this also fails, disconnect child nodes and try again
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unable to connect to any other node. Disconnecting children...");

					this->disconnectAllChildNodes(gs);

					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Searching for cheapest neighbor...");
					this->contactCheapestNeighbor(gs);

					if (gs->getContactedParent() == 0)
					{
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Could not find any possible parent.");
						gs->lock(false);
						gs->resetChildLocks();
						gs->setLockedBy(Mac48Address::GetBroadcast());
					}
				}
			}
		}

		//Reset doIncrementAfterConfirm
		gs->setDoIncrAfterConfirm(false);

		//Reset unchanged counter
		gs->resetUnchangedCounter();

		gs->incrementRejectionCounter();
	}

	/*
	 * Handle parent revocation (FrameType 5)
	 * Sent by child, received by parent
	 * if child wants to disconnect from a parent
	 */
	void EEBTProtocolMutex::handleParentRevocation(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//NS_LOG_DEBUG(this->device->GetNode()->GetId() << " => EEBTProtocolMutex::handleParentRevocation()");

		if (gs->checkLastFrameType(node->getAddress(), PARENT_REVOCATION, gs->getLastSeqNo(node->getAddress(), CHILD_REQUEST)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Parent revocation from [" << node->getAddress() << "] dismissed because we received a CHILD_REQUEST with a higher sequence number earlier.");
			return;
		}

		//Assertion
		if (gs->isChild(node->getAddress()))
		{
			//Remove node from lock list
			gs->lock(node, false);

			//Remove child from child list
			gs->removeChild(node);

			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child [" << node->getAddress() << "] disconnected");
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: My current child nodes:");
			for (int i = 0; i < gs->getNChilds(); i++)
				NS_LOG_DEBUG("\t\t[" << gs->getChild(i)->getAddress() << "]: rP = " << gs->getChild(i)->getReachPower());
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: hTx = " << gs->getHighestTxPower() << ", shTx = " << gs->getSecondHighestTxPower());

			if (!gs->gameFinished())
			{
				gs->resetUnchangedCounter();
				gs->resetNeighborDiscoveryEvent();

				if ((!gs->isLocked() && gs->getParent() != 0) || gs->isInitiator())
					EEBTProtocol::Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);
			}

			this->checkNodeLocks(gs);
		}
		else
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Node [" << node->getAddress() << "] is not a child of mine");
	}

	/*
	 * Helper methods
	 */

	/*
	 * This method handles the CHILD_REQUEST to new parents
	 */
	void EEBTProtocolMutex::contactNode(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//If we are locked...
		if (gs->isLocked())
		{
			//...and are locked by ourself, contact node
			if (gs->getLockedBy() == this->myAddress)
			{
				if (node == gs->getParent())
				{
					if (gs->isParentWaitingForLock())
					{
						gs->setLockedBy(gs->getParentWaitingLockOriginator()->getAddress());
						this->lockChildNodes(gs);
						this->checkNodeLocks(gs);
					}
					else
						this->unlockChildNodes(gs);
				}
				else
				{
					if (gs->gameFinished() && gs->getNChildsLocked() < gs->getNChilds())
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Tried connecting to [" << node->getAddress() << "] but still waiting for locks! Game finished: " << (gs->gameFinished() ? "true" : "false"));
					else
					{
						gs->setContactedParent(0);
						EEBTProtocol::contactNode(gs, node);
					}
				}
			}
			else
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: I am locked by someone else: " << gs->getLockedBy());
				return;
			}
		}
		else
		{
			gs->setContactedParent(node);
			gs->resetNeighborDiscoveryEvent();

			if (!gs->gameFinished())
				this->disconnectOldParent(gs);

			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Locking subtree before connecting to [" << node->getAddress() << "]");

			//Lock our subtree
			gs->setLockedBy(this->myAddress);
			this->lockChildNodes(gs);
			this->checkNodeLocks(gs);
		}
	}

	/*
	 * Mutex helper
	 */
	bool EEBTProtocolMutex::lockChildNodes(Ptr<GameState> gs)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Locking child nodes:");

		bool wasLockedBefore = gs->isLocked();
		gs->lock(true);

		gs->resetChildLocks();
		gs->resetUnchangedCounter();
		gs->resetNeighborDiscoveryEvent();

		if (gs->hasChilds())
		{
			for (int i = 0; i < gs->getNChilds(); i++)
			{
				NS_LOG_DEBUG("\t\tSend mutex to [" << gs->getChild(i)->getAddress() << "]");
				if (wasLockedBefore)
					this->Send(gs, gs->getChild(i), Mac48Address::GetBroadcast(), gs->getLockedBy(), Mac48Address::GetBroadcast());
				else
					this->Send(gs, gs->getChild(i), gs->getLockedBy(), Mac48Address::GetBroadcast(), Mac48Address::GetBroadcast());
			}
			return false;
		}
		return true;
	}

	void EEBTProtocolMutex::unlockChildNodes(Ptr<GameState> gs)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unlocking child nodes:");

		//Unlock ourself
		gs->lock(false);

		if (gs->hasChilds())
		{
			for (int i = 0; i < gs->getNChilds(); i++)
			{
				NS_LOG_DEBUG("\t\tSend mutex to [" << gs->getChild(i)->getAddress() << "]");
				gs->lock(gs->getChild(i), false);
				this->Send(gs, gs->getChild(i), Mac48Address::GetBroadcast(), Mac48Address::GetBroadcast(), Mac48Address::GetBroadcast());
			}
			gs->resetChildLocks();
		}

		//Reset locked by address
		gs->setLockedBy(Mac48Address::GetBroadcast());
	}

	void EEBTProtocolMutex::disconnectOldParent(Ptr<GameState> gs)
	{
		gs->setParentWaitingForLock(false);
		gs->setParentWaitingLockOriginator(0);

		EEBTProtocol::disconnectOldParent(gs);
	}

	/*
	 * Check the lock status
	 * This method checks whether all child nodes have been locked or not
	 * and performs the required action if all nodes have been locked
	 */
	void EEBTProtocolMutex::checkNodeLocks(Ptr<GameState> gs)
	{
		//If all child nodes are locked...
		if (gs->isLocked() && gs->getNChildsLocked() >= gs->getNChilds() && !gs->isInitiator())
		{
			//If we are locked by ourself
			if (gs->getLockedBy() == this->myAddress)
			{
				if (gs->getContactedParent() != 0)
				{
					//Check if there is a possible better parent then the one we want to connect to
					Ptr<EEBTPNode> cheapestNeighbor = gs->getCheapestNeighbor();

					if (cheapestNeighbor == 0)
					{
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: All nodes are out of range! Disconnecting child nodes...");
						gs->setContactedParent(0);
						gs->lock(false);
						gs->resetChildLocks();
						gs->setLockedBy(Mac48Address::GetBroadcast());
						this->disconnectAllChildNodes(gs);
					}
					else
					{
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Subtree is now locked. Connecting to new parent ["
											  << gs->getContactedParent()->getAddress() << "]/[" << cheapestNeighbor->getAddress() << "]");
						this->contactNode(gs, cheapestNeighbor);
					}
				}
				else
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: We locked ourself but did not note the parent we want to connect to?!");
					this->unlockChildNodes(gs);
				}
			}
			else
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Subtree is now locked. Contacting parent node [" << gs->getParent()->getAddress() << "]");

				//Inform parent, that our subtree is now locked
				this->Send(gs, gs->getParent(), Mac48Address::GetBroadcast(), Mac48Address::GetBroadcast(), gs->getLockedBy());
			}
		}
	}
}
