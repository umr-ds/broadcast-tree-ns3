/*
 * GameState.cc
 *
 *  Created on: 03.06.2020
 *      Author: Kevin Küchler
 *
 *  The GameState class hold all information about the current game state
 *
 *  bool initiator				Defines if the current node is the initiator
 *  							of this game or not.
 *
 *  bool endOfGame				Defines if the game has ended for this node.
 *  							The game end either if the unchangedCounter
 *  							hits a certain threshold N or if all child
 *  							nodes have sent an EndOfGame frame to this
 *  							node.
 *
 *  bool discovery				Defines if this node has already done a
 *  							neighbor discovery or not. Every node should
 *  							at least do one neighbor discovery to tell
 *  							nearby nodes it is here
 *
 *  uint64_t gameID				The gameID is used to identify the current
 *  							running game. Thus is it possible to construct
 *  							multiple broadcast trees within the same
 *  							network without colliding with other games.
 *  							The gameID field in the protocol header is
 *  							just 48 bit large but is held internally in
 *  							a 64 bit integer.
 *
 *  uint32_t unchangedCounter	This counter is incremented when no changes
 *  							have been made after receiving a packet. Even
 *  							if the node changed its parent and the cost
 *  							of the connection is the same as before.
 *
 *  double maxTxPower			The maximum transmission power of a nodes
 *  							refers to the current maximum transmission
 *  							power in dBm to reach all child nodes that
 *  							are registered at this node.
 *
 *  double costOfCurrentConn	The cost of current connection determines
 *  							how much transmission power is needed to
 *  							reach the parent node of this node. The
 *  							transmitter will only send with this TX
 *  							power set, if this nodes want to reach its
 *  							parent node.
 *
 * vector childList				The child list hold all child nodes that are
 * 								registered at this node. The child list is
 * 								always a subset of the neighbor list. It is
 * 								used to send the cycle test packet and later
 * 								the application data packets.
 *
 * vector neighbors				The neighbor list contains all other nodes
 * 								this node has ever received a packet from.
 * 								It is used to find possible (new) parents.
 */

#include "float.h"
#include "ns3/integer.h"
#include "GameState.h"
#include "ns3/core-module.h"
#include "ns3/wifi-utils.h"

#include "SendEvent.h"
#include "LockEvent.h"
#include "ParentPathCheckEvent.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("GameState");

	/*
	 * Implementation EEBTPNode
	 */
	EEBTPNode::EEBTPNode(Mac48Address addr, double power)
	{
		this->address = addr;
		this->finished = false;

		this->highest_maxTxPower = power;
		this->second_maxTxPower = power;

		this->noise = 0;
		this->rxPower = 0;
		this->reachPower = FLT_MAX;

		this->connCounter = 0;

		this->rpChanged = false;
		this->pChanged = false;
		this->hasRpProblem = false;
	}

	EEBTPNode::~EEBTPNode()
	{
	}

	double EEBTPNode::getHighestMaxTxPower()
	{
		return this->highest_maxTxPower;
	}

	void EEBTPNode::setHighestMaxTxPower(double maxTxPower)
	{
		this->highest_maxTxPower = maxTxPower;
	}

	double EEBTPNode::getSecondHighestMaxTxPower()
	{
		return this->second_maxTxPower;
	}

	void EEBTPNode::setSecondHighestMaxTxPower(double maxTxPower)
	{
		this->second_maxTxPower = maxTxPower;
	}

	double EEBTPNode::getReachPower()
	{
		return this->reachPower;
	}

	void EEBTPNode::setReachPower(double reachPower)
	{
		if (this->reachPower < reachPower + 0.0000001 && this->reachPower > reachPower - 0.0000001)
			this->rpChanged = true;
		this->reachPower = reachPower;
	}

	double EEBTPNode::getNoise()
	{
		return this->noise;
	}

	void EEBTPNode::updateRxInfo(double rxPower, double noise)
	{
		this->noise = noise;
		this->rxPower = rxPower;
	}

	bool EEBTPNode::hasReachPowerProblem()
	{
		return this->hasRpProblem;
	}

	void EEBTPNode::hasReachPowerProblem(bool b)
	{
		this->hasRpProblem = b;
	}

	Mac48Address EEBTPNode::getAddress()
	{
		return this->address;
	}

	Mac48Address EEBTPNode::getParentAddress()
	{
		return this->parent;
	}

	void EEBTPNode::setParentAddress(Mac48Address addr)
	{
		this->parent = addr;
	}

	bool EEBTPNode::hasFinished()
	{
		return this->finished;
	}

	void EEBTPNode::setFinished(bool f)
	{
		this->finished = f;
	}

	bool EEBTPNode::isOnPath(Mac48Address addr)
	{
		for (uint i = 0; i < this->srcPath.size(); i++)
		{
			Mac48Address node = this->srcPath[i];
			if (node == addr)
				return true;
		}
		return false;
	}

	std::vector<Mac48Address> EEBTPNode::getSrcPath()
	{
		return this->srcPath;
	}

	void EEBTPNode::setSrcPath(std::vector<Mac48Address> path)
	{
		this->pChanged = false;
		if (path.size() == this->srcPath.size())
		{
			for (uint i = 0; i < path.size(); i++)
				if (path[i] != this->srcPath[i])
				{
					this->pChanged = true;
					break;
				}
		}
		else
			this->pChanged = true;

		this->srcPath.clear();
		for (uint i = 0; i < path.size(); i++)
		{
			this->srcPath.push_back(path[i]);
		}
	}

	bool EEBTPNode::reachPowerChanged()
	{
		return this->rpChanged;
	}

	void EEBTPNode::resetReachPowerChanged()
	{
		this->rpChanged = false;
	}

	bool EEBTPNode::pathChanged()
	{
		return this->pChanged;
	}

	void EEBTPNode::resetPathChanged()
	{
		this->pChanged = false;
	}

	uint32_t EEBTPNode::getConnCounter()
	{
		return this->connCounter;
	}

	void EEBTPNode::incrementConnCounter()
	{
		this->connCounter++;
	}

	void EEBTPNode::resetConnCounter()
	{
		this->connCounter = 0;
	}

	/*
	 * Implementation GameState
	 */
	GameState::GameState() : GameState(false, 0)
	{
	}

	GameState::GameState(bool initiator, uint64_t gid)
	{
		this->endOfGame = false;
		this->initiator = initiator;
		this->doIncrementAfterConfirm = false;

		this->rejectionCounter = 0;
		this->parentUnchangedCounter = 0;

		this->needCycleCheck = false;

		this->gameID = gid;
		this->unchangedCounter = 0;

		this->highestTxPower = WToDbm(0);
		this->secondTxPower = WToDbm(0);
		this->costOfCurrentConn = FLT_MAX;

		this->locked = false;
		this->childsLocked = 0;
		this->parentIsWaitingForLock = false;
		this->newParentIsWaitingForLock = false;
		this->lockedByNode = Mac48Address::GetBroadcast();

		this->emptyPathOnConnect = false;

		this->adh = Create<ApplicationDataHandler>();
	}

	GameState::~GameState()
	{
		this->blacklist.clear();
		this->childList.clear();
		this->frameTypeCache.clear();
		this->locks.clear();
		this->neighbors.clear();
		this->srcPath.clear();

		this->adh = 0;
		this->contactedParent = 0;
		this->lockOriginator = 0;
		this->neighborDiscoveryEvent = 0;
		this->newParentLockOriginator = 0;
		this->parent = 0;
		this->ppcEvent = 0;
	}

	TypeId GameState::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::GameState")
								.SetParent<Object>()
								.AddConstructor<GameState>()
								.AddAttribute("initiator", "Indicates if the node with this state is the initiator of the game",
											  BooleanValue(false),
											  MakeBooleanAccessor(&GameState::initiator),
											  MakeBooleanChecker())
								.AddAttribute("gameID", "The gameID this packet corresponds to",
											  IntegerValue(0),
											  MakeIntegerAccessor(&GameState::gameID),
											  MakeIntegerChecker<int>());
		return tid;
	}

	TypeId GameState::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	//Returns true if this node is the initiator of this game
	bool GameState::isInitiator()
	{
		return this->initiator;
	}

	//Return the game ID of the current game
	uint64_t GameState::getGameID()
	{
		return this->gameID;
	}

	bool GameState::needsCycleCheck()
	{
		return this->needCycleCheck;
	}

	void GameState::setCycleCheckNeeded(bool cc)
	{
		this->needCycleCheck = cc;
	}

	Mac48Address GameState::getMyAddress()
	{
		return this->myAddress;
	}

	void GameState::setMyAddress(Mac48Address addr)
	{
		this->myAddress = addr;
	}

	/*
	 * Finish game
	 * 	- Finish the game of a node and saves the time
	 * 	- Checks if the game already finished
	 * 	- Get the time when the game was finished
	 */
	void GameState::finishGame()
	{
		this->endOfGame = true;
		this->finishTime = Now();
	}

	bool GameState::gameFinished()
	{
		return this->endOfGame;
	}

	Time GameState::getTimeFinished()
	{
		return this->finishTime;
	}

	/*
	 * Sequence numbers
	 * 	- Get the last seen sequence number of a sender and a specific frame type
	 * 	- Check whether the given sequence number is higher then the last seen
	 * 		sequence number of a sender and a specific frame type
	 * 	- Update the cache for a sender and a specific frame type with a newer sequence number
	 */
	uint16_t GameState::getLastSeqNo(Mac48Address sender, uint8_t ft)
	{
		return this->frameTypeCache[sender][ft];
	}

	bool GameState::checkLastFrameType(Mac48Address sender, uint8_t ft, uint16_t seqNo)
	{
		if (this->frameTypeCache[sender][ft] < seqNo)
			return true;
		return false;
	}

	void GameState::updateLastFrameType(Mac48Address sender, uint8_t ft, uint16_t seqNo)
	{
		this->frameTypeCache[sender][ft] = seqNo;
	}

	/*
	 * Unchanged counter - Counts the rounds without any changes
	 *	- Get the current counter
	 *	- Increment the counter by one
	 *	- Reset the counter to zero
	 */
	uint32_t GameState::getUnchangedCounter()
	{
		return this->unchangedCounter;
	}

	void GameState::incrementUnchangedCounter()
	{
		if (!this->locked)
		{
			this->unchangedCounter++;
			NS_LOG_DEBUG("\tUnchanged counter incremented: " << this->unchangedCounter);
		}
	}

	void GameState::resetUnchangedCounter()
	{
		this->unchangedCounter = 0;
		NS_LOG_DEBUG("\tUnchanged counter reset");
	}

	/*
	 * Parent handling
	 * 	- Get the current parent
	 * 	- Set a new parent
	 * 	- Get the node we want to connect to (contactedNode)
	 * 	- Set the node we want to connect to
	 */
	Ptr<EEBTPNode> GameState::getParent()
	{
		return this->parent;
	}

	void GameState::setParent(Ptr<EEBTPNode> p)
	{
		//For mutex mode: If we changed our parent (p != this->parent), our old parent is not waiting for us anymore
		if (p != this->parent)
			this->parentIsWaitingForLock = false;
		this->parent = p;
	}

	Ptr<EEBTPNode> GameState::getContactedParent()
	{
		return this->contactedParent;
	}

	void GameState::setContactedParent(Ptr<EEBTPNode> p)
	{
		this->contactedParent = p;
	}

	/*
	 * LastParentStack
	 */
	bool GameState::hasLastParents()
	{
		return !this->lastParents.empty();
	}

	Ptr<EEBTPNode> GameState::popLastParent()
	{
		if (!this->lastParents.empty())
		{
			Ptr<EEBTPNode> node = *(this->lastParents.end() - 1);
			node->resetConnCounter();
			this->lastParents.erase(this->lastParents.end() - 1);
			return node;
		}
		else
			return 0;
	}

	Ptr<EEBTPNode> GameState::getLastParent()
	{
		if (!this->lastParents.empty())
			return *(this->lastParents.end() - 1);
		else
			return 0;
	}

	void GameState::pushLastParent(Ptr<EEBTPNode> lastParent)
	{
		for (std::vector<Ptr<EEBTPNode>>::iterator it = this->lastParents.begin(); it != this->lastParents.end(); it++)
		{
			if ((*it) == lastParent || (*it)->getAddress() == lastParent->getAddress())
			{
				(*it)->incrementConnCounter();
				this->lastParents.erase(it);
				break;
			}
		}
		this->lastParents.push_back(lastParent);
	}

	void GameState::clearLastParents()
	{
		for (Ptr<EEBTPNode> node : this->lastParents)
			node->resetConnCounter();
		this->lastParents.clear();
	}

	/*
	 * Cost of current connection
	 */
	double GameState::getCostOfCurrentConn()
	{
		if (this->parent != 0)
		{
			//Difference between the maxTxPower of our parent and the reach power to our parent
			//If the difference is 0, we are (one of) the nodes that are the farthest away
			return (DbmToW(this->parent->getHighestMaxTxPower()) - DbmToW(this->parent->getReachPower()));
		}
		return 0;
	}

	/*
	 * Calculated maximum transmission power
	 */
	double GameState::getHighestTxPower()
	{
		return this->highestTxPower;
	}

	double GameState::getSecondHighestTxPower()
	{
		return this->secondTxPower;
	}

	void GameState::findHighestTxPowers()
	{
		double maxTx = WToDbm(0);
		double sMaxTx = WToDbm(0);
		for (uint i = 0; i < this->childList.size(); i++)
		{
			Ptr<EEBTPNode> child = this->childList[i];
			double reachPower = child->getReachPower();

			//If this child has a higher reach power than the last highest one
			if (reachPower > maxTx)
			{
				sMaxTx = maxTx;
				maxTx = reachPower;
			}
			//else if we already have a highest child but this child is higher than our second highest but lower than our highest child
			else if (sMaxTx < maxTx && reachPower > sMaxTx && reachPower < maxTx)
				sMaxTx = reachPower;
		}

		this->highestTxPower = maxTx;
		this->secondTxPower = sMaxTx;
	}

	/*
	 * NeighborList
	 */
	bool GameState::isNeighbor(Mac48Address n)
	{
		for (Ptr<EEBTPNode> p : this->neighbors)
		{
			if (p->getAddress() == n)
				return true;
		}
		return false;
	}

	void GameState::addNeighbor(Mac48Address n)
	{
		this->neighbors.push_back(Create<EEBTPNode>(n, FLT_MIN));
	}

	Ptr<EEBTPNode> GameState::getNeighbor(Mac48Address n)
	{
		for (uint i = 0; i < this->neighbors.size(); i++)
		{
			Ptr<EEBTPNode> node = this->neighbors[i];
			if (node->getAddress() == n)
				return node;
		}
		return 0;
	}

	Ptr<EEBTPNode> GameState::getNeighbor(uint32_t index)
	{
		if (index < this->neighbors.size())
			return this->neighbors[index];
		else
			return 0;
	}

	void GameState::removeNeighbor(Ptr<EEBTPNode> n)
	{
		uint i = 0;
		for (; i < this->neighbors.size(); i++)
		{
			if (this->neighbors[i] == n)
				break;
		}

		if (i < this->neighbors.size())
			neighbors.erase(this->neighbors.begin() + i, this->neighbors.begin() + (i + 1));
	}

	uint32_t GameState::getNNeighbors()
	{
		return this->neighbors.size();
	}

	/*
	 * Searches in the local maintained neighbor list for the
	 * neighbor with the lowest cost of new connection who
	 * is NOT blacklisted.
	 */
	Ptr<EEBTPNode> GameState::getCheapestNeighbor()
	{
		double cost = FLT_MAX;
		double threshold = DbmToW(1.0);
		Ptr<EEBTPNode> neighbor = this->parent;

		//Cost of current connection (connCost) is 0 if we are (one of) the nodes that are the farthest away
		double connCost = this->getCostOfCurrentConn();

		//Only if we are (one of) the nodes that are the farthest away, it is useful to switch (else no savings)
		if (connCost <= 0.0001 && connCost >= -0.0001)
		{
			//If we have a parent and we are one of the nodes that are the farthest away, our cost are the energy saved by leaving our parent
			if (this->parent != 0)
				cost = DbmToW(this->parent->getHighestMaxTxPower()) - DbmToW(this->parent->getSecondHighestMaxTxPower());
			NS_LOG_DEBUG("\tCurrent connection cost: " << WToDbm(cost));

			for (uint i = 0; i < this->neighbors.size(); i++)
			{
				Ptr<EEBTPNode> node = this->neighbors[i];
				//TODO: Find better way to submit maxAllowedTxPower
				if (this->isBlacklisted(node->getAddress(), node->getParentAddress()))
				{
					NS_LOG_DEBUG("\t[" << node->getAddress() << "] is blacklisted!");
				}
				else if (this->isChild(node))
				{
					NS_LOG_DEBUG("\t[" << node->getAddress() << "] is a child of mine");
				}
				else if (node->getReachPower() > 23.0)
				{
					NS_LOG_DEBUG("\t[" << node->getAddress() << "] is out of my range: rP = " << node->getReachPower() << "dBm, noise = " << node->getNoise() << "dBm");
				}
				else if (node == this->parent)
				{
					NS_LOG_DEBUG("\t[" << node->getAddress() << "] is my parent");
				}
				else if (node->isOnPath(this->myAddress))
				{
					NS_LOG_DEBUG("\t[" << node->getAddress() << "] uses me to reach the source node");
				}
				else if (node->getConnCounter() > 5)
				{
					NS_LOG_DEBUG("\t[" << node->getAddress() << "]'s connection counter exceeds the maximum");
				}
				else
				{
					//Cost of the new connection is the difference between the node's highest tx power and the reach power to this node
					double costOfNewConn = DbmToW(node->getReachPower()) - DbmToW(node->getHighestMaxTxPower());
					costOfNewConn += threshold;

					NS_LOG_DEBUG("\t[" << node->getAddress() << "] => " << WToDbm(costOfNewConn) << "dBm");

					if (costOfNewConn <= cost)
					{
						neighbor = node;
						cost = costOfNewConn;
					}
				}
			}
		}

		return neighbor;
	}

	/*
	 * ChildList
	 */
	bool GameState::hasChilds()
	{
		return this->childList.size() != 0;
	}

	bool GameState::isChild(Mac48Address c)
	{
		for (Ptr<EEBTPNode> n : this->childList)
			if (c == n->getAddress())
				return true;
		return false;
	}

	bool GameState::isChild(Ptr<EEBTPNode> c)
	{
		for (Ptr<EEBTPNode> n : this->childList)
			if (c == n)
				return true;
		return false;
	}

	void GameState::addChild(Ptr<EEBTPNode> c)
	{
		if (!this->isChild(c))
			this->childList.push_back(c);
		this->findHighestTxPowers();
	}

	void GameState::removeChild(Ptr<EEBTPNode> c)
	{
		uint i = 0;
		for (; i < this->childList.size(); i++)
		{
			if (this->childList[i] == c)
				break;
		}

		if (i < this->childList.size())
			childList.erase(this->childList.begin() + i, this->childList.begin() + (i + 1));

		this->findHighestTxPowers();
	}

	int GameState::getNChilds()
	{
		return this->childList.size();
	}

	Ptr<EEBTPNode> GameState::getChild(int index)
	{
		return this->childList[index];
	}

	bool GameState::allChildsFinished()
	{
		bool allFinished = true;
		for (uint i = 0; i < this->childList.size(); i++)
		{
			Ptr<EEBTPNode> child = this->childList[i];
			if (!child->hasFinished())
			{
				allFinished = false;
				break;
			}
		}
		return allFinished;
	}

	/*
	 * Blacklist management
	 */
	bool GameState::isBlacklisted(Mac48Address node, Mac48Address parent)
	{
		return (this->blacklist[node] == parent) && parent != Mac48Address("00:00:00:00:00:00");
	}

	bool GameState::isBlacklisted(Ptr<EEBTPNode> node)
	{
		return this->isBlacklisted(node->getAddress(), node->getParentAddress());
	}

	void GameState::updateBlacklist(Ptr<EEBTPNode> node)
	{
		this->blacklist[node->getAddress()] = node->getParentAddress();
	}

	void GameState::resetBlacklist()
	{
		this->blacklist.clear();

		for (Ptr<EEBTPNode> node : this->neighbors)
			node->resetConnCounter();
	}

	/*
	 * This flag is used to increment the unchanged counter
	 * if we switched the parent but the actual needed transmission
	 * power remains unchanged
	 */
	bool GameState::doIncrAfterConfirm()
	{
		return this->doIncrementAfterConfirm;
	}

	void GameState::setDoIncrAfterConfirm(bool v)
	{
		this->doIncrementAfterConfirm = v;
	}

	/*
	 * Events
	 */
	Ptr<SendEvent> GameState::getNeighborDiscoveryEvent()
	{
		return this->neighborDiscoveryEvent;
	}

	void GameState::setNeighborDiscoveryEvent(Ptr<SendEvent> evt)
	{
		this->resetNeighborDiscoveryEvent();
		this->neighborDiscoveryEvent = evt;
	}

	void GameState::resetNeighborDiscoveryEvent()
	{
		if (this->neighborDiscoveryEvent != 0)
			this->neighborDiscoveryEvent->Cancel();
		this->neighborDiscoveryEvent = 0;
	}

	/*
	 * Mutex
	 */
	bool GameState::isLocked()
	{
		return this->locked;
	}

	bool GameState::isLocked(Ptr<EEBTPNode> node)
	{
		for (std::vector<Ptr<EEBTPNode>>::iterator it = this->locks.begin(); it != this->locks.end(); ++it)
		{
			if ((*it)->getAddress() == node->getAddress())
				return true;
		}
		return false;
	}

	void GameState::lock(bool l)
	{
		this->locked = l;
	}

	void GameState::lock(Ptr<EEBTPNode> node, bool l)
	{
		if (l)
		{
			if (!this->isLocked(node))
				this->locks.push_back(node);
		}
		else
		{
			uint i = 0;
			for (std::vector<Ptr<EEBTPNode>>::iterator it = this->locks.begin(); it != this->locks.end(); ++it)
			{
				if ((*it)->getAddress() == node->getAddress())
					break;
				i++;
			}

			if (i < this->locks.size())
				this->locks.erase(this->locks.begin() + i);
		}
	}

	int GameState::getNChildsLocked()
	{
		return this->locks.size();
	}

	void GameState::resetChildLocks()
	{
		this->locks.clear();
	}

	Mac48Address GameState::getLockedBy()
	{
		return this->lockedByNode;
	}

	void GameState::setLockedBy(Mac48Address addr)
	{
		this->lockedByNode = addr;
	}

	bool GameState::isParentWaitingForLock()
	{
		return this->parentIsWaitingForLock;
	}

	void GameState::setParentWaitingForLock(bool b)
	{
		this->parentIsWaitingForLock = b;
	}

	Ptr<EEBTPNode> GameState::getParentWaitingLockOriginator()
	{
		return this->lockOriginator;
	}

	void GameState::setParentWaitingLockOriginator(Ptr<EEBTPNode> originator)
	{
		this->lockOriginator = originator;
	}

	bool GameState::isNewParentWaitingForLock()
	{
		return this->newParentIsWaitingForLock;
	}

	void GameState::setNewParentWaitingForLock(bool b)
	{
		this->newParentIsWaitingForLock = b;
	}

	Ptr<EEBTPNode> GameState::getNewParentWaitingLockOriginator()
	{
		return this->newParentLockOriginator;
	}

	void GameState::setNewParentWaitingLockOriginator(Ptr<EEBTPNode> originator)
	{
		this->newParentLockOriginator = originator;
	}

	/*
	 * Application data handler
	 */
	Ptr<ApplicationDataHandler> GameState::getApplicationDataHandler()
	{
		return this->adh;
	}

	uint32_t GameState::getRejectionCounter()
	{
		return this->rejectionCounter;
	}

	void GameState::setRejectionCounter(uint32_t rj)
	{
		this->rejectionCounter = rj;
	}

	void GameState::incrementRejectionCounter()
	{
		this->rejectionCounter++;
	}

	void GameState::resetRejectionCounter()
	{
		this->rejectionCounter = 0;
	}

	/*
	 * Path-To-Source
	 */
	Time GameState::getLastParentUpdate()
	{
		return this->lastParentUpdate;
	}

	void GameState::setLastParentUpdate(Time t)
	{
		this->lastParentUpdate = t;
	}

	uint32_t GameState::getParentUnchangedCounter()
	{
		return this->parentUnchangedCounter;
	}

	void GameState::resetParentUnchangedCounter()
	{
		this->parentUnchangedCounter = 0;
	}

	void GameState::incrementParentUnchangedCounter()
	{
		this->parentUnchangedCounter++;
	}

	Ptr<ParentPathCheckEvent> GameState::getPPCEvent()
	{
		return this->ppcEvent;
	}

	void GameState::setPPCEvent(Ptr<ParentPathCheckEvent> e)
	{
		this->ppcEvent = e;
	}

	bool GameState::hadEmptyPathOnConnect()
	{
		return this->emptyPathOnConnect;
	}

	void GameState::setEmptyPathOnConnect(bool b)
	{
		this->emptyPathOnConnect = b;
	}
}
