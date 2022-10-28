/*
 * SendEvent.cc
 *
 *  Created on: 25.06.2020
 *      Author: krassus
 */

#include "LockEvent.h"
#include "ns3/log.h"

namespace ns3
{
	LockEvent::LockEvent(Ptr<GameState> gs, Ptr<EEBTProtocolMutex> prot, Ptr<EEBTPNode> recipient, Mac48Address originator, Mac48Address newOriginator, Mac48Address unused) : EventImpl()
	{
		this->gs = gs;
		this->prot = prot;
		this->recipient = recipient;
		this->originator = originator;
		this->newOriginator = newOriginator;
		this->unused = unused;
		this->counter = 0;
	}

	LockEvent::~LockEvent(){}

	void LockEvent::Notify()
	{
		this->prot->Send(this->gs, this->recipient, this->originator, this->newOriginator, this->unused);
		this->counter++;
	}

	uint32_t LockEvent::getNTimes()
	{
		return this->counter;
	}
}
