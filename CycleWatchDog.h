/*
 * CycleWatchDog.h
 *
 *  Created on: 25.08.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_CYCLEWATCHDOG_H_
#define BROADCAST_CYCLEWATCHDOG_H_

#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"

namespace ns3
{
	class CycleInfo : public Object
	{
	public:
		CycleInfo();
		virtual ~CycleInfo();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;

		void printPath();
		virtual void Print(std::ostream &os) const;

		uint32_t getNodeId();
		void setNodeId(uint32_t id);

		bool isRealCycle();

		void addNode(Ptr<NetDevice> dev);
		bool containsNode(Ptr<NetDevice> dev);
		std::vector<Ptr<NetDevice>> getNodes();

		Time getStartTime();
		void setStartTime(Time t);

		Time getEndTime();
		void setEndTime(Time t);

		Time getDuration();

	private:
		uint32_t nodeId;
		std::vector<Ptr<NetDevice>> nodeList;

		Time start;
		Time end;
		Time duration;
	};

	class CycleWatchDog : public Object
	{
	public:
		CycleWatchDog();
		virtual ~CycleWatchDog();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;

		void setNetDeviceContainer(NetDeviceContainer ndc);

		void checkForCycles(uint64_t gid, Ptr<NetDevice> device);
		std::vector<Ptr<CycleInfo>> getCycles(uint64_t gid, uint32_t nodeId);

		uint32_t getUniqueCycles();

	private:
		uint32_t uniqueCycles;
		NetDeviceContainer ndc;
		std::map<uint64_t, std::map<uint32_t, std::vector<Ptr<CycleInfo>>>> cycles;
	};
}

#endif /* BROADCAST_CYCLEWATCHDOG_H_ */
