/*
 * CustomWifiTxCurrentModel.cc
 *
 *  Created on: 26.05.2020
 *      Author: krassus
 */

#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "CustomWifiTxCurrentModel.h"
#include "ns3/wifi-utils.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("CustomWifiTxCurrentModel");

	NS_OBJECT_ENSURE_REGISTERED(CustomWifiTxCurrentModel);

	/*
	 * TypeId getter was taken from LinearWifiTxCurrentModel.cc
	 */
	TypeId CustomWifiTxCurrentModel::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::CustomWifiTxCurrentModel")
								.SetParent<WifiTxCurrentModel>()
								.SetGroupName("Wifi")
								.AddConstructor<CustomWifiTxCurrentModel>()
								.AddAttribute("Eta", "The efficiency of the power amplifier.",
											  DoubleValue(0.10),
											  MakeDoubleAccessor(&CustomWifiTxCurrentModel::eta),
											  MakeDoubleChecker<double>())
								.AddAttribute("Voltage", "The supply voltage (in Volts).",
											  DoubleValue(3.0),
											  MakeDoubleAccessor(&CustomWifiTxCurrentModel::voltage),
											  MakeDoubleChecker<double>())
								.AddAttribute("IdleCurrent", "The current in the IDLE state (in Ampere).",
											  DoubleValue(0.273333),
											  MakeDoubleAccessor(&CustomWifiTxCurrentModel::idleCurrent),
											  MakeDoubleChecker<double>())
								.AddAttribute("Chip", "The chip model to simulate",
											  StringValue("MAX2831"),
											  MakeStringAccessor(&CustomWifiTxCurrentModel::chip),
											  MakeStringChecker());
		return tid;
	}

	CustomWifiTxCurrentModel::CustomWifiTxCurrentModel()
	{
		NS_LOG_FUNCTION(this);
	}

	CustomWifiTxCurrentModel::~CustomWifiTxCurrentModel()
	{
		NS_LOG_FUNCTION(this);
	}

	double CustomWifiTxCurrentModel::CalcTxCurrent(double txPowerDbm) const
	{
		NS_LOG_FUNCTION(this << txPowerDbm);
		double current = 0;

		if (this->chip == "MAX2831") {
			current = calcMax2831(txPowerDbm);
		} else {
			current = DbmToW(txPowerDbm) / (this->voltage * this->eta);
		}

		return (current + this->idleCurrent);
	}

	/*
	* The formula used in this function to calculate the current TX power in watts
	* is taken from the bachelor thesis of Sergio Domínguez.
	*/
	double CustomWifiTxCurrentModel::calcMax2831(double txPowerDbm) const
	{
		//0.0007*pow(txPowerDbm,4) - 0.0111*pow(txPowerDbm,3) + 0.0889*pow(txPowerDbm,2) + 0.3483*txPowerDbm + 134.91;
		double current = 0;
		double txPow = txPowerDbm;

		if (txPowerDbm >= 0)
		{
			current = 134.91 + 0.3483 * txPowerDbm;

			txPow *= txPowerDbm; //txPowerDbm^2
			current += (0.0889 * txPow);

			txPow *= txPowerDbm; //txPowerDbm^3
			current += (-0.0111 * txPow);

			txPow *= txPowerDbm; //txPowerDbm^4
			current += (0.0007 * txPow);
		}
		else
		{
			current = 124.4196777 - 2.427503769 * txPow;

			txPow *= txPowerDbm;
			current += (-0.4798606017 * txPow);

			txPow *= txPowerDbm;
			current += (-0.03112035853 * txPow);

			txPow *= txPowerDbm;
			current += (-0.00089877372 * txPow);

			txPow *= txPowerDbm;
			current += (-0.000009708995023 * txPow);
		}

		current = current / 1000;

		NS_LOG_UNCOND("dBm = " << txPowerDbm << ", Current = " << current << ", volt = " << this->voltage);

        return current;
	}
}
