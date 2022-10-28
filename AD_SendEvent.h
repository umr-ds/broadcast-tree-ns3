/*
 * AD_SendEvent.h
 *
 *  Created on: 11.09.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_AD_SENDEVENT_H_
#define BROADCAST_AD_SENDEVENT_H_

#include "ns3/event-impl.h"

#include "GameState.h"
#include "EEBTProtocol.h"

namespace ns3
{
	class GameState;
	class EEBTProtocol;

	class ADSendEvent : public EventImpl
	{
	public:
		ADSendEvent(Ptr<GameState> gs, Ptr<EEBTProtocol> prot, uint16_t seqNo);
		~ADSendEvent();

		void Notify();

	private:
		uint16_t seqNo;
		Ptr<GameState> gs;
		Ptr<EEBTProtocol> prot;
	};
}

#endif /* BROADCAST_AD_SENDEVENT_H_ */
