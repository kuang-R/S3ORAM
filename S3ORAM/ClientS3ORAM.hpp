/*
 * ClientS3ORAM.hpp
 *
 *  Created on: Mar 15, 2017
 *      Author: ceyhunozkaptan, thanghoang
 */
 
#ifndef CLIENTS3ORAM_HPP
#define CLIENTS3ORAM_HPP

#include "config.h"
#include <pthread.h>
#include "zmq.hpp"
#include "struct_socket.h"
struct TYPE_DATA_CACHE
{
	TYPE_INDEX logicalID;
	TYPE_DATA DATA[DATA_CHUNKS];
};
class ClientS3ORAM
{
private:
	//client storage for ORAM operations
	vector<TYPE_INDEX> *pos_map;
	
	vector<TYPE_DATA *> *stash;
	vector<TYPE_DATA_CACHE> *data_cache;
   
    
    unsigned char* stash_index_buffer_out;	
    unsigned char* stash_buffer_out; 
    unsigned char* stash_buffer_in;

public:
    ClientS3ORAM();
    ~ClientS3ORAM();

    //main functions
    int init();
    int load();
    int access(TYPE_INDEX blockID);
    int sendORAMTree();
    
    //retrieval_vector
    int getLogicalVector(TYPE_DATA* logicalVector, TYPE_ID blockID);

    //eviction_matrix
    int getEvictMatrix(TYPE_DATA** evictMatrix, TYPE_INDEX n_evict);
    
    //socket
	static void* thread_socket_func(void* args);	
    static int sendNrecv(std::string ADDR, unsigned char* data_out, size_t data_out_size, unsigned char* data_in, size_t data_in_size, int CMD);

    //logging
	static unsigned long int exp_logs[9]; 	static unsigned long int thread_max;
	static char timestamp[16];
	
};

#endif // CLIENTORAM_HPP
