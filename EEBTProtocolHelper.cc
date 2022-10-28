/*
 * SimpleBroadcastProtocolHelper.cc
 *
 *  Created on: 07.05.2020
 *      Author: krassus
 */

#include "EEBTProtocol.h"
#include "EEBTProtocol_Mutex.h"
#include "EEBTProtocolHelper.h"
#include "EEBTProtocol_SrcPath.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("EEBTProtocolHelper");

	EEBTProtocolHelper::EEBTProtocolHelper()
	{
		this->cpm = CYCLE_TEST_ASYNC;
	}

	EEBTProtocolHelper::~EEBTProtocolHelper()
	{
	}

	void EEBTProtocolHelper::Install(Ptr<WifiNetDevice> device) const
	{
		ObjectFactory factory;

		switch (this->cpm)
		{
		case MUTEX:
			factory.SetTypeId("ns3::EEBTProtocolMutex");
			break;
		case PATH_TO_SRC:
			factory.SetTypeId("ns3::EEBTProtocolSrcPath");
			break;
		case CYCLE_TEST_ASYNC:
		default:
			factory.SetTypeId("ns3::EEBTPProtocol");
			break;
		}

		Ptr<Object> protocol = factory.Create<Object>();
		device->AggregateObject(protocol);

		Ptr<EEBTProtocol> eebtp = 0;
		switch (this->cpm)
		{
		case MUTEX:
			eebtp = device->GetObject<EEBTProtocolMutex>();
			break;
		case PATH_TO_SRC:
			eebtp = device->GetObject<EEBTProtocolSrcPath>();
			break;
		case CYCLE_TEST_ASYNC:
		default:
			eebtp = device->GetObject<EEBTProtocol>();
			break;
		}

		if (eebtp != 0)
			eebtp->Install(device, this->cwd);
		else
			NS_FATAL_ERROR("Failed to install EEBTPProtocol on node " << device->GetNode()->GetId());
	}

	void EEBTProtocolHelper::Install(NetDeviceContainer c) const
	{
		for (NetDeviceContainer::Iterator i = c.Begin(); i != c.End(); i++)
		{
			Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(*i);
			if (device != 0)
				this->Install(device);
			else
				NS_LOG_ERROR("Failed to cast NetDevice to WifiNetDevice");
		}
	}

	void EEBTProtocolHelper::Install(std::string nodeName) const
	{
		Ptr<WifiNetDevice> node = Names::Find<WifiNetDevice>(nodeName);
		this->Install(node);
	}

	/*
	 * Cycle prevention method
	 */
	CYCLE_PREV_METHOD EEBTProtocolHelper::getCyclePreventionMethod()
	{
		return this->cpm;
	}

	void EEBTProtocolHelper::setCyclePreventionMethod(CYCLE_PREV_METHOD cpm)
	{
		this->cpm = cpm;
	}

	void EEBTProtocolHelper::setCycleWatchDogCallback(Ptr<CycleWatchDog> cwd)
	{
		this->cwd = cwd;
	}
}
