/*
 * EEBTPHeader.h
 *
 *  Created on: 12.05.2020
 *      Author: Kevin Küchler
 */

#ifndef BROADCAST_EEBTPHeader_H_
#define BROADCAST_EEBTPHeader_H_

#include "ns3/header.h"
#include "ns3/mac48-address.h"

namespace ns3
{
	class EEBTPHeader : public Header
	{
	public:
		EEBTPHeader();
		virtual ~EEBTPHeader();

		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;
		virtual void Print(std::ostream &os) const;
		virtual void Serialize(Buffer::Iterator start) const;
		virtual uint32_t Deserialize(Buffer::Iterator start);
		virtual uint32_t GetSerializedSize() const;

		void setShort(bool b);

		uint8_t GetFrameType();
		void SetFrameType(uint8_t tid);

		uint16_t GetSequenceNumber();
		void SetSequenceNumber(uint16_t seqNo);

		uint64_t GetGameId();
		void SetGameId(uint64_t gameId);

		bool hadReceivingProblems();
		void setReceivingProblems(bool b);

		bool getGameFinishedFlag();
		void setGameFinishedFlag(bool f);

		bool getNeededLockUpdate();
		void setNeededLockUpdate(bool b);

		Mac48Address GetParent();
		void SetParent(Mac48Address addr);

		double GetTxPower();
		void SetTxPower(double txPower);

		double GetHighestMaxTxPower();
		void SetHighestMaxTxPower(double maxTxPower);

		double GetSecondHighestMaxTxPower();
		void SetSecondHighestMaxTxPower(double maxTxPower);

		/*Mac48Address GetRecipient();
			void SetRecipient(Mac48Address addr);*/

		Mac48Address GetOriginator();
		void SetOriginator(Mac48Address originator);

		Mac48Address GetNewParent();
		void SetNewParent(Mac48Address newParent);

		Mac48Address GetOldParent();
		void SetOldParent(Mac48Address oldParent);

	protected:
		bool isShort;

		bool gameFinished;
		bool needLockUpdate;
		bool receivingProblems;

		//Frame type: 4,5,6 (unicast)
		uint8_t frameType; //Valid types: 0,1,2,3,4,5,6,7						[ 1 byte ]
		uint64_t gameID;   //48 bits gID, 16 bits seqNo						[ 8 bytes]
		uint16_t seqNo;
		float txPower_dBm;	 //Power at the sender side in dBm					[ 4 bytes]
		Mac48Address parent; //Address of the current parent						[ 6 bytes]

		//Frame type: 1 (broadcast), 2 (unicast)
		float highest_maxTxPower_dBm; //Highest maxTxPower								[ 4 bytes]
		float second_maxTxPower_dBm;  //Second highest maxTxPower							[ 4 bytes]

		//Frame type: 3 (multicast) only
		Mac48Address recipient; //Address of the actual recipient					[ 6 bytes]

		//Frame type: 7 (broadcast)

		//Frame type: 0 (multicast)
		Mac48Address originator; //													[ 6 bytes]
		Mac48Address newParent;	 //													[ 6 bytes]
		Mac48Address oldParent;	 //													[ 6 bytes]
	};
}

#endif /* BROADCAST_SIMPLEBROADCASTHEADER_H_ */
