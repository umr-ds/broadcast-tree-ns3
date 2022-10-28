/*
 * SimpleBroadcastProtocolHelper.cc
 *
 *  Created on: 07.05.2020
 *      Author: krassus
 */

#include "SimpleBroadcastProtocol.h"
#include "SimpleBroadcastProtocolHelper.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("SimpleBroadcastProtocolHelper");

	SimpleBroadcastProtocolHelper::SimpleBroadcastProtocolHelper()
	{
	}

	SimpleBroadcastProtocolHelper::~SimpleBroadcastProtocolHelper()
	{
	}

	void SimpleBroadcastProtocolHelper::Install(Ptr<WifiNetDevice> device) const
	{
		ObjectFactory factory;
		factory.SetTypeId("ns3::SimpleBroadcastProtocol");
		Ptr<Object> protocol = factory.Create<Object>();
		device->AggregateObject(protocol);

		Ptr<SimpleBroadcastProtocol> sbp = device->GetObject<SimpleBroadcastProtocol>();
		if (sbp != 0)
			sbp->Install(device);
		else
			NS_FATAL_ERROR("Failed to install SimpleBroadcastProtocol on node " << device->GetNode()->GetId());
	}

	void SimpleBroadcastProtocolHelper::Install(NetDeviceContainer c) const
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

	void SimpleBroadcastProtocolHelper::Install(std::string nodeName) const
	{
		Ptr<WifiNetDevice> node = Names::Find<WifiNetDevice>(nodeName);
		this->Install(node);
	}
}
