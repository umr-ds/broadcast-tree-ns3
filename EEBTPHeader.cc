/*
 * EEBTPHeader.cc
 *
 *  Created on: 12.05.2020
 *      Author: Kevin Küchler
 *
 *  Energy Efficient Broadcast Tree Protocol (EEBTP)
 *  This header class defines the protocol header which will be added
 *  to every packet that's being sent with the EEBT Protocol.
 *
 *  The following ASCII-Art shows the minimum header which is always sent.
 *  The frame types 4,5 and 6 are using only this minimum header.
 *  +-----8 bits-----+------------16 bits-------------+-------------------------28 bits------------------------+
 *  |    frameType   |        sequence number         |                          gameID                        |
 *  +----------------20 bits-----------------+--------+-------------------32 bits------------------------------+
 *  |                gameID                  |                          txPower_dBm                            |
 *  +----------------------------------------+-----------------------------------------------------------------+
 *
 *  The frame types 0,1,2,3 and 7 have an additional field for the calculated current maximum
 *  transmission power of a node (maxTxPower)
 *  +----------------------------32 bits------------------------------+
 *  |                           maxTxPower                            |
 *  +-----------------------------------------------------------------+
 *
 *  The frame type 0 is used to detect loops in the network. The sender of this frame type
 *  needs to set itself as the originator, the old parent it was connected to and the new
 *  parent it connected to.
 *  +--------------------------------------------48 bits---------------------------------------------+
 *  |                                           originator                                           |
 *  +--------------------------------------------48 bits---------------------------------------------+
 *  |                                           newParent                                            |
 *  +--------------------------------------------48 bits---------------------------------------------+
 *  |                                           oldParent                                            |
 *  +------------------------------------------------------------------------------------------------+
 */

#include "bitset"

#include "ns3/log.h"
#include "ns3/integer.h"
#include "EEBTPHeader.h"
#include "ns3/wifi-utils.h"

namespace ns3
{
	NS_OBJECT_ENSURE_REGISTERED(EEBTPHeader);

	EEBTPHeader::EEBTPHeader()
	{
		this->seqNo = 0;
		this->isShort = false;
		this->gameFinished = false;
		this->needLockUpdate = false;
		this->receivingProblems = false;

		this->gameID = 0;
		this->frameType = 0;

		this->txPower_dBm = 23;
		this->highest_maxTxPower_dBm = WToDbm(0);
		this->second_maxTxPower_dBm = WToDbm(0);
	}

	EEBTPHeader::~EEBTPHeader()
	{
	}

	TypeId EEBTPHeader::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::EEBTPHeader")
								.SetParent<Header>()
								.AddConstructor<EEBTPHeader>()
								.AddAttribute("FrameType", "The type of frame this packet should represent",
											  IntegerValue(1),
											  MakeIntegerAccessor(&EEBTPHeader::frameType),
											  MakeIntegerChecker<int>())
								.AddAttribute("seqNo", "The sequence number of this packet",
											  IntegerValue(0),
											  MakeIntegerAccessor(&EEBTPHeader::seqNo),
											  MakeIntegerChecker<int>())
								.AddAttribute("gameID", "The gameID this packet corresponds to",
											  IntegerValue(0),
											  MakeIntegerAccessor(&EEBTPHeader::gameID),
											  MakeIntegerChecker<int>());
		return tid;
	}

	TypeId EEBTPHeader::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	void EEBTPHeader::Print(std::ostream &os) const
	{
		os << "originator=" << originator.operator ns3::Address();
	}

	void EEBTPHeader::setShort(bool b)
	{
		this->isShort = b;
	}

	uint32_t EEBTPHeader::GetSerializedSize() const
	{
		if (this->isShort)
			return 9;

		uint32_t sSize = 27; //Minimum header size

		if (this->frameType == 0)
		{
			sSize += 18; //originator, newParent and oldParent are 3 * 48 bits large (18 bytes) // @suppress("No break at end of case")
		}
		else if (this->frameType == 3)
		{
			//sSize += 6;						//Recipient

			if (this->needLockUpdate)
				sSize += 6; //New lockholder
		}

		return sSize;
	}

	void EEBTPHeader::Serialize(Buffer::Iterator start) const
	{
		//Write the frame type
		uint8_t ft = this->frameType;

		if (this->receivingProblems)
			ft |= 0b10000000;

		if (this->gameFinished)
			ft |= 0b01000000;

		if (this->needLockUpdate)
			ft |= 0b00100000;

		start.WriteU8(ft);

		//Write sequence number and game ID
		uint64_t seqNo_gid = ((uint64_t)this->seqNo << 48) | this->gameID;
		start.WriteU64(seqNo_gid);

		if (this->isShort)
			return;

		//Write current transmission power
		uint8_t buff[4];
		memcpy(&buff, &this->txPower_dBm, sizeof(this->txPower_dBm));
		start.Write(buff, 4);

		//Write the current parent
		uint8_t addr[6];
		this->parent.CopyTo(addr);
		start.Write(addr, 6);

		//Write highest maxTxPower
		memcpy(&buff, &this->highest_maxTxPower_dBm, sizeof(this->highest_maxTxPower_dBm));
		start.Write(buff, 4);

		//Write second highest maxTxPower
		memcpy(&buff, &this->second_maxTxPower_dBm, sizeof(this->second_maxTxPower_dBm));
		start.Write(buff, 4);

		if (this->frameType == 0)
		{
			this->originator.CopyTo(addr);
			start.Write(addr, 6);

			this->newParent.CopyTo(addr);
			start.Write(addr, 6);

			this->oldParent.CopyTo(addr);
			start.Write(addr, 6);
		}
		else if (this->frameType == 3)
		{
			if (this->needLockUpdate)
			{
				this->originator.CopyTo(addr);
				start.Write(addr, 6);
			}
		}
	}

	uint32_t EEBTPHeader::Deserialize(Buffer::Iterator start)
	{
		uint32_t bytesRead = 27;

		//Read the frame type
		this->frameType = start.ReadU8();

		//Check for receiving problem flag
		if ((this->frameType & 0b10000000) == 0b10000000)
			this->receivingProblems = true;
		else
			this->receivingProblems = false;

		//Check for game finished flag
		if ((this->frameType & 0b01000000) == 0b01000000)
			this->gameFinished = true;
		else
			this->gameFinished = false;

		//Check if the recipient should update its lock
		if ((this->frameType & 0b00100000) == 0b00100000)
			this->needLockUpdate = true;
		else
			this->needLockUpdate = false;

		//Set correct frameType
		this->frameType &= 0b00011111;

		//Read sequence number and game ID
		this->gameID = start.ReadU64();
		this->seqNo = this->gameID >> 48;
		this->gameID &= ((uint64_t)-1) >> 16;

		if (this->isShort)
			return 9;

		//Read the current transmission power of the remote node
		uint8_t buff[4];
		start.Read(buff, 4);
		memcpy(&this->txPower_dBm, &buff, sizeof(this->txPower_dBm));

		//Read current parent
		uint8_t addr[6];
		start.Read(addr, 6);
		this->parent.CopyFrom(addr);

		//Read highest maxTxPower
		start.Read(buff, 4);
		memcpy(&this->highest_maxTxPower_dBm, &buff, sizeof(this->highest_maxTxPower_dBm));

		//Read second highest maxTxPower
		start.Read(buff, 4);
		memcpy(&this->second_maxTxPower_dBm, &buff, sizeof(this->second_maxTxPower_dBm));

		if (this->frameType == 0)
		{
			start.Read(addr, 6);
			this->originator.CopyFrom(addr);

			start.Read(addr, 6);
			this->newParent.CopyFrom(addr);

			start.Read(addr, 6);
			this->oldParent.CopyFrom(addr);

			bytesRead += 18;
		}
		else if (this->frameType == 3)
		{
			if (this->needLockUpdate)
			{
				start.Read(addr, 6);
				this->originator.CopyFrom(addr);
				bytesRead += 6;
			}
		}

		return bytesRead;
	}

	/*
	 * Getter and Setter for the frame type ID field
	 */
	uint8_t EEBTPHeader::GetFrameType()
	{
		return this->frameType;
	}

	void EEBTPHeader::SetFrameType(uint8_t tid)
	{
		this->frameType = tid;
	}

	/*
	 * Getter and Setter for the sequence number
	 */
	uint16_t EEBTPHeader::GetSequenceNumber()
	{
		return this->seqNo;
	}

	void EEBTPHeader::SetSequenceNumber(uint16_t seqNo)
	{
		this->seqNo = seqNo;
	}

	/*
	 * Getter and Setter for the game ID field
	 */
	uint64_t EEBTPHeader::GetGameId()
	{
		return this->gameID;
	}

	void EEBTPHeader::SetGameId(uint64_t gameId)
	{
		this->gameID = gameId;
	}

	bool EEBTPHeader::hadReceivingProblems()
	{
		return this->receivingProblems;
	}

	void EEBTPHeader::setReceivingProblems(bool b)
	{
		this->receivingProblems = b;
	}

	bool EEBTPHeader::getGameFinishedFlag()
	{
		return this->gameFinished;
	}

	void EEBTPHeader::setGameFinishedFlag(bool f)
	{
		this->gameFinished = f;
	}

	bool EEBTPHeader::getNeededLockUpdate()
	{
		return this->needLockUpdate;
	}

	void EEBTPHeader::setNeededLockUpdate(bool b)
	{
		this->needLockUpdate = b;
	}

	Mac48Address EEBTPHeader::GetParent()
	{
		return this->parent;
	}

	void EEBTPHeader::SetParent(Mac48Address addr)
	{
		this->parent = addr;
	}

	/*
	 * Getter and Setter for the current TX power field
	 */
	double EEBTPHeader::GetTxPower()
	{
		return this->txPower_dBm;
	}

	void EEBTPHeader::SetTxPower(double txPower)
	{
		this->txPower_dBm = txPower;
	}

	/*
	 * Getter and Setter for the calculated max TX power field
	 */
	double EEBTPHeader::GetHighestMaxTxPower()
	{
		return this->highest_maxTxPower_dBm;
	}

	void EEBTPHeader::SetHighestMaxTxPower(double maxTxPower)
	{
		this->highest_maxTxPower_dBm = maxTxPower;
	}

	double EEBTPHeader::GetSecondHighestMaxTxPower()
	{
		return this->second_maxTxPower_dBm;
	}

	void EEBTPHeader::SetSecondHighestMaxTxPower(double maxTxPower)
	{
		this->second_maxTxPower_dBm = maxTxPower;
	}

	/*
	 * Getter and Setter for the originator of this packet (only frame type 0)
	 */
	Mac48Address EEBTPHeader::GetOriginator()
	{
		return this->originator;
	}

	void EEBTPHeader::SetOriginator(Mac48Address originator)
	{
		this->originator = originator;
	}

	/*
	 * Getter and Setter for the new parent the node connected to (only frame type 0)
	 */
	Mac48Address EEBTPHeader::GetNewParent()
	{
		return this->newParent;
	}

	void EEBTPHeader::SetNewParent(Mac48Address newParent)
	{
		this->newParent = newParent;
	}

	/*
	 * Getter and Setter for the old parent the node was connected to (only frame type 0)
	 */
	Mac48Address EEBTPHeader::GetOldParent()
	{
		return this->oldParent;
	}

	void EEBTPHeader::SetOldParent(Mac48Address oldParent)
	{
		this->oldParent = oldParent;
	}
}
