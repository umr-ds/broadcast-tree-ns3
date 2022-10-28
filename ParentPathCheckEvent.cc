/*
 * SendEvent.cc
 *
 *  Created on: 13.09.2020
 *      Author: Kevin Küchler
 */

#include "ns3/log.h"
#include "ParentPathCheckEvent.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("ParentPathCheckEvent");

	ParentPathCheckEvent::ParentPathCheckEvent(Ptr<GameState> gs, Ptr<EEBTProtocolSrcPath> prot) : EventImpl()
	{
		this->gs = gs;
		this->prot = prot;
	}

	ParentPathCheckEvent::~ParentPathCheckEvent() {}

	void ParentPathCheckEvent::Notify()
	{
		this->prot->checkParentPathStatus(this->gs);
	}
}
