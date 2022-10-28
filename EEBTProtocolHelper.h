/*
 * SimpleBroadcastProtocolHelper.h
 *
 *  Created on: 07.05.2020
 *      Author: krassus
 */

#ifndef BROADCAST_EEBTPROTOCOLHELPER_H_
#define BROADCAST_EEBTPROTOCOLHELPER_H_

#include "ns3/ptr.h"
#include "ns3/callback.h"
#include "CycleWatchDog.h"
#include "ns3/network-module.h"
#include "ns3/node-container.h"
#include "ns3/wifi-net-device.h"

namespace ns3
{
	typedef enum
	{
		CYCLE_TEST_ASYNC,
		MUTEX,
		PATH_TO_SRC
	} CYCLE_PREV_METHOD;

	class EEBTProtocolHelper
	{
	public:
		EEBTProtocolHelper();
		virtual ~EEBTProtocolHelper();

		void Install(std::string nodeName) const;
		void Install(Ptr<WifiNetDevice> device) const;
		void Install(NetDeviceContainer c) const;

		CYCLE_PREV_METHOD getCyclePreventionMethod();
		void setCyclePreventionMethod(CYCLE_PREV_METHOD cpm);

		void setCycleWatchDogCallback(Ptr<CycleWatchDog> cwd);

	private:
		CYCLE_PREV_METHOD cpm;
		Ptr<CycleWatchDog> cwd;
	};
}

#endif /* BROADCAST_EEBTPROTOCOLHELPER_H_ */
