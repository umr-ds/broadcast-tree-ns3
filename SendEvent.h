/*
 * SendEvent.h
 *
 *  Created on: 25.06.2020
 *      Author: krassus
 */

#ifndef BROADCAST_SENDEVENT_H_
#define BROADCAST_SENDEVENT_H_

#include "ns3/event-impl.h"

#include "GameState.h"
#include "EEBTProtocol.h"

namespace ns3
{
	class GameState;
	class EEBTProtocol;

	class SendEvent : public EventImpl
	{
	public:
		SendEvent(Ptr<GameState> gs, Ptr<EEBTProtocol> prot, FRAME_TYPE ft, Mac48Address recipient, double txPower, uint16_t seqNo);
		~SendEvent();

		void Notify();

		uint32_t getNTimes();
		void updateTxPower(double txPower);

		uint16_t getSeqNo();
		void setSeqNo(uint16_t seqNo);

	private:
		Ptr<GameState> gs;
		Ptr<EEBTProtocol> prot;

		FRAME_TYPE ft;
		uint16_t seqNo;
		Mac48Address recipient;

		double txPower;
		uint32_t counter;
	};
}

#endif /* BROADCAST_SENDEVENT_H_ */
