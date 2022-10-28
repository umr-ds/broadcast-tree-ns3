/*
 *	CustomThresholdPreambleDetectionModel.cc
 *
 *  Created on: 28.08.2020
 *      Author: Kevin Küchler
 *
 *  This class has been adopted from threashold-preamble-detection-model.cc
 *  Only the min RSSI value has been changed from -82 dBm to -100 dBm since this is a more realistic value
 */

#include "CustomThresholdPreambleDetectionModel.h"

#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/wifi-utils.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("CustomThresholdPreambleDetectionModel");
	NS_OBJECT_ENSURE_REGISTERED(CustomThresholdPreambleDetectionModel);

	CustomThresholdPreambleDetectionModel::CustomThresholdPreambleDetectionModel()
	{
		this->minSNR = 4;
		this->minRssi = -100;
	}

	CustomThresholdPreambleDetectionModel::~CustomThresholdPreambleDetectionModel()
	{
	}

	TypeId CustomThresholdPreambleDetectionModel::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::CustomThresholdPreambleDetectionModel")
								.SetParent<PreambleDetectionModel>()
								.SetGroupName("Wifi")
								.AddConstructor<CustomThresholdPreambleDetectionModel>()
								.AddAttribute("MinimumSNR",
											  "Minimum SNR (dB) to successfully detect the preamble.",
											  DoubleValue(4),
											  MakeDoubleAccessor(&CustomThresholdPreambleDetectionModel::minSNR),
											  MakeDoubleChecker<double>())
								.AddAttribute("MinimumRssi",
											  "Minimum RSSI (dBm) to successfully detect the signal.",
											  DoubleValue(-100),
											  MakeDoubleAccessor(&CustomThresholdPreambleDetectionModel::minRssi),
											  MakeDoubleChecker<double>());
		return tid;
	}

	bool CustomThresholdPreambleDetectionModel::IsPreambleDetected(double rssi, double snr, double channelWidth) const
	{
		if (WToDbm(rssi) >= this->minRssi)
		{
			if (RatioToDb(snr) >= this->minSNR)
			{
				return true;
			}
			else
			{
				NS_LOG_DEBUG("Received RSSI is above the target RSSI but SNR is too low");
				return false;
			}
		}
		else
		{
			NS_LOG_DEBUG("Received RSSI is below the target RSSI");
			return false;
		}
	}
}
