/*
 * SeqNoCache.h
 *
 *  Created on: 02.06.2020
 *      Author: krassus
 */

#ifndef BROADCAST_CACHE_SEQNOCACHE_H_
#define BROADCAST_CACHE_SEQNOCACHE_H_

#include "map"
#include "ns3/mac48-address.h"

#include "EEBTPHeader.h"

namespace ns3
{
	class SeqNoCache
	{
	public:
		SeqNoCache();
		virtual ~SeqNoCache();

		bool checkForDuplicate(Mac48Address sender, uint16_t seqNo);

		void injectSeqNo(EEBTPHeader *header);

	private:
		uint16_t seqNo;
		//std::map<Mac48Address,std::pair<std::map<uint16_t, bool>, std::pair<uint16_t, uint16_t>>> cache;
		std::map<Mac48Address, std::map<uint16_t, bool>> cache;
		std::map<Mac48Address, std::vector<uint16_t>> cacheX;
	};
}

#endif /* BROADCAST_CACHE_SEQNOCACHE_H_ */
