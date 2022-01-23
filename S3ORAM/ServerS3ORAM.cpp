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
	this->stashIndex_buffer_in = new unsigned char[sizeof(TYPE_INDEX)];
	this->stash_buffer_out = new unsigned char[sizeof(TYPE_DATA)*DATA_CHUNKS*STASH];
	this->stash_buffer_in = new unsigned char[sizeof(TYPE_DATA)*DATA_CHUNKS*STASH];
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
				this->retrieve(socket);
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
				this->recvBlock(socket);
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
        fwrite(block_buffer_in, 1, sizeof(TYPE_DATA)*DATA_CHUNKS, file_out);
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
	memset(server_logs, 0, sizeof(unsigned long int)*13);
	
	int ret = 1;
	
	auto start = time_now;
	socket.recv(stashIndex_buffer_in,sizeof(TYPE_INDEX),0);
	auto end = time_now;
	cout<< "	[SendBlock] receive stash index in " << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() << " ns" <<endl;
    server_logs[0] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
	
	TYPE_INDEX stashIndex;
	memcpy(&stashIndex, stashIndex_buffer_in, sizeof(stashIndex));
	
    
    S3ORAM ORAM;
	TYPE_INDEX subSetSequenceIdx[STASH];
    ORAM.subSetSequenceIdx(subSetSequenceIdx, stashIndex);
	
    //use thread to load data from files
    start = time_now;
	ServerS3ORAM::loadRetrievalData_func(subSetSequenceIdx, stash_buffer_out);
    end = time_now;
	long load_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    cout<< "	[SendBlock] subSet Sequence blocks READ from Disk in " << load_time << " ns"<<endl;
    server_logs[1] = load_time;
    
    start = time_now;
    cout<< "	[SendBlock] Sending Block Stash with ID-" << stashIndex <<endl;
    socket.send(stash_buffer_out,sizeof(TYPE_DATA)*DATA_CHUNKS*STASH);
    end = time_now;
    cout<< "	[SendBlock] Stash SENT in " << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() <<endl;
    server_logs[2] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    ret = 0;
    return ret ;
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
	cout<< "	[recvBlock] Receiving Stash Index..." <<endl;
	TYPE_INDEX stash_index;
	auto start = time_now;
	socket.recv((unsigned char*)&stash_index, sizeof(TYPE_INDEX), 0);
	auto end = time_now;
	cout<< "	[recvBlock] Stash Index RECV in " << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() <<endl;
    server_logs[3] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    
	//send a Ok to client
	socket.send((unsigned char*)CMD_SUCCESS,sizeof(CMD_SUCCESS));
	
	//receive the other command named CMD_SEND_BLOCK
	int CMD;
	unsigned char buffer[sizeof(CMD)];
	cout<< "[Server] Waiting for a Command..." <<endl;
	socket.recv(buffer,sizeof(CMD));
		
	memcpy(&CMD, buffer, sizeof(CMD));
	cout<< "[Server] Command RECEIVED!" <<endl;
		
	socket.send((unsigned char*)CMD_SUCCESS,sizeof(CMD_SUCCESS));
	
	// update the blocks in server 
	start = time_now;
    socket.recv(stash_buffer_in, sizeof(TYPE_DATA)*DATA_CHUNKS*STASH, 0);
	cout<< "[Server] Stash RECEIVED!" <<endl;
	end = time_now;
	cout<< "	[recvStash] Stash Receieved in " << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() <<endl;
	server_logs[4] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
	// store the stash into disk
	start = time_now;
    FILE *file_update;
    string path = rootPath + to_string(0) + "/0";
    if((file_update = fopen(path.c_str(),"r+b")) == NULL)
    {
        cout<< "	[recvBlock] File Cannot be Opened!!" <<endl;
        exit(0);
    }
    for(int u = 0 ; u < STASH; u++)
    {
		fseek(file_update, (stash_index + u*STEP)*BLOCK_SIZE, SEEK_SET);
        fwrite(stash_buffer_in,DATA_CHUNKS ,sizeof(TYPE_DATA),file_update);
    }
    fclose(file_update);
    end = time_now;
	cout<< "[recvBlock] Stash STORED in Disk in " << std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count() <<endl;
	server_logs[5] = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
	
    socket.send((unsigned char*)CMD_SUCCESS,sizeof(CMD_SUCCESS));
	cout<< "	[recvStash] ACK is SENT!" <<endl;
    Utils::write_list_to_file(to_string(HEIGHT) + "_" + to_string(BLOCK_SIZE) + "_server" + to_string(0)+ "_" + timestamp + ".txt",logDir, server_logs, 13);
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
void* ServerS3ORAM::loadRetrievalData_func(TYPE_INDEX* sss, unsigned char* sbo)
{

    unsigned long int load_time = 0;
    FILE* file_in = NULL;
    string path = rootPath + to_string(0) + "/" + to_string(0);
	if((file_in = fopen(path.c_str(),"rb")) == NULL){
            cout<< "	[SendBlock] File cannot be opened!!" <<endl;
            exit;
        }
	for (int i = 0; i < STASH; i++){
		fseek(file_in, BLOCK_SIZE*sss[i],SEEK_SET);
		fread(sbo+i*BLOCK_SIZE,DATA_CHUNKS,sizeof(TYPE_DATA),file_in);
	}
        fclose(file_in);
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




