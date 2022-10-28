/*
 * SendEvent.cc
 *
 *  Created on: 25.06.2020
 *      Author: krassus
 */

#include "ns3/log.h"
#include "ns3/core-module.h"

#include "Mutex_SendEvent.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("MutexSendEvent");

	MutexSendEvent::MutexSendEvent(Ptr<GameState> gs, Ptr<EEBTProtocolMutex> prot, Ptr<EEBTPNode> recipient, Mac48Address originator, Mac48Address newOriginator, Mac48Address unused, double txPower, uint16_t seqNo) : EventImpl()
	{
		this->gs = gs;
		this->prot = prot;
		this->seqNo = seqNo;
		this->txPower = txPower;
		this->counter = 0;

		this->recipient = recipient;
		this->originator = originator;
		this->newOriginator = newOriginator;
		this->unused = unused;
	}

	MutexSendEvent::~MutexSendEvent() {}

	void MutexSendEvent::Notify()
	{
		this->prot->Send(this->gs, this->recipient, this->originator, this->newOriginator, this->unused, this->seqNo, this->txPower, this);
		this->counter++;
	}

	uint32_t MutexSendEvent::getNTimes()
	{
		return this->counter;
	}

	void MutexSendEvent::updateTxPower(double txPower)
	{
		this->txPower = txPower;
	}

	uint16_t MutexSendEvent::getSeqNo()
	{
		return this->seqNo;
	}

	void MutexSendEvent::setSeqNo(uint16_t seqNo)
	{
		this->seqNo = seqNo;
	}
}
