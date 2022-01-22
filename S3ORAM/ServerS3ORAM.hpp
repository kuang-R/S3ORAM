/*
 * ServerS3ORAM.hpp
 *
 *  Created on: Apr 7, 2017
 *      Author: ceyhunozkaptan, thanghoang
 */

#ifndef SERVERS3ORAM_HPP
#define SERVERS3ORAM_HPP

#include "config.h"
#include <zmq.hpp>
#include <pthread.h>

class ServerS3ORAM
{
private:

    //local variable
    std::string CLIENT_ADDR;
	
    //variables for retrieval
    zz_p** dot_product_vector;
    TYPE_DATA* sumBlock;
	unsigned char* stashIndex_buffer_in;
    
    //socket 
    unsigned char* block_buffer_in;
    unsigned char* block_buffer_out;
    
    unsigned char* stash_buffer_out;
    unsigned char** shares_buffer_out;

public:
    ServerS3ORAM(); 
    ~ServerS3ORAM();

    int start();
    
    // main functions
    int retrieve(zmq::socket_t& socket);
    int evict(zmq::socket_t& socket);
    int recvBlock(zmq::socket_t& socket); 
    int recvORAMTree(zmq::socket_t& socket);

    // retrieval subroutine 
    static void* thread_dotProduct_func(void* args);

    // eviction subroutine
    int multEvictTriplet( zz_p** evictMatrix);

    static int send(std::string ADDR, unsigned char* input, size_t inputSize);
    static int recv(std::string ADDR, unsigned char* output, size_t outputSize);

    //thread functions
    static void* thread_crossProduct_func(void* args);
    static void* thread_socket_func(void* args);
    static void* loadRetrievalData_func(TYPE_INDEX* sss, unsigned char* sbo);
    static void* thread_loadTripletData_func(void* args);
    
    static unsigned long int server_logs[13]; 
    static unsigned long int thread_max;
    static char timestamp[16];
};

#endif // SERVERS3ORAM_HPP
