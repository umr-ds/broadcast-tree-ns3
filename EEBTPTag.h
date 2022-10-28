/*
 * EEBTPTag.h
 *
 *  Created on: 15.06.2020
 *      Author: krassus
 */

#ifndef SCRATCH_BROADCAST_EEBTPTAG_H_
#define SCRATCH_BROADCAST_EEBTPTAG_H_

#include <iostream>

#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/wifi-mode.h"

namespace ns3
{
    class EEBTPTag : public Tag
    {
    public:
        static TypeId GetTypeId();
        TypeId GetInstanceTypeId() const;
        virtual uint32_t GetSerializedSize() const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

        double getNoise();
        double getSignal();
        double getMinSNR();

        void setNoise(double noise);
        void setSignal(double signal);
        void setMinSNR(double minSNR);

        double getTxPower();
        void setTxPower(double txPower);

        uint64_t getGameID();
        uint8_t getFrameType();
        uint16_t getSequenceNumber();

        void setGameID(uint64_t gid);
        void setFrameType(uint8_t ft);
        void setSequenceNumber(uint16_t seqNo);

    private:
        double noise;
        double signal;
        double minSNR;

        double txPower;

        uint64_t gid;
        uint16_t seqNo;
        uint8_t frameType;
    };
}

#endif /* SCRATCH_BROADCAST_EEBTPTAG_H_ */
