/*
 *	CustomThresholdPreambleDetectionModel.h
 *
 *  Created on: 28.08.2020
 *      Author: Kevin Küchler
 *
 *  This class has been adopted from threashold-preamble-detection-model.h
 */

#ifndef BROADCAST_CUSTOM_THRESHOLD_PREAMBLE_DETECTION_MODEL_H
#define BROADCAST_CUSTOM_THRESHOLD_PREAMBLE_DETECTION_MODEL_H

#include "ns3/preamble-detection-model.h"

namespace ns3
{
	class CustomThresholdPreambleDetectionModel : public PreambleDetectionModel
	{
	public:
		CustomThresholdPreambleDetectionModel();
		~CustomThresholdPreambleDetectionModel();

		static TypeId GetTypeId(void);

		bool IsPreambleDetected(double rssi, double snr, double channelWidth) const;

	private:
		double minSNR;
		double minRssi;
	};
}

#endif /* BROADCAST_CUSTOM_THRESHOLD_PREAMBLE_DETECTION_MODEL_H */
