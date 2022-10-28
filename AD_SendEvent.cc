/*
 * AD_SendEvent.cc
 *
 *  Created on: 11.09.2020
 *      Author: Kevin Küchler
 */

#include "ns3/log.h"
#include "ns3/core-module.h"

#include "AD_SendEvent.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("ADSendEvent");

	ADSendEvent::ADSendEvent(Ptr<GameState> gs, Ptr<EEBTProtocol> prot, uint16_t seqNo) : EventImpl()
	{
		this->gs = gs;
		this->prot = prot;
		this->seqNo = seqNo;
	}

	ADSendEvent::~ADSendEvent() {}

	void ADSendEvent::Notify()
	{
		this->prot->sendApplicationData(gs, seqNo);
	}
}
