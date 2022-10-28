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
#include "AD_SendEvent.h"
#include "CC_SendEvent.h"
#include "EEBTProtocol.h"
#include "EEBTPDataHeader.h"
#include "EEBTPQueueDiscItem.h"
#include "CustomWifiTxCurrentModel.h"

#include "float.h"
#include "ns3/nstime.h"
#include "ns3/mac-low.h"
#include "ns3/log.h"
#include "ns3/wifi-utils.h"
#include "ns3/integer.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-phy.h"
#include "ns3/llc-snap-header.h"
#include "ns3/mobility-module.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/traffic-control-helper.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("EEBTProtocol");
	NS_OBJECT_ENSURE_REGISTERED(EEBTProtocol);

	const uint16_t EEBTProtocol::PROT_NUMBER = 153;
	const uint32_t EEBTProtocol::MAX_UNCHANGED_ROUNDS = 10;

	EEBTProtocol::EEBTProtocol()
	{
		this->sendCounter = 0;
		this->maxPackets = 1000;
		this->dataLength = 1000;

		this->ndInterval = 0;
		this->cache = SeqNoCache();
		this->maxAllowedTxPower = 23;
	}

	EEBTProtocol::~EEBTProtocol()
	{
		this->cache.~SeqNoCache();
		this->cycleWatchDog->~CycleWatchDog();
		this->cycleWatchDog = 0;
		this->games.clear();
		this->packetManager->~EEBTPPacketManager();
		this->packetManager = 0;
	}

	TypeId EEBTProtocol::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::EEBTPProtocol").SetParent<Object>().AddConstructor<EEBTProtocol>();
		return tid;
	}

	TypeId EEBTProtocol::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	std::ostream &operator<<(std::ostream &os, EEBTProtocol &prot)
	{
		prot.Print(os);
		return os;
	}

	void EEBTProtocol::Print(std::ostream &os) const
	{
		Ptr<GameState> gs;
		uint64_t gid = 0;
		bool found = false;

		for (uint i = 0; i < this->games.size(); i++)
		{
			gs = this->games[i];
			if (gs->getGameID() == gid)
			{
				found = true;
				break;
			}
		}

		os << "<===================== Node " << this->device->GetNode()->GetId() << " =====================>\n";

		if (!found)
		{
			os << "NO INFORMATION AVAILABLE\n";
			return;
		}
		else
			gs->findHighestTxPowers();

		Vector pos = this->device->GetNode()->GetObject<MobilityModel>()->GetPosition();
		os << "MAC ADDR:\t\t" << this->myAddress << "\n";
		os << "POSITION:\t\tX " << pos.x << ", Y " << pos.y << "\n";
		os << "TX POWER:\t\t" << gs->getHighestTxPower() << "dBm\n";
		os << "FINISH TIME:\t" << gs->getTimeFinished() << "\n";

		Ptr<EEBTPNode> parent = gs->getParent();
		if (parent != 0)
			os << "PARENT:\t\t\t" << parent->getAddress() << "\n";
		else
			os << "PARENT:\t\t\tff:ff:ff:ff:ff:ff\n";
		os << "\n";

		os << "FINISHED:\t\t" << (gs->gameFinished() ? "TRUE" : "FALSE") << "\n";
		os << "WAITING FOR:\t";
		if (gs->allChildsFinished())
			os << "NONE";
		else
		{
			for (int i = 0; i < gs->getNChilds(); i++)
			{
				os << "\n";

				Ptr<EEBTPNode> child = gs->getChild(i);
				if (!child->hasFinished())
					os << "\t" << child->getAddress();
			}
		}
		os << "\n\n";

		os << "LOCKED:\t\t" << (gs->isLocked() ? "TRUE" : "FALSE") << "\n";
		os << "LOCKED BY:\t" << (gs->isLocked() ? gs->getLockedBy() : Mac48Address::GetBroadcast()) << "\n";
		os << "\n";

		os << "HIGHEST TX POWER:\t\t\t" << gs->getHighestTxPower() << "\n";
		os << "SECOND HIGHEST TX POWER:\t" << gs->getSecondHighestTxPower() << "\n";
		os << "\n";

		os << "APP DATA PACKETS:\t\t\t" << this->maxPackets << "\n";
		os << "MISSING APP DATA PACKETS:\t" << (this->maxPackets - gs->getApplicationDataHandler()->getPacketCount()) << "\n";
		os << "\n";

		os << "CYCLES DURING CONSTRUCTION PHASE:\n";
		int numCyc = 0;
		std::vector<Ptr<CycleInfo>> cycles = this->cycleWatchDog->getCycles(gid, this->device->GetNode()->GetId());
		for (Ptr<CycleInfo> ci : cycles)
		{
			if (ci->isRealCycle())
			{
				numCyc++;
				os << "\t\tTIME: start = " << ci->getStartTime() << ", end = " << ci->getEndTime() << ", duration = " << ci->getDuration() << " | ";
				ci->Print(os);
				os << "\n";
			}
		}
		os << "TOTAL NUMBER OF CYCLES:\t" << numCyc << "\n";
		os << "\n";

		os << "MY CHILD NODES:\n";
		for (int i = 0; i < gs->getNChilds(); i++)
		{
			Ptr<EEBTPNode> child = gs->getChild(i);
			os << "\tNODE " << child->getAddress() << " => " << child->getReachPower() << "dBm | " << child->getNoise() << "dBm\n";
		}
		os << "\n";

		os << "REACH POWER FOR NODES:\n";
		for (uint i = 0; i < gs->getNNeighbors(); i++)
		{
			Ptr<EEBTPNode> neighbor = gs->getNeighbor(i);
			os << "\tNODE " << neighbor->getAddress() << " => " << neighbor->getReachPower() << "dBm | " << neighbor->getNoise() << "dBm\n";
			os << "\t\thTx  = " << neighbor->getHighestMaxTxPower() << "dBm\n";
			os << "\t\tshTx = " << neighbor->getSecondHighestMaxTxPower() << "dBm\n";
		}
		os << "\n";

		os << "ENERGY BY FRAME TYPE:\tRECV | SENT\n";
		os << "\tCYCLE_CHECK:\t\t" << this->packetManager->getEnergyByRecvFrame(gid, 0) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 0) << "J\n";
		os << "\tNEIGHBOR_DISCOVERY:\t" << this->packetManager->getEnergyByRecvFrame(gid, 1) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 1) << "J\n";
		os << "\tCHILD_REQUEST:\t\t" << this->packetManager->getEnergyByRecvFrame(gid, 2) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 2) << "J\n";
		os << "\tCHILD_CONFIRMATION:\t" << this->packetManager->getEnergyByRecvFrame(gid, 3) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 3) << "J\n";
		os << "\tCHILD_REJECTION:\t" << this->packetManager->getEnergyByRecvFrame(gid, 4) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 4) << "J\n";
		os << "\tPARENT_REVOCATION:\t" << this->packetManager->getEnergyByRecvFrame(gid, 5) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 5) << "J\n";
		os << "\tEND_OF_GAME:\t\t" << this->packetManager->getEnergyByRecvFrame(gid, 6) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 6) << "J\n";
		os << "\tAPPLICATION_DATA:\t" << this->packetManager->getEnergyByRecvFrame(gid, 7) << "J | " << this->packetManager->getEnergyBySentFrame(gid, 7) << "J\n";
		os << "TOTAL ENERGY:\t\t\t" << this->packetManager->getTotalEnergyConsumed(gid) << "J\n\n";

		uint32_t allFrameTypesSent = 0;
		for (int i = 0; i < 8; i++)
			allFrameTypesSent += this->packetManager->getFrameTypeSent(gid, i);
		os << "DATA SENT BY FRAME TYPE:\tCOUNT\t | DATA\n";
		os << "\tCYCLE_CHECK:\t\t\t" << this->packetManager->getFrameTypeSent(gid, 0) << "\t | " << this->packetManager->getDataSentByFrame(gid, 0) << "B\n";
		os << "\tNEIGHBOR_DISCOVERY:\t\t" << this->packetManager->getFrameTypeSent(gid, 1) << "\t | " << this->packetManager->getDataSentByFrame(gid, 1) << "B\n";
		os << "\tCHILD_REQUEST:\t\t\t" << this->packetManager->getFrameTypeSent(gid, 2) << "\t | " << this->packetManager->getDataSentByFrame(gid, 2) << "B\n";
		os << "\tCHILD_CONFIRMATION:\t\t" << this->packetManager->getFrameTypeSent(gid, 3) << "\t | " << this->packetManager->getDataSentByFrame(gid, 3) << "B\n";
		os << "\tCHILD_REJECTION:\t\t" << this->packetManager->getFrameTypeSent(gid, 4) << "\t | " << this->packetManager->getDataSentByFrame(gid, 4) << "B\n";
		os << "\tPARENT_REVOCATION:\t\t" << this->packetManager->getFrameTypeSent(gid, 5) << "\t | " << this->packetManager->getDataSentByFrame(gid, 5) << "B\n";
		os << "\tEND_OF_GAME:\t\t\t" << this->packetManager->getFrameTypeSent(gid, 6) << "\t | " << this->packetManager->getDataSentByFrame(gid, 6) << "B\n";
		os << "\tAPPLICATION_DATA:\t\t" << this->packetManager->getFrameTypeSent(gid, 7) << "\t | " << this->packetManager->getDataSentByFrame(gid, 7) << "B\n";
		os << "DATA SENT TOTAL:\t\t\t" << allFrameTypesSent << "\t | " << this->packetManager->getDataSent(gid) << "B\n\n";

		uint32_t allFrameTypesRecv = 0;
		for (int i = 0; i < 8; i++)
			allFrameTypesRecv += this->packetManager->getFrameTypeRecv(gid, i);
		os << "DATA RECEIVED BY FRAME TYPE:\tCOUNT\t | DATA\n";
		os << "\tCYCLE_CHECK:\t\t\t" << this->packetManager->getFrameTypeRecv(gid, 0) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 0) << "B\n";
		os << "\tNEIGHBOR_DISCOVERY:\t\t" << this->packetManager->getFrameTypeRecv(gid, 1) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 1) << "B\n";
		os << "\tCHILD_REQUEST:\t\t\t" << this->packetManager->getFrameTypeRecv(gid, 2) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 2) << "B\n";
		os << "\tCHILD_CONFIRMATION:\t\t" << this->packetManager->getFrameTypeRecv(gid, 3) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 3) << "B\n";
		os << "\tCHILD_REJECTION:\t\t" << this->packetManager->getFrameTypeRecv(gid, 4) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 4) << "B\n";
		os << "\tPARENT_REVOCATION:\t\t" << this->packetManager->getFrameTypeRecv(gid, 5) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 5) << "B\n";
		os << "\tEND_OF_GAME:\t\t\t" << this->packetManager->getFrameTypeRecv(gid, 6) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 6) << "B\n";
		os << "\tAPPLICATION_DATA:\t\t" << this->packetManager->getFrameTypeRecv(gid, 7) << "\t | " << this->packetManager->getDataRecvByFrame(gid, 7) << "B\n";
		os << "DATA RECEIVED TOTAL:\t\t\t" << allFrameTypesRecv << "\t | " << this->packetManager->getDataRecv(gid) << "B\n";
		os << "<==================================================>\n";
	}

	/*
	 * Statistics Getter
	 */
	double EEBTProtocol::getEnergyByFrameType(uint64_t gid, FRAME_TYPE ft)
	{
		double energy = 0;
		energy += this->packetManager->getEnergyByRecvFrame(gid, ft);
		energy += this->packetManager->getEnergyBySentFrame(gid, ft);
		return energy;
	}

	double EEBTProtocol::getEnergyForConstruction(uint64_t gid)
	{
		double energy = 0;
		for (uint8_t i = 0; i < 7; i++)
		{
			energy += this->packetManager->getEnergyByRecvFrame(gid, i);
			energy += this->packetManager->getEnergyBySentFrame(gid, i);
		}
		return energy;
	}

	double EEBTProtocol::getEnergyForApplicationData(uint64_t gid)
	{
		double energy = 0;
		//energy += this->packetManager->getEnergyByRecvFrame(gid, 7);
		energy += this->packetManager->getEnergyBySentFrame(gid, 7);
		return energy;
	}

	Ptr<CycleWatchDog> EEBTProtocol::getCycleWatchDog()
	{
		return this->cycleWatchDog;
	}

	Ptr<EEBTPPacketManager> EEBTProtocol::getPacketManager()
	{
		return this->packetManager;
	}

	Ptr<NetDevice> EEBTProtocol::GetDevice()
	{
		return this->device;
	}

	/*
	 * This method searches for the GameState with the gameID `gid`
	 * If there is no such GameState, it creates a new one and stores
	 * it in the games list
	 */
	Ptr<GameState> EEBTProtocol::getGameState(uint64_t gid)
	{
		for (uint i = 0; i < this->games.size(); i++)
		{
			Ptr<GameState> gs = this->games[i];
			if (gs->getGameID() == gid)
				return gs;
		}

		Ptr<GameState> gs = Create<GameState>(false, gid);
		gs->setMyAddress(this->myAddress);
		this->games.push_back(gs);
		return this->games[this->games.size() - 1];
	}

	Ptr<GameState> EEBTProtocol::initGameState(uint64_t gid)
	{
		for (uint i = 0; i < this->games.size(); i++)
		{
			Ptr<GameState> gs = this->games[i];
			if (gs->getGameID() == gid)
				return gs;
		}

		Ptr<GameState> gs = Create<GameState>(true, gid);
		gs->setMyAddress(this->myAddress);
		this->games.push_back(gs);
		return this->games[this->games.size() - 1];
	}

	void EEBTProtocol::removeGameState(uint64_t gid)
	{
		uint i = 0;
		for (; i < this->games.size(); i++)
		{
			if (this->games[i]->getGameID() == gid)
				break;
		}

		if (i < this->games.size())
			games.erase(this->games.begin() + i, this->games.begin() + (i + 1));
	}

	/*
	 * Install method to install this protocol on the stack of a node
	 */
	void EEBTProtocol::Install(Ptr<WifiNetDevice> netDevice, Ptr<CycleWatchDog> cwd)
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

		this->wifiPhy->SetTxPowerEnd(this->maxAllowedTxPower);
		this->wifiPhy->SetTxPowerStart(this->maxAllowedTxPower);

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
		this->tcl->RegisterProtocolHandler(MakeCallback(&EEBTProtocol::Receive, this), EEBTProtocol::PROT_NUMBER, this->device);
		//this->device->GetNode()->RegisterProtocolHandler(MakeCallback(&EEBTProtocol::Receive, this), EEBTProtocol::PROT_NUMBER, this->device);
	}

	/*
	 * TxPower calculation
	 * Calculates the required transmission power to reach a given node (reachPower)
	 * rxPower (dBm)
	 * txPower (dBm)
	 * noise (dBm)
	 * minSNR (dB)
	 */
	double EEBTProtocol::calculateTxPower(double rxPower, double txPower, double noise, double minSNR)
	{
		double snr = rxPower - noise;
		double neededPower = (txPower - (snr - minSNR));

		if (neededPower <= this->maxAllowedTxPower)
			neededPower = std::min(neededPower + 5, this->maxAllowedTxPower);
		//NS_LOG_DEBUG("rxPower = " << rxPower << "dBm, txPower = " << txPower << "dBm, noise = " << noise << "dBm, SNR = " << (rxPower - noise) << "dB, minSNR = " << minSNR << "dB, neededPower = " << neededPower);
		return neededPower;
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
	void EEBTProtocol::Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t pID, const Address &sender, const Address &receiver, NetDevice::PacketType pType)
	{
		EEBTPTag tag;
		EEBTPHeader header;
		Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(device);
		Mac48Address sender_addr = Mac48Address::ConvertFrom(sender);
		Mac48Address receiver_addr = Mac48Address::ConvertFrom(receiver);

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
		else if (header.GetFrameType() != CYCLE_CHECK)
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
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "/" << Now() << "]: ReachPower for node [" << node->getAddress() << "] is too high: " << node->getReachPower() << "dBm > " << this->maxAllowedTxPower << "dBm; FRAME_TYPE: " << (uint)header.GetFrameType());

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
						this->Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);
					}
				}
			}
		}

		switch (header.GetFrameType())
		{
		case CYCLE_CHECK:
			//If we are the recipient, we need some information from our neighbor list which intermediate nodes could not have since they don't have these neighbors
			if (receiver_addr == header.GetOriginator())
				this->handleCycleCheck(gs, node, Create<EEBTPNode>(header.GetOriginator(), this->maxAllowedTxPower), gs->getNeighbor(header.GetNewParent()), gs->getNeighbor(header.GetOldParent()));
			else
			{
				Ptr<EEBTPNode> newParent = gs->getNeighbor(header.GetNewParent());
				if (newParent == 0)
					newParent = Create<EEBTPNode>(header.GetNewParent(), this->maxAllowedTxPower);
				Ptr<EEBTPNode> oldParent = gs->getNeighbor(header.GetOldParent());
				if (oldParent == 0)
					oldParent = Create<EEBTPNode>(header.GetOldParent(), this->maxAllowedTxPower);

				if (gs->isChild(header.GetOriginator()))
					this->handleCycleCheck(gs, node, gs->getNeighbor(header.GetOriginator()), newParent, oldParent);
				else
					this->handleCycleCheck(gs, node, Create<EEBTPNode>(header.GetOriginator(), this->maxAllowedTxPower), newParent, oldParent);
			}
			break;
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

	/*
	 * Send methods
	 *
	 * The regular send method just takes the frame type, recipient and transmission power,
	 * build the EEBTPHeader with information from the GameState and hand it over to the final send method.
	 *
	 * There is also a send method for quick broadcast. It just takes the frame type and txPower and adds
	 * the broadcast address as recipient.
	 *
	 * To initialize a cycle check there is a send method which takes the originator, newParent and oldParent
	 * to build the EEBTPHeader and send it to the parent node
	 *
	 * The final send method takes care of repeating events (SendEvent), the sequence number and transmission power
	 */
	void EEBTProtocol::Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, double txPower)
	{
		EEBTPHeader header = EEBTPHeader();
		header.SetFrameType(ft);

		if (ft == CHILD_REJECTION && gs->isChild(recipient))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Node [" << recipient << "] is a child of mine, but it is marked as a child...");
			gs->removeChild(gs->getNeighbor(recipient));
		}

		this->Send(gs, header, recipient, txPower);
	}

	//Send method for Sendevent to check if a packet needs retransmission or not
	void EEBTProtocol::Send(Ptr<GameState> gs, FRAME_TYPE ft, Mac48Address recipient, uint16_t seqNo, double txPower, Ptr<SendEvent> event)
	{
		if (ft == CHILD_REQUEST || ft == CHILD_CONFIRMATION || ft == CHILD_REJECTION || ft == PARENT_REVOCATION || ft == END_OF_GAME)
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

				EEBTPHeader header;
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

	//Send method for quick broadcasts
	void EEBTProtocol::Send(Ptr<GameState> gs, FRAME_TYPE ft, double txPower)
	{
		this->Send(gs, ft, Mac48Address::GetBroadcast(), txPower);
	}

	//Send method for initializing cycle checks
	void EEBTProtocol::Send(Ptr<GameState> gs, Mac48Address originator, Mac48Address newParent, Mac48Address oldParent)
	{
		EEBTPHeader header = EEBTPHeader();
		header.SetFrameType(CYCLE_CHECK);

		header.SetOriginator(originator);
		header.SetNewParent(newParent);
		header.SetOldParent(oldParent);

		//If we have a parent, send cycle check to our parent
		//If we are currently switching our parent, it is not necessary since we will send a cycle check after connection successfully
		if (gs->getParent() != 0)
			this->Send(gs, header, gs->getParent()->getAddress(), gs->getParent()->getReachPower());
		else
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Cannot send cycle check since we have no parent");
	}

	//Send method for CCSendevent to check if a packet needs retransmission or not
	void EEBTProtocol::Send(Ptr<GameState> gs, Mac48Address originator, Mac48Address newParent, Mac48Address oldParent, uint16_t seqNo, double txPower, Ptr<CCSendEvent> event)
	{
		if (this->packetManager->isPacketAcked(seqNo))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has been acked. No retransmission, time = " << Now());
			this->packetManager->deleteSeqNoEntry(seqNo);
			return;
		}
		else if (this->packetManager->isPacketLost(seqNo) || event->getNTimes() > 20)
		{
			if (event->getNTimes() > 20)
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has not been acked yet. Retransmitting..., time = " << Now());
			else
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Packet with seqNo " << seqNo << " has been lost. Retransmitting..., time = " << Now());
			this->packetManager->deleteSeqNoEntry(seqNo);

			EEBTPHeader header = EEBTPHeader();
			header.SetFrameType(CYCLE_CHECK);

			header.SetOriginator(originator);
			header.SetNewParent(newParent);
			header.SetOldParent(oldParent);
			header.SetSequenceNumber(seqNo);

			//If we have a parent, send cycle check to our parent
			//If we are currently switching our parent, it is not necessary since we will send a cycle check after connection successfully
			if (gs->getParent() != 0)
				this->Send(gs, header, gs->getParent()->getAddress(), gs->getParent()->getReachPower() + 1, true);
			else
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Cannot send cycle check since we have no parent");
		}
		else
		{
			Time ttw = this->device->GetMac()->GetAckTimeout() * 200;
			//Simulator::Schedule(ttw, Create<CCSendEvent>(gs, this, originator, newParent, oldParent, txPower, seqNo));
			Simulator::Schedule(ttw, event);
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Rescheduled CCSendEvent(" << seqNo << ") for " << (Now() + ttw) << ", now = " << Now());
		}
	}

	void EEBTProtocol::Send(Ptr<GameState> gs, EEBTPHeader header, Mac48Address recipient, double txPower)
	{
		this->Send(gs, header, recipient, txPower, false);
	}

	//Final send method
	void EEBTProtocol::Send(Ptr<GameState> gs, EEBTPHeader header, Mac48Address recipient, double txPower, bool isRetransmission)
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

		if (header.GetFrameType() == NEIGHBOR_DISCOVERY) //Check the neighbor discovery event handler (SendEvent)
		{
			if (!this->checkNeigborDiscoverySendEvent(gs))
				return;
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

		header.setGameFinishedFlag(gs->gameFinished());

		packet->AddHeader(header);

		if (header.GetFrameType() == CYCLE_CHECK)
		{
			Time ttw = this->device->GetMac()->GetAckTimeout() * 100;
			Simulator::Schedule(ttw, Create<CCSendEvent>(gs, this, header.GetOriginator(), header.GetNewParent(), header.GetOldParent(), txPower, header.GetSequenceNumber()));
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Scheduled CCSendEvent(" << header.GetSequenceNumber() << ") for " << (Now() + ttw));
		}
		else if (header.GetFrameType() == CHILD_REQUEST || header.GetFrameType() == CHILD_CONFIRMATION || header.GetFrameType() == CHILD_REJECTION || header.GetFrameType() == PARENT_REVOCATION || header.GetFrameType() == END_OF_GAME)
		{
			Time ttw = this->device->GetMac()->GetAckTimeout() * 100;
			EventId id = Simulator::Schedule(ttw, Create<SendEvent>(gs, this, (FRAME_TYPE)header.GetFrameType(), recipient, txPower, header.GetSequenceNumber()));
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Scheduled SendEvent(" << (uint)header.GetFrameType() << ") for " << (Now() + ttw) << ". EventID = " << id.GetUid());
		}

		/*Ptr<EEBTPQueueDiscItem> qdi = Create<EEBTPQueueDiscItem>(packet, recipient, EEBTProtocol::PROT_NUMBER);
		this->tcl->Send(this->device, qdi);*/

		EEBTPTag tag;
		tag.setGameID(gs->getGameID());
		tag.setFrameType(header.GetFrameType());
		tag.setSequenceNumber(header.GetSequenceNumber());
		tag.setTxPower(header.GetTxPower());
		packet->AddPacketTag(tag);

		this->packetManager->sendPacket(packet, recipient);

		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: EEBTProtocol::Send(): " << this->myAddress << " => " << recipient << " / SeqNo: " << header.GetSequenceNumber() << " / FRAME_TYPE: " << (uint)header.GetFrameType() << " / txPower: " << header.GetTxPower() << "/" << this->wifiPhy->GetTxPowerStart() << "|" << this->wifiPhy->GetTxPowerEnd() << "dBm");
		NS_LOG_DEBUG("\thTx: " << header.GetHighestMaxTxPower() << ", shTx: " << header.GetSecondHighestMaxTxPower() << ", rounds: " << gs->getUnchangedCounter() << "/" << ((gs->getNNeighbors() * 0.5) + 2));
	}

	/*
	 * Handle the cycle check (FrameType 0)
	 */
	void EEBTProtocol::handleCycleCheck(Ptr<GameState> gs, Ptr<EEBTPNode> node, Ptr<EEBTPNode> originator, Ptr<EEBTPNode> newParent, Ptr<EEBTPNode> oldParent)
	{
		NS_ASSERT_MSG(originator != 0, "originator is null");
		NS_ASSERT_MSG(newParent != 0, "newParent is null");
		NS_ASSERT_MSG(oldParent != 0, "oldParent is null");
		NS_LOG_DEBUG(this->device->GetNode()->GetId() << " => EEBTProtocol::handleCycleCheck() | FROM: " << node->getAddress() << " | ORIG: " << originator->getAddress() << " | nP: " << newParent->getAddress() << " | oP: " << oldParent->getAddress());

		if (gs->isInitiator())
		{
			//NS_LOG_DEBUG("Ignoring cycle check from " << node->getAddress() << " because I am the initiator");
		}
		else if (originator->getAddress() == this->myAddress)
		{
			if (gs->getParent() != 0 && newParent->getAddress() == gs->getParent()->getAddress())
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Cycle detected! Connecting to last parent (" << oldParent->getAddress() << ") and blacklisting (" << newParent->getAddress() << "," << newParent->getParentAddress() << ")");

				//Set my parent with its parent on the blacklist
				gs->updateBlacklist(gs->getParent());

				this->disconnectOldParent(gs);

				//Remove all my previous parents from the list until the oldParent occurs
				if (gs->hasLastParents())
				{
					Ptr<EEBTPNode> p = gs->popLastParent();
					while (p != oldParent && gs->hasLastParents())
						p = gs->popLastParent();
				}

				//Try to connect to one of our last parents
				//We get automatically disconnected from our current parent
				Ptr<EEBTPNode> lastParent = gs->popLastParent();
				while (lastParent != 0)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Last parent is also blacklisted, contacting another last parent [" << lastParent->getAddress() << "]");
					this->contactNode(gs, lastParent);

					//If we cannot contact lastParent, get next lastParent from stack, else break the loop
					if (gs->getContactedParent() == 0)
						lastParent = gs->popLastParent();
					else
						break;
				}

				//If we could not contact a lastParent, search for cheapest neighbor
				if (gs->getContactedParent() == 0)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: None of our last parents is an option, searching cheapest neighbor...");
					this->contactCheapestNeighbor(gs);

					//If we are not able to find a valid cheapest neighbor, disconnect child nodes and try again
					if (gs->getContactedParent() == 0)
					{
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unable to connect to any other node. Disconnecting children...");

						this->disconnectAllChildNodes(gs);

						this->contactCheapestNeighbor(gs);
					}
				}
			}
			else
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Cycle detected! newParent: " << newParent->getAddress() << ", currentParent: " << ((gs->getParent() != 0) ? gs->getParent()->getAddress() : "ff:ff:ff:ff:ff:ff"));

				//Blacklist newParent, since this route creates a cycle
				gs->updateBlacklist(newParent);
			}
		}
		else
		{
			//We are not the originator of that packet nor the initiator of the game. Sending this packet to our parent
			this->Send(gs, originator->getAddress(), newParent->getAddress(), oldParent->getAddress());
		}
	}

	/*
	 * Handle neighbor discovery (FrameType 1)
	 */
	void EEBTProtocol::handleNeighborDiscovery(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: EEBTProtocol::handleNeighborDiscovery()");

		if (gs->isInitiator()) //Check if I am the initiator. If yes, we can ignore this packet
		{
			//NS_LOG_DEBUG("Ignoring neighbor discovery from " << node->getAddress() << " because I am the initiator");
		}
		else if (gs->isBlacklisted(node)) //If the sender is on our blacklist due to a cycle, ignore
		{
			//NS_LOG_DEBUG("Ignoring neighbor discovery from " << node->getAddress() << " because it and its parent (" << node->getParentAddress() << ") are blacklisted.");
		}
		else if (gs->isChild(node)) //Check if sender is child of me
		{
			//NS_LOG_DEBUG("Ignoring neighbor discovery from " << node->getAddress() << " because it is a child of mine");

			//If the reachpower of this child changed, inform neighbors
			if (node->reachPowerChanged() && gs->gameFinished())
				this->Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);
		}
		else if (node->getReachPower() > this->maxAllowedTxPower)
		{
			//NS_LOG_DEBUG("Ignoring neighbor discovery from " << node->getAddress() << " because I cannot reach it (" << node->getReachPower() << " dBm)");
		}
		else if (node->getConnCounter() > 5)
		{
			//NS_LOG_DEBUG("Ignoring neighbor discovery from " << node->getAddress() << " because connection counter exceeds the maximum");
		}
		else if (node == gs->getLastParent())
		{
			//NS_LOG_DEBUG("Ignoring neighbor discovery from " << node->getAddress() << " because it is my last parent");
		}
		else if (gs->getContactedParent() == 0)
		{
			//If we have no parent and are not connecting to on
			if (gs->getParent() == 0)
			{
				gs->resetRejectionCounter();
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Switching to [" << node->getAddress() << "] because of neighbor discovery");
				return this->contactNode(gs, node);
			}
			else if (gs->getParent() != 0 && node != gs->getParent())
			{
				//Cost of current connection (connCost) is 0 if we are (one of) the nodes that are the farthest away
				Ptr<EEBTPNode> parent = gs->getParent();
				double connCost = (parent->getHighestMaxTxPower() - parent->getReachPower());

				//Only if we are (one of) the nodes that are the farthest away, it is useful to switch (else no savings)
				if (connCost <= 0.00001 && connCost >= -0.00001)
				{
					//TX power our parent can save, if we leave
					double saving = DbmToW(gs->getParent()->getHighestMaxTxPower()) - DbmToW(gs->getParent()->getSecondHighestMaxTxPower());

					//Cost of the new connection is the difference between the node's highest tx power and the reach power to this node
					double costOfNewConn = DbmToW(node->getReachPower()) - DbmToW(node->getHighestMaxTxPower());

					//If we are actually saving tx power, switch
					if (costOfNewConn <= saving)
					{
						if (costOfNewConn < saving + 0.0001 && costOfNewConn > saving - 0.0001 && (gs->gameFinished() || gs->getUnchangedCounter() >= EEBTProtocol::MAX_UNCHANGED_ROUNDS))
							NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Ignoring neighbor discovery from [" << node->getAddress() << "] since we cannot realy save energy an we had too many unchanged roundes");
						else
						{
							NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Switching to [" << node->getAddress() << "] because of neighbor discovery");
							NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: "
												  << "rp(" << node->getAddress() << ") = " << node->getReachPower() << ", "
												  << "hTx(" << node->getAddress() << ") = " << node->getHighestMaxTxPower() << ", "
												  << "shTx(" << node->getAddress() << ") = " << node->getSecondHighestMaxTxPower());
							NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: "
												  << "rp(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getReachPower() << ", "
												  << "hTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getHighestMaxTxPower() << ", "
												  << "shTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getSecondHighestMaxTxPower());
							NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: saving = " << saving << "W, costOfNewConn = " << costOfNewConn << "W");

							this->contactNode(gs, node);
						}
					}
					else
					{
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << " / " << Now() << "]: [" << node->getAddress() << "] is NOT a better choice than our current parent");
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: "
											  << "rp(" << node->getAddress() << ") = " << node->getReachPower() << ", "
											  << "hTx(" << node->getAddress() << ") = " << node->getHighestMaxTxPower() << ", "
											  << "shTx(" << node->getAddress() << ") = " << node->getSecondHighestMaxTxPower());
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: "
											  << "rp(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getReachPower() << ", "
											  << "hTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getHighestMaxTxPower() << ", "
											  << "shTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getSecondHighestMaxTxPower());
						NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: saving = " << saving << ", costOfNewConn = " << costOfNewConn);
					}
				}
				else
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << " / " << Now() << "]: Ignoring neighbor discovery from [" << node->getAddress() << "] since we cannot save energy by leaving our parent: "
										  << DbmToW(gs->getParent()->getHighestMaxTxPower()) << " - " << DbmToW(gs->getParent()->getSecondHighestMaxTxPower()) << " = " << gs->getCostOfCurrentConn());
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: "
										  << "rp(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getReachPower() << ", "
										  << "hTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getHighestMaxTxPower() << ", "
										  << "shTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getSecondHighestMaxTxPower());
				}
			}
		}
	}

	/*
	 * Handle child request (FrameType 2)
	 * Sent by child, received by parent
	 * if a child wants to connect to the parent node
	 */
	void EEBTProtocol::handleChildRequest(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//NS_LOG_DEBUG(this->device->GetNode()->GetId() << " => EEBTProtocol::handleChildRequest()");

		//Check if we received a parent revocation after this child request
		if (gs->checkLastFrameType(node->getAddress(), CHILD_REQUEST, gs->getLastSeqNo(node->getAddress(), PARENT_REVOCATION)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child request from [" << node->getAddress() << "] dismissed because we received a PARENT_REVOCATION with a higher sequence number earlier.");
			return;
		}

		//Reject child request if the reach power for this node exceeds our maxAllowedTxPower
		if (node->getReachPower() > this->maxAllowedTxPower)
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child request from [" << node->getAddress() << "] dismissed because of reach power (" << node->getReachPower() << ")");
			return this->Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
		}

		if (gs->getParent() == node || gs->getContactedParent() == node)
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child request from [" << node->getAddress() << "] dismissed because it's my parent");
			return EEBTProtocol::Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());
		}

		//This should never happen but it is implemented for a more stable process
		//If we are not the initiator and we are not connected to the source node and did not mark any other node to connect to...
		if (!gs->isInitiator() && (gs->getParent() == 0 && gs->getContactedParent() == 0))
		{
			//...reject request
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child request from [" << node->getAddress() << "] dismissed because I am not connected to a parent");
			this->Send(gs, CHILD_REJECTION, node->getAddress(), node->getReachPower());

			if (gs->getRejectionCounter() > (gs->getNNeighbors() * 2) && gs->getParent() == 0)
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Do not search for new parent since we failed to many times. Waiting for new neighbor");
			else
			{
				//and contact the cheapest neighbor
				return this->contactCheapestNeighbor(gs);
			}
		}

		//NS_ASSERT_MSG(!gs->isChild(node), "Child sent CHILD_REQUEST but it is already a child of mine");
		if (gs->isChild(node))
		{
			NS_LOG_WARN("[Node " << this->device->GetNode()->GetId() << "]: Node [" << node->getAddress() << "] sent a child request but is already a child of mine");
			this->Send(gs, CHILD_CONFIRMATION, node->getAddress(), node->getReachPower());
		}
		else
		{
			//Store new child in child list
			gs->addChild(node);

			//Update the node's parent address
			node->setParentAddress(this->myAddress);

			//Send child confirmation
			this->Send(gs, CHILD_CONFIRMATION, node->getAddress(), node->getReachPower());

			//Reset the unchanged counter since the topology changed
			if (!gs->gameFinished())
				gs->resetUnchangedCounter();
			else //else, force new child to finish its game since application data may arrive soon
				this->handleEndOfGame(gs, node);

			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child request from [" << node->getAddress() << "] accepted");
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: My current child nodes:");
			for (int i = 0; i < gs->getNChilds(); i++)
				NS_LOG_DEBUG("\t\t[" << gs->getChild(i)->getAddress() << "]: rP = " << gs->getChild(i)->getReachPower() << "dBm, finished: " << (gs->getChild(i)->hasFinished() ? "true" : "false"));
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: hTx = " << gs->getHighestTxPower() << ", shTx = " << gs->getSecondHighestTxPower());

			//If game is finished, we already sent application data frames and now have one child, continue sending application data
			if (gs->gameFinished() && gs->getApplicationDataHandler()->getLastSeqNo() > 0 && gs->getNChilds() == 1)
				this->sendApplicationData(gs, gs->getApplicationDataHandler()->getLastSeqNo());
		}
	}

	/*
	 * Handle child confirmation (FrameType 3)
	 * Sent by parent, received by child
	 * if parent accepts the child request
	 */
	void EEBTProtocol::handleChildConfirmation(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		if (gs->checkLastFrameType(node->getAddress(), CHILD_CONFIRMATION, gs->getLastSeqNo(node->getAddress(), CHILD_REJECTION)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child confirmation from [" << node->getAddress() << "] dismissed because we received a CHILD_REJECTION with a higher sequence number earlier.");
			return this->Send(gs, PARENT_REVOCATION, node->getAddress(), node->getReachPower());
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
			this->contactCheapestNeighbor(gs);
		}

		//If we did not contact another node...
		if (gs->getContactedParent() == 0)
		{
			node->resetConnCounter();

			//...and have child nodes, do a cycle check
			if (gs->hasChilds())
			{
				NS_LOG_DEBUG("Sending cycle_check: " << node->getParentAddress() << ", reachPower: " << node->getReachPower());
				//NS_ASSERT_MSG(gs->hasLastParents(), "We never had a parent but should have, since we have child nodes");
				if (gs->hasLastParents())
					this->Send(gs, this->myAddress, gs->getParent()->getAddress(), gs->getLastParent()->getAddress());
				else
					this->Send(gs, this->myAddress, gs->getParent()->getAddress(), gs->getParent()->getAddress());
				gs->setCycleCheckNeeded(false);
			}

			if (node->hasFinished())
				this->handleEndOfGame(gs, node);
			else if (gs->gameFinished()) //If we finished our game earlier, send END_OF_GAME to our parent
				this->Send(gs, END_OF_GAME, gs->getParent()->getAddress(), gs->getParent()->getReachPower());

			//Reset the unchanged counter, since our topology changed
			if (!gs->doIncrAfterConfirm() && gs->getUnchangedCounter() < EEBTProtocol::MAX_UNCHANGED_ROUNDS)
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Resetting unchanged counter");
				gs->resetUnchangedCounter();
			}

			gs->setDoIncrAfterConfirm(false);

			//Inform our neighbors
			this->Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);

			gs->resetRejectionCounter();
		}
		else
			gs->setCycleCheckNeeded(true);
	}

	/*
	 * Handle child rejection (FrameType 4)
	 * Sent by parent, received by child
	 * if a parent rejects a child request
	 */
	void EEBTProtocol::handleChildRejection(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		if (gs->checkLastFrameType(node->getAddress(), CHILD_REJECTION, gs->getLastSeqNo(node->getAddress(), CHILD_CONFIRMATION)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Child rejection from [" << node->getAddress() << "] dismissed because we received a CHILD_CONFIRMATION with a higher sequence number earlier.");
			return;
		}

		//Check if node is our parent
		if (node != gs->getParent() && node != gs->getContactedParent())
		{
			NS_LOG_DEBUG("Ignoring child rejection from [" << node->getAddress() << "] since it is not our parent nor the node we want to connect to!");
			return;
		}

		if (node == gs->getParent())
		{
			gs->setParent(0);
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: My parent [" << node->getAddress() << "] disconnected me");

			//Check our last cycle and set the finish time
			std::vector<Ptr<CycleInfo>> cycles = this->cycleWatchDog->getCycles(gs->getGameID(), this->device->GetNode()->GetId());
			if (cycles.size() > 0)
			{
				Ptr<CycleInfo> ci = *(cycles.end() - 1);
				if (ci->getEndTime().GetNanoSeconds() == 0)
					ci->setEndTime(Now());
			}

			if (gs->gameFinished() && gs->getContactedParent() != 0)
			{
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: I am connecting to [" << gs->getContactedParent()->getAddress() << "]...");
				return;
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

		//Reset contacted parent
		gs->setContactedParent(0);

		if (gs->getRejectionCounter() > (gs->getNNeighbors() * 2) && gs->getParent() == 0)
		{
			gs->resetBlacklist();
			this->disconnectAllChildNodes(gs);
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Failed " << (gs->getNNeighbors() * 2) << " times to connect to any node. Waiting for new neighbor discovery frames");
			return;
		}

		//If our game is not finished yet
		if (!gs->gameFinished())
		{
			//If the new parent rejects us and we have a last parent, connect to the old one
			if (gs->hasLastParents())
			{
				Ptr<EEBTPNode> lastParent = gs->popLastParent();
				while (lastParent != 0)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Contacting last parent [" << lastParent->getAddress() << "]");
					this->contactNode(gs, lastParent);

					//If we cannot contact lastParent, get next lastParent from stack, else break the loop
					if (gs->getContactedParent() == 0)
						lastParent = gs->popLastParent();
					else
						break;
				}
			}
		}

		//If we did not found a not blacklisted old parent, contact cheapest neighbor
		if (gs->getContactedParent() == 0)
		{
			//Contact cheapest neighbor
			this->contactCheapestNeighbor(gs);

			if (gs->getContactedParent() == 0)
			{
				//If the game is already finished and we did not disconnect our old parent
				if (gs->gameFinished() && gs->getParent() != 0)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: We already finished our game and did not disconnect from our old parent.");
					if (gs->needsCycleCheck() && gs->hasChilds())
					{
						NS_LOG_DEBUG("Sending cycle_check: " << node->getParentAddress() << ", reachPower: " << node->getReachPower());
						//NS_ASSERT_MSG(gs->hasLastParents(), "We never had a parent but should have, since we have child nodes");
						if (gs->hasLastParents())
							this->Send(gs, this->myAddress, gs->getParent()->getAddress(), gs->getLastParent()->getAddress());
						else
							this->Send(gs, this->myAddress, gs->getParent()->getAddress(), gs->getParent()->getAddress());
						gs->setCycleCheckNeeded(false);
					}
					return;
				}
				else
				{
					//If this also fails, disconnect child nodes and try again
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Unable to connect to any other node. Disconnecting children...");
					this->disconnectAllChildNodes(gs);

					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Searching for cheapest neighbor...");
					this->contactCheapestNeighbor(gs);
				}
			}

			//Reset doIncrementAfterConfirm
			gs->setDoIncrAfterConfirm(false);

			//Reset unchanged counter
			gs->resetUnchangedCounter();
		}

		gs->incrementRejectionCounter();
	}

	/*
	 * Handle parent revocation (FrameType 5)
	 * Sent by child, received by parent
	 * if child wants to disconnect from a parent
	 */
	void EEBTProtocol::handleParentRevocation(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//NS_LOG_DEBUG(this->device->GetNode()->GetId() << " => EEBTProtocol::handleParentRevocation()");

		if (gs->checkLastFrameType(node->getAddress(), PARENT_REVOCATION, gs->getLastSeqNo(node->getAddress(), CHILD_REQUEST)))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Parent revocation from [" << node->getAddress() << "] dismissed because we received a CHILD_REQUEST with a higher sequence number earlier.");
			return;
		}

		//Assertion
		if (gs->isChild(node->getAddress()))
		{
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
			}

			if (gs->getParent() != 0 || gs->isInitiator())
				this->Send(gs, NEIGHBOR_DISCOVERY, this->maxAllowedTxPower);
		}
	}

	/*
	 * Handle end of game (FrameType 6)
	 * Sent by child, received by parent
	 * if a child has finished the game
	 */
	void EEBTProtocol::handleEndOfGame(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//NS_LOG_DEBUG(this->device->GetNode()->GetId() << " => EEBTProtocol::handleEndOfGame()");

		if (node == gs->getParent() && gs->getContactedParent() == 0)
		{
			gs->resetNeighborDiscoveryEvent();

			NS_LOG_DEBUG("New parent is finished. finishing my game. my children: " << gs->getNChilds());
			if (gs->hasChilds() && !gs->allChildsFinished())
			{
				for (int i = 0; i < gs->getNChilds(); i++)
				{
					Ptr<EEBTPNode> child = gs->getChild(i);
					this->Send(gs, END_OF_GAME, child->getAddress(), child->getReachPower());
					this->handleEndOfGame(gs, child);
				}
			}

			node->setFinished(true);
			gs->finishGame();
		}
		else if (gs->isChild(node->getAddress()))
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: A child [" << node->getAddress() << "] finished the game.");
			node->setFinished(true);
		}
	}

	/*
	 * Handle application data (FrameType 7)
	 * Sent by parent, received by child
	 */
	void EEBTProtocol::handleApplicationData(Ptr<GameState> gs, Ptr<EEBTPNode> node, Ptr<Packet> packet)
	{
		//NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: EEBTProtocol::handleApplicationData()");

		EEBTPHeader hdr;
		packet->RemoveHeader(hdr);

		//Store data
		if (gs->getApplicationDataHandler()->handleApplicationData(packet) && gs->hasChilds())
		{
			this->sendApplicationData(gs, packet);
		}
	}

	/*
	 * Helper methods
	 */

	/*
	 * Searches the cheapest neighbor and contacts it (CHILD_REQUEST)
	 * if it is not already our parent
	 */
	void EEBTProtocol::contactCheapestNeighbor(Ptr<GameState> gs)
	{
		//Get cheapest neighbor from our neighbor list
		Ptr<EEBTPNode> cheapestNeighbor = gs->getCheapestNeighbor();

		//If the cheapest neighbor is not our parent
		if (cheapestNeighbor != gs->getParent())
		{
			if (gs->getParent() != 0)
			{
				double saving = DbmToW(gs->getParent()->getHighestMaxTxPower()) - DbmToW(gs->getParent()->getSecondHighestMaxTxPower());
				double costOfNewConn = DbmToW(cheapestNeighbor->getReachPower()) - DbmToW(cheapestNeighbor->getHighestMaxTxPower());

				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: "
									  << "rp(" << cheapestNeighbor->getAddress() << ") = " << cheapestNeighbor->getReachPower() << ", "
									  << "hTx(" << cheapestNeighbor->getAddress() << ") = " << cheapestNeighbor->getHighestMaxTxPower() << ", "
									  << "shTx(" << cheapestNeighbor->getAddress() << ") = " << cheapestNeighbor->getSecondHighestMaxTxPower());
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: "
									  << "rp(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getReachPower() << ", "
									  << "hTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getHighestMaxTxPower() << ", "
									  << "shTx(" << gs->getParent()->getAddress() << ") = " << gs->getParent()->getSecondHighestMaxTxPower());
				NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: saving = " << saving << "W, costOfNewConn = " << costOfNewConn << "W");
			}

			//Contact cheapest neighbor
			this->contactNode(gs, cheapestNeighbor);
		}
	}

	/*
	 * This method handles the CHILD_REQUEST to new parents
	 */
	void EEBTProtocol::contactNode(Ptr<GameState> gs, Ptr<EEBTPNode> node)
	{
		//First check, if this node is blacklisted
		if (gs->isBlacklisted(node))
		{
			NS_LOG_DEBUG("Node [" << node->getAddress() << "] with its parent [" << node->getParentAddress() << "] is blacklisted");
			return;
		}

		//Check if the node is a child of us
		if (gs->isChild(node))
		{
			NS_LOG_DEBUG("Node [" << node->getAddress() << "] is a child of mine");
			return;
		}

		//If our parent is the node, return. We don't want to connect to it again
		if (gs->getParent() == node)
		{
			NS_LOG_DEBUG("Node is our parent!");
			return;
		}

		if (node->getReachPower() > this->maxAllowedTxPower)
		{
			NS_LOG_DEBUG("Cannot contact [" << node->getAddress() << "] because I cannot reach it (" << node->getReachPower() << " dBm)");
			return;
		}

		//If we are in the process of contacting another node, we note that
		//we should check our neighbors after finishing that process
		if (gs->getContactedParent() != 0)
		{
			NS_LOG_DEBUG("Cannot contact [" << node->getAddress() << "] while waiting for response of contacted node [" << gs->getContactedParent()->getAddress() << "]");
			return;
		}

		Ptr<EEBTPNode> parent = gs->getParent();

		gs->resetNeighborDiscoveryEvent();

		if (parent != 0)
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Switching from [" << parent->getAddress() << "] to [" << node->getAddress() << "]");

			//If the connection to the new parent costs the same as to the old parent,
			//we still increment the unchanged counter since we don't want to end in a loop
			double costOfCurrentConn = gs->getCostOfCurrentConn();
			double saving = DbmToW(parent->getHighestMaxTxPower()) - DbmToW(parent->getSecondHighestMaxTxPower());
			double costOfNewConn = DbmToW(node->getReachPower()) - DbmToW(node->getHighestMaxTxPower());

			//Only if we are one of the farthest nodes away, we can save energy
			if (costOfCurrentConn < 0.0001 && costOfCurrentConn > -0.0001)
			{
				//If the amount of energy we save is the same as the new costs of energy, set 'doIncr' flag
				if (saving < costOfNewConn + 0.0001 && saving > costOfNewConn - 0.0001)
				{
					NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Setting doIncrAfterConfirm since saving ~= costOfNewConn => " << saving << " ~= " << costOfNewConn);
					gs->setDoIncrAfterConfirm(true);
				}
			}
		}
		else
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Connecting to [" << node->getAddress() << "]");

		//Check if we have a parent or not
		if (!gs->gameFinished())
			this->disconnectOldParent(gs);
		else
			NS_LOG_DEBUG("\tGame has finished. We disconnect after the next successful connection");

		//Set contacted parent
		gs->setContactedParent(node);

		//Send child request
		this->Send(gs, CHILD_REQUEST, node->getAddress(), node->getReachPower());
	}

	void EEBTProtocol::finishGame(Ptr<GameState> gs)
	{
		if (gs->getContactedParent() != 0)
		{
			//NS_LOG_DEBUG("Cannot finish game, because we are waiting for a response...");
			return;
		}

		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Finishing game...");

		//Set finish flag
		gs->finishGame();

		gs->resetNeighborDiscoveryEvent();

		Ptr<EEBTPNode> parent = gs->getParent();

		//If we have a parent (Assertion)
		if (parent != 0)
		{
			//inform parent that we have finished the game
			this->Send(gs, END_OF_GAME, parent->getAddress(), parent->getReachPower());
		}
		else if (gs->isInitiator())
		{
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: I am the initiator and all nodes have finished the game at " << Now() << ". Start sending application data...");
			this->sendApplicationData(gs, 0);
		}
	}

	bool EEBTProtocol::checkNeigborDiscoverySendEvent(Ptr<GameState> gs)
	{
		//If the event handler is not set, create one
		if (gs->getNeighborDiscoveryEvent() == 0)
			gs->setNeighborDiscoveryEvent(Create<SendEvent>(gs, this, NEIGHBOR_DISCOVERY, Mac48Address::GetBroadcast(), this->maxAllowedTxPower, 0));

		//If we sent more than MAX_UNCHANGED_ROUNDS times a neighbor discovery...
		//uint32_t maxUnchangedCounter = ((gs->getNNeighbors() * 0.5) + 2);
		if ((gs->getUnchangedCounter() >= EEBTProtocol::MAX_UNCHANGED_ROUNDS && gs->allChildsFinished()) && (!gs->isInitiator() || gs->getNChilds() > 0))
		{
			//Check for the cheapest neighbor, if we are not the initiator
			if (!gs->isInitiator())
				this->contactCheapestNeighbor(gs);

			//and deactivate the handler
			gs->resetNeighborDiscoveryEvent();

			//If we did not contact a new node, finish the game
			if (gs->getContactedParent() == 0)
				this->finishGame(gs);
		}
		else
		{
			//The event should fire every 500 slots (4500 microseconds)
			Time slotTime = this->device->GetMac()->GetSlot();
			Simulator::Schedule(MicroSeconds(slotTime.GetMicroSeconds() * 2000), gs->getNeighborDiscoveryEvent());

			gs->incrementUnchangedCounter();
		}

		return (!gs->gameFinished());
	}

	void EEBTProtocol::disconnectOldParent(Ptr<GameState> gs)
	{
		Ptr<EEBTPNode> parent = gs->getParent();

		if (parent != 0)
		{
			//Send parent revocation to current parent
			this->Send(gs, PARENT_REVOCATION, parent->getAddress(), parent->getReachPower());

			//Check our last cycle and set the finish time
			std::vector<Ptr<CycleInfo>> cycles = this->cycleWatchDog->getCycles(gs->getGameID(), this->device->GetNode()->GetId());
			NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: Cycles stored: " << cycles.size());
			if (cycles.size() > 0)
			{
				Ptr<CycleInfo> ci = *(cycles.end() - 1);
				if (ci->getEndTime().GetNanoSeconds() == 0)
				{
					NS_LOG_DEBUG("\tEnding last cycle...");
					ci->setEndTime(Now());
				}
				else
					NS_LOG_DEBUG("\tLast cycle end time: " << ci->getEndTime());
			}

			//Add parent to last parent stack
			if (!gs->isBlacklisted(parent))
				gs->pushLastParent(parent);

			parent->setHighestMaxTxPower(WToDbm(0));
			parent->setSecondHighestMaxTxPower(WToDbm(0));

			//Set parent to null
			gs->setParent(0);
		}
	}

	void EEBTProtocol::disconnectAllChildNodes(Ptr<GameState> gs)
	{
		while (gs->getNChilds() > 0)
		{
			Ptr<EEBTPNode> child = gs->getChild(0);
			child->setSrcPath(std::vector<Mac48Address>());
			this->Send(gs, CHILD_REJECTION, child->getAddress(), child->getReachPower());
			gs->removeChild(child);
		}

		gs->resetBlacklist();
		gs->clearLastParents();
		gs->resetUnchangedCounter();
	}

	void EEBTProtocol::sendApplicationData(Ptr<GameState> gs, Ptr<Packet> packet)
	{
		gs->findHighestTxPowers();

		EEBTPHeader header;

		//Set sequence number
		this->cache.injectSeqNo(&header);

		if (gs->isInitiator())
			Simulator::Schedule(MilliSeconds(10), Create<ADSendEvent>(gs, this, header.GetSequenceNumber()));

		header.SetFrameType(APPLICATION_DATA);
		header.SetGameId(gs->getGameID());
		header.SetTxPower(gs->getHighestTxPower());
		//header.SetTxPower(this->maxAllowedTxPower);

		if (gs->getParent() == 0)
			header.SetParent(Mac48Address::GetBroadcast());
		else
			header.SetParent(gs->getParent()->getAddress());

		header.SetHighestMaxTxPower(gs->getHighestTxPower());
		header.SetSecondHighestMaxTxPower(gs->getSecondHighestTxPower());
		header.setGameFinishedFlag(gs->gameFinished());

		EEBTPDataHeader dataHeader;
		packet->PeekHeader(dataHeader);
		packet->AddHeader(header);

		EEBTPTag tag;
		packet->RemovePacketTag(tag);
		tag.setGameID(gs->getGameID());
		tag.setFrameType(header.GetFrameType());
		tag.setSequenceNumber(header.GetSequenceNumber());
		tag.setTxPower(header.GetTxPower());
		packet->AddPacketTag(tag);

		this->packetManager->sendPacket(packet, Mac48Address::GetBroadcast());
		NS_LOG_DEBUG("[Node " << this->device->GetNode()->GetId() << "]: " << this->device->GetAddress() << " => " << Mac48Address::GetBroadcast() << " / SeqNo: " << header.GetSequenceNumber() << " / dSeqNo: " << dataHeader.GetSequenceNumber() << " / txPower: " << header.GetTxPower() << "/" << this->wifiPhy->GetTxPowerStart() << "|" << this->wifiPhy->GetTxPowerEnd() << "dBm");
	}

	void EEBTProtocol::sendApplicationData(Ptr<GameState> gs, uint16_t seqNo)
	{
		if (gs->isInitiator() && gs->hasChilds())
		{
			if (this->sendCounter < this->maxPackets)
			{
				if (this->packetManager->isPacketAcked(seqNo) || this->sendCounter == 0)
				{
					EEBTPDataHeader dataHeader;
					dataHeader.SetDataLength(this->dataLength);
					dataHeader.SetSequenceNumber(gs->getApplicationDataHandler()->getLastSeqNo());
					gs->getApplicationDataHandler()->incrementSeqNo();
					uint8_t *data = new uint8_t[dataHeader.GetDataLength()];

					Ptr<Packet> packet = Create<Packet>(data, dataHeader.GetDataLength());
					packet->AddHeader(dataHeader);
					this->sendApplicationData(gs, packet);

					this->sendCounter++;
				}
				else
					Simulator::Schedule(MilliSeconds(10), Create<ADSendEvent>(gs, this, seqNo));
			}
		}
	}
}
