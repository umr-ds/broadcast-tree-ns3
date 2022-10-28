/*
 * CycleWatchDog.cc
 *
 *  Created on: 25.08.2020
 *      Author: krassus
 */

#include "ns3/log.h"

#include "EEBTProtocol.h"
#include "CycleWatchDog.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("CycleWatchDog");
	NS_OBJECT_ENSURE_REGISTERED(CycleWatchDog);

	CycleWatchDog::CycleWatchDog()
	{
		this->uniqueCycles = 0;
	}

	CycleWatchDog::~CycleWatchDog()
	{
	}

	TypeId CycleWatchDog::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::CycleWatchDog")
								.SetParent<Object>()
								.AddConstructor<CycleWatchDog>();
		return tid;
	}

	TypeId CycleWatchDog::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	void CycleWatchDog::setNetDeviceContainer(NetDeviceContainer ndc)
	{
		this->ndc = ndc;
	}

	void CycleWatchDog::checkForCycles(uint64_t gid, Ptr<NetDevice> device)
	{
		Mac48Address currentParent = Mac48Address::GetBroadcast();
		Ptr<EEBTProtocol> proto = device->GetObject<EEBTProtocol>();
		Ptr<GameState> gs = proto->getGameState(gid);

		Ptr<CycleInfo> ci = Create<CycleInfo>();
		ci->setNodeId(device->GetNode()->GetId());

		//Loop until we do not find a node anymore
		while (proto != 0)
		{
			if (!ci->containsNode(proto->GetDevice()))
			{
				//Add the node to the list
				ci->addNode(proto->GetDevice());

				//Get the MAC address of the current nodes parent
				if (gs->getParent() != 0)
					currentParent = gs->getParent()->getAddress();
				else
					currentParent = Mac48Address::GetBroadcast();

				//Find the node with the MAC address of the current nodes parent
				proto = 0;
				Mac48Address child = gs->getMyAddress();
				for (NetDeviceContainer::Iterator i = this->ndc.Begin(); i != this->ndc.End(); i++)
				{
					if ((*i)->GetAddress() == currentParent)
					{
						proto = (*i)->GetObject<EEBTProtocol>();
						gs = proto->getGameState(gid);

						if (!gs->isChild(child))
							proto = 0;

						break;
					}
				}
			}
			else //If a node is already in the list, we completed the cycle
			{
				//If we are this node, we are in the cycle, else we would hang on a node that is in the cycle
				if (proto->GetDevice() == device)
				{
					ci->setStartTime(Now());

					NS_LOG_DEBUG("[Node " << device->GetNode()->GetId() << "]: Pushing cycle to nodes...");
					//Add this cycle to every node in the cycle
					for (Ptr<NetDevice> dev : ci->getNodes())
					{
						NS_LOG_DEBUG("\tPush to Node " << dev->GetNode()->GetId());
						this->cycles[gid][dev->GetNode()->GetId()].push_back(ci);
					}
					this->uniqueCycles++;

					//Add the node to the list
					ci->addNode(proto->GetDevice());

					ci->printPath();
				}
				break;
			}
		}
	}

	std::vector<Ptr<CycleInfo>> CycleWatchDog::getCycles(uint64_t gid, uint32_t nodeId)
	{
		return this->cycles[gid][nodeId];
	}

	uint32_t CycleWatchDog::getUniqueCycles()
	{
		return this->uniqueCycles;
	}

	NS_OBJECT_ENSURE_REGISTERED(CycleInfo);

	CycleInfo::CycleInfo()
	{
		this->nodeId = 0;
	}

	CycleInfo::~CycleInfo()
	{
	}

	TypeId CycleInfo::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::CycleInfo")
								.SetParent<Object>()
								.AddConstructor<CycleInfo>();
		return tid;
	}

	TypeId CycleInfo::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	void CycleInfo::Print(std::ostream &os) const
	{
		std::stringstream str;

		for (Ptr<NetDevice> dev : this->nodeList)
		{
			str << Mac48Address::ConvertFrom(dev->GetAddress());
			str << " => ";
		}
		std::string s = str.str();
		s.erase(s.end() - 4, s.end());

		os << s;
	}

	void CycleInfo::printPath()
	{
		std::stringstream str;

		for (Ptr<NetDevice> dev : this->nodeList)
		{
			str << Mac48Address::ConvertFrom(dev->GetAddress());
			str << " => ";
		}
		std::string s = str.str();
		s.erase(s.end() - 4, s.end());

		NS_LOG_DEBUG("[Node " << this->nodeList[0]->GetNode()->GetId() << "]: Cycle: " << s);
	}

	uint32_t CycleInfo::getNodeId()
	{
		return this->nodeId;
	}

	void CycleInfo::setNodeId(uint32_t id)
	{
		this->nodeId = id;
	}

	bool CycleInfo::isRealCycle()
	{
		return (*this->nodeList.begin()) == (*(this->nodeList.end() - 1));
	}

	void CycleInfo::addNode(Ptr<NetDevice> dev)
	{
		this->nodeList.push_back(dev);
	}

	bool CycleInfo::containsNode(Ptr<NetDevice> dev)
	{
		for (Ptr<NetDevice> d : this->nodeList)
			if (d == dev)
				return true;
		return false;
	}

	std::vector<Ptr<NetDevice>> CycleInfo::getNodes()
	{
		return this->nodeList;
	}

	/*
	 * Time
	 */
	Time CycleInfo::getStartTime()
	{
		return this->start;
	}

	void CycleInfo::setStartTime(Time t)
	{
		this->start = t;
	}

	Time CycleInfo::getEndTime()
	{
		return this->end;
	}

	void CycleInfo::setEndTime(Time t)
	{
		this->end = t;
	}

	Time CycleInfo::getDuration()
	{
		Time t = (this->end - this->start);
		if (t.GetDouble() < 0)
			return (Now() - this->start);
		return t;
	}
}
