/*
 * SendEvent.h
 *
 *  Created on: 25.06.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_LOCKEVENT_H_
#define BROADCAST_LOCKEVENT_H_

#include "ns3/event-impl.h"

#include "GameState.h"
#include "EEBTProtocol_Mutex.h"

namespace ns3
{
	class GameState;
	class EEBTProtocolMutex;

	class LockEvent : public EventImpl
	{
	public:
		LockEvent(Ptr<GameState> gs, Ptr<EEBTProtocolMutex> prot, Ptr<EEBTPNode> recipient, Mac48Address originator, Mac48Address newOriginator, Mac48Address unused);
		~LockEvent();

		void Notify();

		uint32_t getNTimes();

	private:
		Ptr<GameState> gs;
		Ptr<EEBTProtocolMutex> prot;
		Ptr<EEBTPNode> recipient;
		Mac48Address originator;
		Mac48Address newOriginator;
		Mac48Address unused;
		uint32_t counter;
	};
}

#endif /* BROADCAST_LOCKEVENT_H_ */
