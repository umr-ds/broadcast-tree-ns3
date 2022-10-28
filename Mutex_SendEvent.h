/*
 * SendEvent.h
 *
 *  Created on: 25.06.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_MUTEX_SENDEVENT_H_
#define BROADCAST_MUTEX_SENDEVENT_H_

#include "ns3/event-impl.h"

#include "GameState.h"
#include "EEBTProtocol_Mutex.h"

namespace ns3
{
	class GameState;
	class EEBTProtocolMutex;

	class MutexSendEvent : public EventImpl
	{
	public:
		MutexSendEvent(Ptr<GameState> gs, Ptr<EEBTProtocolMutex> prot, Ptr<EEBTPNode> recipient, Mac48Address originator, Mac48Address newOriginator, Mac48Address unused, double txPower, uint16_t seqNo);
		~MutexSendEvent();

		void Notify();

		uint32_t getNTimes();
		void updateTxPower(double txPower);

		uint16_t getSeqNo();
		void setSeqNo(uint16_t seqNo);

	private:
		Ptr<GameState> gs;
		Ptr<EEBTProtocolMutex> prot;

		uint16_t seqNo;
		uint32_t counter;
		Ptr<EEBTPNode> recipient;
		Mac48Address originator;
		Mac48Address newOriginator;
		Mac48Address unused;

		double txPower;
	};
}

#endif /* BROADCAST_CC_SENDEVENT_H_ */
