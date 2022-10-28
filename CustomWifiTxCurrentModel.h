/*
 * CustomWifiTxCurrentModel.h
 *
 *  Created on: 26.05.2020
 *      Author: krassus
 */

#ifndef BROADCAST_CUSTOMWIFITXCURRENTMODEL_H_
#define BROADCAST_CUSTOMWIFITXCURRENTMODEL_H_

#include "ns3/object.h"
#include "ns3/string.h"
#include "ns3/wifi-tx-current-model.h"

namespace ns3
{
	class CustomWifiTxCurrentModel : public WifiTxCurrentModel
	{
	public:
		static TypeId GetTypeId(void);

		CustomWifiTxCurrentModel();
		virtual ~CustomWifiTxCurrentModel();
		double CalcTxCurrent(double txPowerDbm) const;

	private:
		double eta;
		double voltage;
		double idleCurrent;
		std::string chip;

		double calcMax2831(double txPower) const;
	};
}

#endif /* BROADCAST_CUSTOMWIFITXCURRENTMODEL_H_ */
