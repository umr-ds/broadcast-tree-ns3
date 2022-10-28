/*
 * EEBTPTag.cc
 *
 *  Created on: 15.06.2020
 *      Author: krassus
 */

#include "EEBTPTag.h"

#include "ns3/log.h"
#include "ns3/core-module.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("EEBTPTag");
    NS_OBJECT_ENSURE_REGISTERED(EEBTPTag);

    TypeId EEBTPTag::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::EEBTPTag")
                                .SetParent<Tag>()
                                .AddConstructor<EEBTPTag>()
                                .AddAttribute("signal",
                                              "Strength of the received signal in dBm",
                                              EmptyAttributeValue(),
                                              MakeDoubleAccessor(&EEBTPTag::signal),
                                              MakeDoubleChecker<double>())
                                .AddAttribute("noise",
                                              "Strength of the background noise in dBm",
                                              EmptyAttributeValue(),
                                              MakeDoubleAccessor(&EEBTPTag::noise),
                                              MakeDoubleChecker<double>())
                                .AddAttribute("minSNR",
                                              "The minimum required SNR specified by 802.11 (17.3.9.8.4)",
                                              EmptyAttributeValue(),
                                              MakeDoubleAccessor(&EEBTPTag::minSNR),
                                              MakeDoubleChecker<double>())
                                .AddAttribute("txPower",
                                              "The transmission power for the corresponding packet",
                                              EmptyAttributeValue(),
                                              MakeDoubleAccessor(&EEBTPTag::txPower),
                                              MakeDoubleChecker<double>())
                                .AddAttribute("gid",
                                              "The ID of the current game",
                                              EmptyAttributeValue(),
                                              MakeDoubleAccessor(&EEBTPTag::gid),
                                              MakeIntegerChecker<uint64_t>())
                                .AddAttribute("seqNo",
                                              "The sequence number of the current packet",
                                              EmptyAttributeValue(),
                                              MakeDoubleAccessor(&EEBTPTag::seqNo),
                                              MakeIntegerChecker<uint16_t>())
                                .AddAttribute("frameType",
                                              "The frame type of the current packet",
                                              EmptyAttributeValue(),
                                              MakeDoubleAccessor(&EEBTPTag::frameType),
                                              MakeIntegerChecker<uint8_t>());
        return tid;
    }

    TypeId EEBTPTag::GetInstanceTypeId() const
    {
        return GetTypeId();
    }

    uint32_t EEBTPTag::GetSerializedSize() const
    {
        return 43;
    }

    void EEBTPTag::Serialize(TagBuffer i) const
    {
        i.WriteDouble(this->noise);
        i.WriteDouble(this->signal);
        i.WriteDouble(this->minSNR);
        i.WriteDouble(this->txPower);
        i.WriteU64(this->gid);
        i.WriteU16(this->seqNo);
        i.WriteU8(this->frameType);
    }

    void EEBTPTag::Deserialize(TagBuffer i)
    {
        this->noise = i.ReadDouble();
        this->signal = i.ReadDouble();
        this->minSNR = i.ReadDouble();
        this->txPower = i.ReadDouble();
        this->gid = i.ReadU64();
        this->seqNo = i.ReadU16();
        this->frameType = i.ReadU8();
    }

    void EEBTPTag::Print(std::ostream &os) const
    {
        os << "noise = " << this->noise << ", signal = " << this->signal << ", minSNR = " << this->minSNR << ", gid = " << this->gid << ", seqNo = " << this->seqNo << ", FRAME_TYPE = " << (uint)this->frameType;
    }

    /*
	 * Getter
	 */
    double EEBTPTag::getNoise()
    {
        return this->noise;
    }

    double EEBTPTag::getSignal()
    {
        return this->signal;
    }

    double EEBTPTag::getMinSNR()
    {
        return this->minSNR;
    }

    double EEBTPTag::getTxPower()
    {
        return this->txPower;
    }

    void EEBTPTag::setTxPower(double txPower)
    {
        this->txPower = txPower;
    }

    uint64_t EEBTPTag::getGameID()
    {
        return this->gid;
    }

    uint8_t EEBTPTag::getFrameType()
    {
        return this->frameType;
    }

    uint16_t EEBTPTag::getSequenceNumber()
    {
        return this->seqNo;
    }

    /*
	 * Setter
	 */
    void EEBTPTag::setNoise(double noise)
    {
        this->noise = noise;
    }

    void EEBTPTag::setSignal(double signal)
    {
        this->signal = signal;
    }

    void EEBTPTag::setMinSNR(double minSNR)
    {
        this->minSNR = minSNR;
    }

    void EEBTPTag::setGameID(uint64_t gid)
    {
        this->gid = gid;
    }

    void EEBTPTag::setFrameType(uint8_t ft)
    {
        this->frameType = ft;
    }

    void EEBTPTag::setSequenceNumber(uint16_t seqNo)
    {
        this->seqNo = seqNo;
    }
}
