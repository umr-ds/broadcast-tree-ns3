/*
 * SendEvent.h
 *
 *  Created on: 13.09.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_PPC_EVENT_H_
#define BROADCAST_PPC_EVENT_H_

#include "ns3/event-impl.h"

#include "GameState.h"
#include "EEBTProtocol_SrcPath.h"

namespace ns3
{
	class GameState;
	class EEBTProtocolSrcPath;

	class ParentPathCheckEvent : public EventImpl
	{
	public:
		ParentPathCheckEvent(Ptr<GameState> gs, Ptr<EEBTProtocolSrcPath> prot);
		~ParentPathCheckEvent();

		void Notify();

	private:
		Ptr<GameState> gs;
		Ptr<EEBTProtocolSrcPath> prot;
	};
}

#endif /* BROADCAST_PPC_EVENT_H_ */
