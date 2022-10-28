/*
 * SeqNoCache.h
 *
 *  Created on: 02.06.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_GAMESTATE_H_
#define BROADCAST_GAMESTATE_H_

#include "map"
#include "stack"
#include "ns3/mac48-address.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"
#include "ns3/traffic-control-layer.h"

#include "EEBTPHeader.h"
#include "ApplicationDataHandler.h"

namespace ns3
{
	class SendEvent;
	class CCSendEvent;
	class MutexSendEvent;
	class ParentPathCheckEvent;

	/*
	 * A Node holds information about a particular (other)
	 * node in the network.
	 */
	class EEBTPNode : public Object
	{
	public:
		EEBTPNode();
		EEBTPNode(Mac48Address addr, double power);
		virtual ~EEBTPNode();

		//static TypeId GetTypeId();
		//virtual TypeId GetInstanceTypeId() const;

		double getHighestMaxTxPower();
		void setHighestMaxTxPower(double maxTxPower);

		double getSecondHighestMaxTxPower();
		void setSecondHighestMaxTxPower(double maxTxPower);

		double getReachPower();
		void setReachPower(double minTxPower);

		double getNoise();
		void updateRxInfo(double rxPower, double noise);

		bool hasReachPowerProblem();
		void hasReachPowerProblem(bool b);

		Mac48Address getAddress();

		Mac48Address getParentAddress();
		void setParentAddress(Mac48Address addr);

		bool hasFinished();
		void setFinished(bool f);

		bool isOnPath(Mac48Address addr);
		std::vector<Mac48Address> getSrcPath();
		void setSrcPath(std::vector<Mac48Address> path);

		bool reachPowerChanged();
		void resetReachPowerChanged();

		bool pathChanged();
		void resetPathChanged();

		uint32_t getConnCounter();
		void incrementConnCounter();
		void resetConnCounter();

	private:
		bool finished;

		bool hasRpProblem;

		bool rpChanged;
		bool pChanged;

		double noise;
		double rxPower;
		double reachPower;
		double highest_maxTxPower;
		double second_maxTxPower;

		uint32_t connCounter;

		Mac48Address parent;
		Mac48Address address;

		std::vector<Mac48Address> srcPath;
	};

	/*
	 * The GameState holds general information about a
	 * particular game identified by its gameID.
	 */
	class GameState : public Object
	{
	public:
		GameState();
		GameState(bool initiator, uint64_t gid);
		virtual ~GameState();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;

		bool isInitiator();
		uint64_t getGameID();

		bool needsCycleCheck();
		void setCycleCheckNeeded(bool cc);

		Mac48Address getMyAddress();
		void setMyAddress(Mac48Address addr);

		void finishGame();
		bool gameFinished();
		Time getTimeFinished();

		uint16_t getLastSeqNo(Mac48Address sender, uint8_t ft);
		bool checkLastFrameType(Mac48Address sender, uint8_t ft, uint16_t seqNo);
		void updateLastFrameType(Mac48Address sender, uint8_t ft, uint16_t seqNo);

		uint32_t getUnchangedCounter();
		void incrementUnchangedCounter();
		void resetUnchangedCounter();

		Ptr<EEBTPNode> getParent();
		void setParent(Ptr<EEBTPNode> p);

		Ptr<EEBTPNode> getContactedParent();
		void setContactedParent(Ptr<EEBTPNode> p);

		bool hasLastParents();
		Ptr<EEBTPNode> popLastParent();
		Ptr<EEBTPNode> getLastParent();
		uint32_t getLastParentConnCounter();
		void pushLastParent(Ptr<EEBTPNode> parent);
		void clearLastParents();

		void findHighestTxPowers();
		double getCostOfCurrentConn();
		double getHighestTxPower();
		double getSecondHighestTxPower();

		bool isNeighbor(Mac48Address n);
		void addNeighbor(Mac48Address n);
		Ptr<EEBTPNode> getNeighbor(uint32_t index);
		Ptr<EEBTPNode> getNeighbor(Mac48Address n);
		Ptr<EEBTPNode> getCheapestNeighbor();
		void removeNeighbor(Ptr<EEBTPNode> n);
		uint32_t getNNeighbors();

		bool hasChilds();
		bool isChild(Mac48Address c);
		bool isChild(Ptr<EEBTPNode> c);
		void addChild(Ptr<EEBTPNode> c);
		void removeChild(Ptr<EEBTPNode> c);
		int getNChilds();
		Ptr<EEBTPNode> getChild(int index);
		bool allChildsFinished();

		bool isBlacklisted(Ptr<EEBTPNode> node);
		void updateBlacklist(Ptr<EEBTPNode> node);
		void resetBlacklist();
		bool isBlacklisted(Mac48Address node, Mac48Address parent);

		bool doIncrAfterConfirm();
		void setDoIncrAfterConfirm(bool v);

		Ptr<SendEvent> getNeighborDiscoveryEvent();
		void setNeighborDiscoveryEvent(Ptr<SendEvent> evt);
		void resetNeighborDiscoveryEvent();

		/*
			 * Mutex
			 */
		bool isLocked();
		bool isLocked(Ptr<EEBTPNode> node);
		void lock(bool l);
		void lock(Ptr<EEBTPNode> node, bool l);

		int getNChildsLocked();
		void resetChildLocks();

		Mac48Address getLockedBy();
		void setLockedBy(Mac48Address addr);

		bool isParentWaitingForLock();
		void setParentWaitingForLock(bool b);
		Ptr<EEBTPNode> getParentWaitingLockOriginator();
		void setParentWaitingLockOriginator(Ptr<EEBTPNode> originator);

		bool isNewParentWaitingForLock();
		void setNewParentWaitingForLock(bool b);
		Ptr<EEBTPNode> getNewParentWaitingLockOriginator();
		void setNewParentWaitingLockOriginator(Ptr<EEBTPNode> originator);

		Ptr<ApplicationDataHandler> getApplicationDataHandler();

		uint32_t getRejectionCounter();
		void incrementRejectionCounter();
		void resetRejectionCounter();
		void setRejectionCounter(uint32_t rj);

		Time getLastParentUpdate();
		void setLastParentUpdate(Time t);

		void resetParentUnchangedCounter();
		uint32_t getParentUnchangedCounter();
		void incrementParentUnchangedCounter();

		Ptr<ParentPathCheckEvent> getPPCEvent();
		void setPPCEvent(Ptr<ParentPathCheckEvent>);

		bool hadEmptyPathOnConnect();
		void setEmptyPathOnConnect(bool b);

	private:
		bool initiator;
		bool endOfGame;
		bool doIncrementAfterConfirm;
		Mac48Address myAddress;

		bool needCycleCheck;

		uint32_t rejectionCounter;

		Time lastParentUpdate;
		bool emptyPathOnConnect;
		uint32_t parentUnchangedCounter;
		Ptr<ParentPathCheckEvent> ppcEvent;

		Time finishTime;

		Ptr<EEBTPNode> parent;
		Ptr<EEBTPNode> contactedParent;

		uint64_t gameID;
		uint32_t unchangedCounter;

		double highestTxPower;
		double secondTxPower;
		double costOfCurrentConn;

		std::vector<Ptr<EEBTPNode>> neighbors;
		std::vector<Ptr<EEBTPNode>> childList;

		std::map<Mac48Address, std::map<uint8_t, uint16_t>> frameTypeCache;

		std::map<Mac48Address, Mac48Address> blacklist;
		std::vector<Ptr<EEBTPNode>> lastParents;

		Ptr<SendEvent> neighborDiscoveryEvent;

		bool locked;
		std::vector<Ptr<EEBTPNode>> locks;

		int childsLocked;
		bool parentIsWaitingForLock;
		bool newParentIsWaitingForLock;
		Ptr<EEBTPNode> lockOriginator;
		Ptr<EEBTPNode> newParentLockOriginator;
		Mac48Address lockedByNode;

		std::vector<Mac48Address> srcPath;

		Ptr<ApplicationDataHandler> adh;
	};
}
#endif /* BROADCAST_GAMESTATE_H_ */
