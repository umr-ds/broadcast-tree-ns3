/*
 * EEBTPHeader_SrcPath.cc
 *
 *  Created on: 21.07.2020
 *      Author: krassus
 */

#include "bitset"

#include "ns3/log.h"
#include "ns3/integer.h"
#include "EEBTPHeader_SrcPath.h"

namespace ns3
{
	NS_OBJECT_ENSURE_REGISTERED(EEBTPHeaderSrcPath);

	EEBTPHeaderSrcPath::EEBTPHeaderSrcPath() : EEBTPHeader()
	{
	}

	EEBTPHeaderSrcPath::~EEBTPHeaderSrcPath()
	{
	}

	TypeId EEBTPHeaderSrcPath::GetTypeId()
	{
		static TypeId tid = TypeId("ns3::EEBTPHeaderSrcPath")
								.SetParent<EEBTPHeader>()
								.AddConstructor<EEBTPHeaderSrcPath>();
		return tid;
	}

	TypeId EEBTPHeaderSrcPath::GetInstanceTypeId() const
	{
		return GetTypeId();
	}

	void EEBTPHeaderSrcPath::Print(std::ostream &os) const
	{
		os << "originator=" << originator.operator ns3::Address();
	}

	uint32_t EEBTPHeaderSrcPath::GetSerializedSize() const
	{
		uint32_t sSize = 21; //Minimum header size

		if (this->frameType < 2 || this->frameType == 3)
		{
			sSize += 1;
			sSize += this->path.size() * 6;
		}

		return sSize;
	}

	void EEBTPHeaderSrcPath::Serialize(Buffer::Iterator start) const
	{
		//Write the frame type
		uint8_t ft = this->frameType;

		if (this->receivingProblems)
			ft |= 0b10000000;

		if (this->gameFinished)
			ft |= 0b01000000;

		start.WriteU8(ft);

		//Write sequence number and game ID
		uint64_t seqNo_gid = ((uint64_t)this->seqNo << 48) | this->gameID;
		start.WriteU64(seqNo_gid);

		//Write current transmission power
		uint8_t buff[4];
		memcpy(&buff, &this->txPower_dBm, sizeof(this->txPower_dBm));
		start.Write(buff, 4);

		//Write highest maxTxPower
		memcpy(&buff, &this->highest_maxTxPower_dBm, sizeof(this->highest_maxTxPower_dBm));
		start.Write(buff, 4);

		//Write second highest maxTxPower
		memcpy(&buff, &this->second_maxTxPower_dBm, sizeof(this->second_maxTxPower_dBm));
		start.Write(buff, 4);

		uint8_t addr[6];
		if (this->frameType < 2)
		{
			//Write length of the node list
			start.WriteU8(this->path.size());

			//Write path
			for (uint i = 0; i < this->path.size(); i++)
			{
				this->path[i].CopyTo(addr);
				start.Write(addr, 6);
			}
		}
		else if (this->frameType == 3)
		{
			this->recipient.CopyTo(addr);
			start.Write(addr, 6);
		}
	}

	uint32_t EEBTPHeaderSrcPath::Deserialize(Buffer::Iterator start)
	{
		uint32_t bytesRead = 13;

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

		//Set correct frameType
		this->frameType &= 0b00111111;

		//Read sequence number and game ID
		this->gameID = start.ReadU64();
		this->seqNo = this->gameID >> 48;
		this->gameID &= ((uint64_t)-1) >> 16;

		//Read the current transmission power of the remote node
		uint8_t buff[4];
		start.Read(buff, 4);
		memcpy(&this->txPower_dBm, &buff, sizeof(this->txPower_dBm));

		//Read highest maxTxPower
		start.Read(buff, 4);
		memcpy(&this->highest_maxTxPower_dBm, &buff, sizeof(this->highest_maxTxPower_dBm));

		//Read second highest maxTxPower
		start.Read(buff, 4);
		memcpy(&this->second_maxTxPower_dBm, &buff, sizeof(this->second_maxTxPower_dBm));

		//Only frame type 0 and 1 have a src path
		uint8_t addr[6];
		if (this->frameType == 1 || this->frameType == 3)
		{
			//Read length of the node list
			uint length = start.ReadU8();
			bytesRead += 1;

			//Read path
			Mac48Address node;
			for (uint i = 0; i < length; i++)
			{
				start.Read(addr, 6);
				node.CopyFrom(addr);
				this->addNodeToPath(node);

				bytesRead += 6;
			}
		}

		return bytesRead;
	}

	void EEBTPHeaderSrcPath::addNodeToPath(Mac48Address addr)
	{
		this->path.push_back(addr);
	}

	std::vector<Mac48Address> EEBTPHeaderSrcPath::getSrcPath()
	{
		return this->path;
	}
}
