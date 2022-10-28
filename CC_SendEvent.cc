/*
 * SendEvent.cc
 *
 *  Created on: 25.06.2020
 *      Author: krassus
 */

#include "ns3/log.h"
#include "ns3/core-module.h"

#include "CC_SendEvent.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("CCSendEvent");

	CCSendEvent::CCSendEvent(Ptr<GameState> gs, Ptr<EEBTProtocol> prot, Mac48Address originator, Mac48Address newParent, Mac48Address oldParent, double txPower, uint16_t seqNo) : EventImpl()
	{
		this->gs = gs;
		this->prot = prot;
		this->seqNo = seqNo;
		this->txPower = txPower;
		this->counter = 0;

		this->originator = originator;
		this->newParent = newParent;
		this->oldParent = oldParent;
	}

	CCSendEvent::~CCSendEvent() {}

	void CCSendEvent::Notify()
	{
		NS_LOG_DEBUG("[" << this->prot->GetDevice()->GetNode()->GetId() << " / " << Now() << "]: CCSendEvent fired with originator: " << this->originator << ", nP: " << this->newParent << ", oP: " << this->oldParent);
		this->prot->Send(this->gs, this->originator, this->newParent, this->oldParent, this->seqNo, this->txPower, this);
		this->counter++;
	}

	uint32_t CCSendEvent::getNTimes()
	{
		return this->counter;
	}

	void CCSendEvent::updateTxPower(double txPower)
	{
		this->txPower = txPower;
	}

	uint16_t CCSendEvent::getSeqNo()
	{
		return this->seqNo;
	}

	void CCSendEvent::setSeqNo(uint16_t seqNo)
	{
		this->seqNo = seqNo;
	}
}
