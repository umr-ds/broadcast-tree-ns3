/*
 * SeqNoCache.cc
 *
 *  Created on: 02.06.2020
 *      Author: krassus
 */

#include "SeqNoCache.h"

namespace ns3
{
	SeqNoCache::SeqNoCache()
	{
		this->seqNo = 0;
	}

	SeqNoCache::~SeqNoCache()
	{
		this->cache.clear();
	}

	bool SeqNoCache::checkForDuplicate(Mac48Address sender, uint16_t seqNo)
	{
		for (uint16_t x : this->cacheX[sender])
		{
			if (x == seqNo)
				return true;
		}

		this->cacheX[sender].push_back(seqNo);

		//Remove all sequence number from sender while there are more than 1000 stored
		while (this->cacheX[sender].size() > 1000)
			this->cacheX[sender].erase(this->cacheX[sender].begin());
		return false;
	}

	void SeqNoCache::injectSeqNo(EEBTPHeader *header)
	{
		header->SetSequenceNumber(++this->seqNo);
	}
}
