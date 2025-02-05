/*
 * SSORAM.hpp
 *
 *  Created on: Mar 15, 2017
 *      Author: ceyhunozkaptan, thanghoang
 */

#ifndef SSORAM_HPP
#define SSORAM_HPP

#include "config.h"

class S3ORAM
{
public:
    S3ORAM();
    ~S3ORAM();

	int build(vector<TYPE_INDEX> *pos_map, unsigned long int *exp_logs);

    int getEvictIdx (TYPE_INDEX *srcIdx, TYPE_INDEX *destIdx, TYPE_INDEX *siblIdx, string evict_str);

	string getEvictString(TYPE_ID n_evict);

	int subSetSequenceIdx(TYPE_INDEX* fullPath, TYPE_INDEX pathID);

    int createShares(TYPE_DATA input, TYPE_DATA* output);

	int getSharedVector(TYPE_DATA* logicVector, TYPE_DATA** sharedVector);

	int simpleRecover(TYPE_DATA** shares, TYPE_DATA* result);

	int precomputeShares(TYPE_DATA input, TYPE_DATA** output, TYPE_INDEX output_size);

};

#endif // SSORAM_HPP
