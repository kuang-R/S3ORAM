/*
 * ServerS3ORAM.cpp
 *
 *  Created on: Apr 7, 2017
 *      Author: ceyhunozkaptan, thanghoang
 */

#include "ServerS3ORAM.hpp"
#include "Utils.hpp"
#include "struct_socket.h"

#include "S3ORAM.hpp"
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>

#include "struct_thread_computation.h"
#include "struct_thread_loadData.h"


unsigned long int ServerS3ORAM::server_logs[13];
unsigned long int ServerS3ORAM::thread_max = 0;
char ServerS3ORAM::timestamp[16];

ServerS3ORAM::ServerS3ORAM()
{
	this->CLIENT_ADDR = "tcp://*:" + std::to_string(SERVER_PORT);
	
	cout<<endl;
	cout << "=================================================================" << endl;
	cout<< "Server Starting" <<endl;
	cout << "=================================================================" << endl;
	
    this->sumBlock = new TYPE_DATA[DATA_CHUNKS];
	this->block_buffer_in = new unsigned char[sizeof(TYPE_DATA)*DATA_CHUNKS];
	this->block_buffer_out = new unsigned char[sizeof(TYPE_DATA)*DATA_CHUNKS];
}

ServerS3ORAM::~ServerS3ORAM()
{
}


/**
 * Function Name: start
 *
 * Description: Starts the server to wait for a command from the client. 
 * According to the command, server performs certain subroutines for distributed ORAM operations.
 * 
 * @return 0 if successful
 */ 
int ServerS3ORAM::start()
{
	int ret = 1;
	int CMD;
    unsigned char buffer[sizeof(CMD)];
    zmq::context_t context(1);
    zmq::socket_t socket(context,ZMQ_REP);
    
	cout<< "[Server] Socket is OPEN on " << this->CLIENT_ADDR << endl;
    socket.bind(this->CLIENT_ADDR.c_str());

	while (true) 
	{
		cout<< "[Server] Waiting for a Command..." <<endl;
        socket.recv(buffer,sizeof(CMD));
		
        memcpy(&CMD, buffer, sizeof(CMD));
		cout<< "[Server] Command RECEIVED!" <<endl;
		
        socket.send((unsigned char*)CMD_SUCCESS,sizeof(CMD_SUCCESS));
        
        switch(CMD)
        {
			case CMD_SEND_ORAM_TREE:
				cout<<endl;
				cout << "=================================================================" << endl;
				cout<< "[Server] Receiving ORAM Data..." <<endl;
				cout << "=================================================================" << endl;
				this->recvORAMTree(socket);
				cout << "=================================================================" << endl;
				cout<< "[Server] ORAM Data RECEIVED!" <<endl;
				cout << "=================================================================" << endl;
				cout<<endl;
				break;
			case CMD_REQUEST_BLOCK:
				cout<<endl;
				cout << "=================================================================" << endl;
				cout<< "[Server] Receiving Logical Vector..." <<endl;
				cout << "=================================================================" << endl;
				//this->retrieve(socket);
				cout << "=================================================================" << endl;
				cout<< "[Server] Block Share SENT" <<endl;
				cout << "=================================================================" << endl;
				cout<<endl;
				break;
            case CMD_SEND_BLOCK:
				cout<<endl;
            	cout << "=================================================================" << endl;
				cout<< "[Server] Receiving Block Data..." <<endl;
				cout << "=================================================================" << endl;
				//this->recvBlock(socket);
				cout << "=================================================================" << endl;
				cout<< "[Server] Block Data RECEIVED!" <<endl;
				cout << "=================================================================" << endl;
				cout<<endl;
				break;
			case CMD_SEND_EVICT:
				cout<<endl;
				cout << "=================================================================" << endl;
				cout<< "Receiving Eviction Matrix..." <<endl;
				cout << "=================================================================" << endl;
				//this->evict(socket);
				cout << "=================================================================" << endl;
				cout<< "[Server] EVICTION and DEGREE REDUCTION DONE!" <<endl;
				cout << "=================================================================" << endl;
				cout<<endl;
				break;
			default:
				break;
		}
	}
	
	ret = 0;
    return ret;
}


/**
 * Function Name: sendORAMTree
 *
 * Description: Distributes generated and shared ORAM buckets to servers over network
 * 
 * @return 0 if successful
 */  
 
int ServerS3ORAM::recvORAMTree(zmq::socket_t& socket)
{
    int ret = 1;
	string path = rootPath + to_string(0) + "/" + to_string(0);
    FILE* file_out = NULL;
	if((file_out = fopen(path.c_str(),"wb+")) == NULL)
	{
		cout<< "	[recvORAMTree] File Cannot be Opened!!" <<endl;
		exit(0);
	}
    for(int i = 0 ; i < NStore; i++)
    {
        socket.recv(block_buffer_in, sizeof(TYPE_DATA)*DATA_CHUNKS, 0);
        fwrite(block_buffer_in, 1, BUCKET_SIZE*sizeof(TYPE_DATA)*DATA_CHUNKS, file_out);
		socket.send((unsigned char*)CMD_SUCCESS,sizeof(CMD_SUCCESS),0);
    }
	fclose(file_out);
	
	cout<< "	[recvORAMTree] ACK is SENT!" <<endl;
	ret = 0;
    return ret ;
}


/**
 * Function Name: retrieve
 *
 * Description: Starts retrieve operation for a block by receiving logical access vector and path ID from the client. 
 * According to path ID, server performs dot-product operation between its block shares on the path and logical access vector.
 * The result of the dot-product is send back to the client.
 * 
 * @param socket: (input) ZeroMQ socket instance for communication with the client
 * @return 0 if successful
 */  
int ServerS3ORAM::retrieve(zmq::socket_t& socket)
{
	
}


/**
 * Function Name: recvBlock
 *
 * Description: Receives the share of previosly accessed block from the client 
 * with its new index number and stores it into root bucket for later eviction. 
 * 
 * @param socket: (input) ZeroMQ socket instance for communication with the client
 * @return 0 if successful
 */  
int ServerS3ORAM::recvBlock(zmq::socket_t& socket)
{
	cout<< "	[recvBlock] Receiving Block Data..." <<endl;
	auto start = time_now;
	socket.recv(block_buffer_in, sizeof(TYPE_DATA)*DATA_CHUNKS+sizeof(TYPE_INDEX), 0);
	auto end = time_now;
    TYPE_INDEX slotIdx;
    memcpy(&slotIdx,&block_buffer_in[sizeof(TYPE_DATA)*DATA_CHUNKS],sizeof(TYPE_INDEX));
    
	cout<< "	[recvBlock] Block Data RECV in " << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() <<endl;
    server_logs[4] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    
	start = time_now;
    // Update root bucket
    FILE *file_update;
    string path = rootPath + to_string(0) + "/0";
    if((file_update = fopen(path.c_str(),"r+b")) == NULL)
    {
        cout<< "	[recvBlock] File Cannot be Opened!!" <<endl;
        exit(0);
    }
    fseek(file_update, slotIdx*sizeof(TYPE_DATA),SEEK_SET);
    for(int u = 0 ; u < DATA_CHUNKS; u++)
    {
        fwrite(&block_buffer_in[u*sizeof(TYPE_DATA)],1,sizeof(TYPE_DATA),file_update);
        fseek(file_update,(BUCKET_SIZE-1)*sizeof(TYPE_DATA),SEEK_CUR);
    }
    fclose(file_update);
    
    end = time_now;
	cout<< "	[recvBlock] Block STORED in Disk in " << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() <<endl;
	server_logs[5] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
	
    socket.send((unsigned char*)CMD_SUCCESS,sizeof(CMD_SUCCESS));
	cout<< "	[recvBlock] ACK is SENT!" <<endl;
    
    return 0;
}

/**
 * Function Name: evict
 *
 * Description: Starts eviction operation with the command of the client by receiving eviction matrix
 * and eviction path no from the client. According to eviction path no, the server performs 
 * matrix multiplication with its buckets and eviction matrix to evict blocks. After eviction operation,
 * the degree of the sharing polynomial doubles. Thus all the servers distributes their shares and perform 
 * degree reduction routine simultaneously. 
 * 
 * @param socket: (input) ZeroMQ socket instance for communication with the client
 * @return 0 if successful
 */  
int ServerS3ORAM::evict(zmq::socket_t& socket)
{
    
    return 0;
}


/**
 * Function Name: multEvictTriplet
 *
 * Description: Performs matrix multiplication between received eviction matrix and affected buckets
 * for eviction operation 
 * 
 * @param evictMatrix: (input) Received eviction matrix from the clietn
 * @return 0 if successful
 */  
int ServerS3ORAM::multEvictTriplet(zz_p** evictMatrix)
{
	
}


/**
 * Function Name: thread_socket_func & send & recv
 *
 * Description: Generic threaded socket functions for send and receive operations
 * 
 * @return 0 if successful
 */  
void *ServerS3ORAM::thread_socket_func(void* args)
{
    struct_socket* opt = (struct_socket*) args;
	
	if(opt->isSend)
	{
		auto start = time_now;
		send(opt->ADDR, opt->data_out, opt->data_out_size);
		auto end = time_now;
		if(thread_max < std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count())
			thread_max = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
	}
	else
	{
		recv(opt->ADDR, opt->data_in, opt->data_in_size);
	}
    pthread_exit((void*)opt);
}
int ServerS3ORAM::send(std::string ADDR, unsigned char* input, size_t inputSize)
{
	zmq::context_t context(1);
    zmq::socket_t socket(context,ZMQ_REQ);

    socket.connect(ADDR.c_str());
	
    unsigned char buffer_in[sizeof(CMD_SUCCESS)];
	
    try
    {
		cout<< "	[ThreadedSocket] Sending to " << ADDR << endl;
		socket.send (input, inputSize);
		cout<< "	[ThreadedSocket] Data SENT!" << ADDR << endl;
        
        socket.recv(buffer_in, sizeof(CMD_SUCCESS));
        cout<< "	[ThreadedSocket] ACK RECEIVED!" << ADDR << endl;
    }
    catch (exception &ex)
    {
        goto exit;
    }

exit:
	socket.disconnect(ADDR.c_str());
	socket.close();
	return 0;
}
int ServerS3ORAM::recv(std::string ADDR, unsigned char* output, size_t outputSize)
{
	zmq::context_t context(1);
    zmq::socket_t socket(context,ZMQ_REP);
	
    socket.bind(ADDR.c_str());
	
    try
    {
		cout<< "	[ThreadedSocket] Waiting Client on " << ADDR << endl;
		socket.recv (output, outputSize);
		cout<< "	[ThreadedSocket] Data RECEIVED! " << ADDR <<endl;
        
        socket.send((unsigned char*)CMD_SUCCESS,sizeof(CMD_SUCCESS));
        cout<< "	[ThreadedSocket] ACK SENT! "  << ADDR<<endl;
    }
    catch (exception &ex)
    {
        cout<<"Socket error!";
        goto exit;
    }
    
exit:
	socket.close();
	return 0;
}


/**
 * Function Name: thread_dotProduct_func
 *
 * Description: Threaded dot-product operation 
 * 
 */  
void *ServerS3ORAM::thread_dotProduct_func(void* args)
{
    THREAD_COMPUTATION* opt = (THREAD_COMPUTATION*) args;
  
    //std::cout << " CPU # " << sched_getcpu() << "\n";
    int size = (H+1)*BUCKET_SIZE;
    for(int k = opt->startIdx; k < opt->endIdx; k++)
    {
        opt->dot_product_output[k] = InnerProd_LL(opt->data_vector[k],opt->select_vector,size,P,zz_p::ll_red_struct());
    }
}


/**
 * Function Name: thread_crossProduct_func
 *
 * Description: Threaded cross-product operation 
 * 
 */  
void *ServerS3ORAM::thread_crossProduct_func(void* args)
{
    THREAD_COMPUTATION* opt = (THREAD_COMPUTATION*) args;
    
    int currBucket;
	int currIndex;
	TYPE_INDEX n = BUCKET_SIZE*(2);
    //std::cout << " CPU # " << sched_getcpu() << "\n";
	for(int l = opt->startIdx ; l < opt->endIdx; l++)
    {
        currBucket = l/BUCKET_SIZE; //this can be removed later
		currIndex = l % BUCKET_SIZE; 
		
        for(int k = 0 ; k < DATA_CHUNKS; k++)
        {
            opt->cross_product_output[k][currBucket*BUCKET_SIZE + currIndex] = InnerProd_LL(opt->data_vector_triplet[k],opt->evict_matrix[l],n,P,zz_p::ll_red_struct());
        }
    }
    pthread_exit((void*)opt);
}


/**
 * Function Name: thread_loadRetrievalData_func
 *
 * Description: Threaded load function to read buckets in a path from disk storage
 * 
 */  
void* ServerS3ORAM::thread_loadRetrievalData_func(void* args)
{
   
}


/**
 * Function Name: thread_loadTripletData_func
 *
 * Description: Threaded load function to read triplet buckets from disk storage
 * 
 */  
void* ServerS3ORAM::thread_loadTripletData_func(void* args)
{
}




