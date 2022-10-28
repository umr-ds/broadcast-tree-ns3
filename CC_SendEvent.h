/*
 * SendEvent.h
 *
 *  Created on: 25.06.2020
 *      Author: krassus
 */

#ifndef BROADCAST_CC_SENDEVENT_H_
#define BROADCAST_CC_SENDEVENT_H_

#include "ns3/event-impl.h"

#include "GameState.h"
#include "EEBTProtocol.h"

namespace ns3
{
	class GameState;
	class EEBTProtocol;

	class CCSendEvent : public EventImpl
	{
	public:
		CCSendEvent(Ptr<GameState> gs, Ptr<EEBTProtocol> prot, Mac48Address originator, Mac48Address newParent, Mac48Address oldParent, double txPower, uint16_t seqNo);
		~CCSendEvent();

		void Notify();

		uint32_t getNTimes();
		void updateTxPower(double txPower);

		uint16_t getSeqNo();
		void setSeqNo(uint16_t seqNo);

	private:
		Ptr<GameState> gs;
		Ptr<EEBTProtocol> prot;

		uint16_t seqNo;
		uint32_t counter;
		Mac48Address originator;
		Mac48Address newParent;
		Mac48Address oldParent;

		double txPower;
	};
}

#endif /* BROADCAST_CC_SENDEVENT_H_ */
