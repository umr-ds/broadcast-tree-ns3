/*
 * SendEvent.cc
 *
 *  Created on: 25.06.2020
 *      Author: krassus
 */

#include "ns3/log.h"
#include "ns3/core-module.h"

#include "SendEvent.h"

namespace ns3
{
	SendEvent::SendEvent(Ptr<GameState> gs, Ptr<EEBTProtocol> prot, FRAME_TYPE ft, Mac48Address recipient, double txPower, uint16_t seqNo) : EventImpl()
	{
		this->gs = gs;
		this->prot = prot;
		this->ft = ft;
		this->seqNo = seqNo;
		this->recipient = recipient;
		this->txPower = txPower;
		this->counter = 0;
	}

	SendEvent::~SendEvent() {}

	void SendEvent::Notify()
	{
		//NS_LOG_UNCOND("[" << Now() << "]: SendEvent fired!");
		this->prot->Send(this->gs, this->ft, this->recipient, seqNo, this->txPower, this);
		this->counter++;
	}

	uint32_t SendEvent::getNTimes()
	{
		return this->counter;
	}

	void SendEvent::updateTxPower(double txPower)
	{
		this->txPower = txPower;
	}

	uint16_t SendEvent::getSeqNo()
	{
		return this->seqNo;
	}

	void SendEvent::setSeqNo(uint16_t seqNo)
	{
		this->seqNo = seqNo;
	}
}
