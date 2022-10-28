/*
 * SimpleBroadcastProtocolHelper.h
 *
 *  Created on: 07.05.2020
 *      Author: krassus
 */

#ifndef BROADCAST_SIMPLEBROADCASTPROTOCOLHELPER_H_
#define BROADCAST_SIMPLEBROADCASTPROTOCOLHELPER_H_

#include "ns3/names.h"
#include "ns3/network-module.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"

namespace ns3
{
	class SimpleBroadcastProtocolHelper
	{
	public:
		SimpleBroadcastProtocolHelper();
		virtual ~SimpleBroadcastProtocolHelper();

		void Install(std::string nodeName) const;
		void Install(Ptr<WifiNetDevice> device) const;
		void Install(NetDeviceContainer c) const;
	};
}

#endif /* BROADCAST_SIMPLEBROADCASTPROTOCOLHELPER_H_ */
