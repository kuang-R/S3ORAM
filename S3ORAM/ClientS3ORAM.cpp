/*
 * ClientS3ORAM.cpp
 *
 *  Created on: Mar 15, 2017
 *      Author: ceyhunozkaptan, thanghoang
 */

#include "ClientS3ORAM.hpp"
#include "Utils.hpp"
#include "S3ORAM.hpp"

using namespace std;

unsigned long int ClientS3ORAM::exp_logs[9];
unsigned long int ClientS3ORAM::thread_max = 0;
char ClientS3ORAM::timestamp[16];

ClientS3ORAM::ClientS3ORAM()
{
	this->pos_map = new vector<TYPE_INDEX>(NStore);
    
	this->stash = new vector<TYPE_DATA *>(STASH);
	for (int i = 0; i < STASH; i++)
		(*this->stash)[i] = new TYPE_DATA[DATA_CHUNKS];
		
	this->data_cache = new vector<TYPE_DATA *>(DATA_CACHE);
	for (int i = 0; i < DATA_CACHE; i++)
		(*this->data_cache)[i] = new TYPE_DATA[DATA_CHUNKS];
    
    this->vector_buffer_out = new unsigned char*[NUM_SERVERS];
    for (TYPE_INDEX i = 0 ; i < NUM_SERVERS ; i++)
    {
        this->vector_buffer_out[i] = new unsigned char[sizeof(TYPE_INDEX)+(H+1)*BUCKET_SIZE*sizeof(TYPE_DATA)]; 
    }
    
    this->block_buffer_out = new unsigned char*[NUM_SERVERS];
    for (int i = 0 ; i < NUM_SERVERS; i ++)
    {
        this->block_buffer_out[i]= new unsigned char[sizeof(TYPE_DATA)*DATA_CHUNKS+sizeof(TYPE_INDEX)];
        memset(this->block_buffer_out[i], 0, sizeof(TYPE_DATA)*DATA_CHUNKS+sizeof(TYPE_INDEX) );
    }
	
	time_t now = time(0);
	char* dt = ctime(&now);
	FILE* file_out = NULL;
	string path = clientLocalDir + "lastest_config";
	string info = "Number of Blocks: " + to_string(NStore) + "\n";
	info += "Block Size (B): " + to_string(BLOCK_SIZE) + "\n";
	info += "Number of Chunks: " + to_string(DATA_CHUNKS) + "\n";
	info += "Total Size of Data (MB): " + to_string((N*BLOCK_SIZE)/1048576.0) + "\n";
	info += "Total Size of ORAM (MB): " + to_string(NStore*BLOCK_SIZE/1048576.0) + "\n";
	
	if((file_out = fopen(path.c_str(),"w+")) == NULL){
		cout<< "	File Cannot be Opened!!" <<endl;
		exit;
	}
	fputs(dt, file_out);
	fputs(info.c_str(), file_out);
	fclose(file_out);
	
	tm *now_time = localtime(&now);
	if(now != -1)
		strftime(timestamp,16,"%d%m_%H%M",now_time);
		
}

ClientS3ORAM::~ClientS3ORAM()
{
}


/**
 * Function Name: init
 *
 * Description: Initialize shared ORAM data on disk storage of the client
 * and creates logging and configuration files
 * 
 * @return 0 if successful
 */ 
int ClientS3ORAM::init()
{
    
    auto start = time_now;
    auto end = time_now;
	
    start = time_now;
    S3ORAM ORAM;
    ORAM.build(this->pos_map);
    end = time_now;
	
	cout<<endl;
    cout<< "Elapsed Time for Setup on Disk: "<<std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()<<" ns"<<endl;
    cout<<endl;
    std::ofstream output;
    string path2 = clientLocalDir + "lastest_config";
    output.open(path2, std::ios_base::app);
    output<< "INITIALIZATION ON CLIENT: Performed\n";
    output.close();
	
	FILE* local_data = NULL;
	if((local_data = fopen(clientTempPath.c_str(),"wb+")) == NULL){
		cout<< "	[init] File Cannot be Opened!!" <<endl;
		exit(0);
	}
	fwrite(this->pos_map->data(), NStore, sizeof(TYPE_INDEX), local_data);
	fclose(local_data);
	
    return 0;
}


/**
 * Function Name: load
 *
 * Description: Loads client storage data from disk for previously generated ORAM structure 
 * in order to continue ORAM operations. Loaded data includes postion map, current number of evictions,
 * current number of reads/writes.
 * 
 * @return 0 if successful
 */ 
int ClientS3ORAM::load()
{
	
    return 0;
}


/**
 * Function Name: sendORAMTree
 *
 * Description: Distributes generated and shared ORAM buckets to servers over network
 * 
 * @return 0 if successful
 */  
int ClientS3ORAM::sendORAMTree()
{
    unsigned char*  block_buffer_out = new unsigned char [BUCKET_SIZE*sizeof(TYPE_DATA)*DATA_CHUNKS]; 
    memset(block_buffer_out,0, sizeof(TYPE_DATA)*DATA_CHUNKS);
    int CMD = CMD_SEND_ORAM_TREE;       
    unsigned char buffer_in[sizeof(CMD_SUCCESS)];
	unsigned char buffer_out[sizeof(CMD)];

    memcpy(buffer_out, &CMD,sizeof(CMD));
    
    zmq::context_t context(1);
    zmq::socket_t socket(context,ZMQ_REQ);

    struct_socket thread_args[NUM_SERVERS];
    {
//        string ADDR = SERVER_ADDR[i]+ ":" + SERVER_PORT[i*NUM_SERVERS+i]; 
		string ADDR = SERVER_ADDR[0]+ ":" + std::to_string(SERVER_PORT+0*NUM_SERVERS+0); 
		cout<< "	[sendORAMTree] Connecting to " << ADDR <<endl;
        socket.connect( ADDR.c_str());
            
        socket.send(buffer_out, sizeof(CMD));
		cout<< "	[sendORAMTree] Command SENT! " << CMD <<endl;
        socket.recv(buffer_in, sizeof(CMD_SUCCESS));
		
		FILE* fdata = NULL;
		string path = clientDataDir + to_string(0);
		if((fdata = fopen(path.c_str(),"rb")) == NULL)
		{
			cout<< "	[sendORAMTree] File Cannot be Opened!!" <<endl;
			exit(0);
		}
        for(TYPE_INDEX j = 0 ; j < NStore; j++)
        {
            //load data to buffer
            if(fread(block_buffer_out ,1 , sizeof(TYPE_DATA)*DATA_CHUNKS, fdata) != sizeof(TYPE_DATA)*DATA_CHUNKS){
                cout<< "	[sendORAMTree] File loading error be Read!!" <<endl;
                exit(0);
            }
            //send to server 
            socket.send(block_buffer_out, sizeof(TYPE_DATA)*DATA_CHUNKS, 0);
			socket.recv(buffer_in,sizeof(CMD_SUCCESS));
        }
		
		fclose(fdata);
        socket.disconnect(ADDR.c_str());
    }
    socket.close();	
    return 0;
}


/**
 * Function Name: access
 *
 * Description: Starts access operation for a block with its ID to be retrived from distributed servers. 
 * This operations consists of several subroutines: generating shares for logical access vector, 
 * retrieving shares from servers, recovering secret block from shares, assigning new path for the block,
 * re-share/upload the block back to servers, run eviction subroutine acc. to EVICT_RATE
 * 
 * @param blockID: (input) ID of the block to be retrieved
 * @return 0 if successful
 */  
int ClientS3ORAM::access(TYPE_ID blockID)
{
	
	return 0;
}


/**
 * Function Name: getLogicalVector
 *
 * Description: Generates logical retrieve vector by putting '1' for the exact index of 
 * accessed block and '0' for the rest on its assigned path
 * 
 * @param logicalVector: (output) Logical retrieve vector to retrive the block.
 * @param blockID: (input) ID of the block to be retrieved.
 * @return 0 if successful
 */  
int ClientS3ORAM::getLogicalVector(TYPE_DATA* logicalVector, TYPE_ID blockID)
{
	
	
	return 0;
}


/**
 * Function Name: getEvictMatrix
 *
 * Description: Generates logical eviction matrix to evict blocks from root to leaves according to 
 * eviction number and source, destination and sibling buckets by scanning position map.
 * 
 * @param evictMatrix: (output) Logical eviction matrix for eviction routine
 * @param n_evict: (input) Eviction number
 * @return 0 if successful
 */  
int ClientS3ORAM::getEvictMatrix(TYPE_DATA** evictMatrix, TYPE_INDEX n_evict)
{
	
	return 0;
}
 
 
/**
 * Function Name: thread_socket_func & send
 *
 * Description: Generic threaded socket function for send and receive operations
 * 
 * @return 0 if successful
 */  
void* ClientS3ORAM::thread_socket_func(void* args)
{
	struct_socket* opt = (struct_socket*) args;
	
	
	sendNrecv(opt->ADDR, opt->data_out, opt->data_out_size, opt->data_in, opt->data_in_size, opt->CMD);

		
    pthread_exit((void*)opt);
}
int ClientS3ORAM::sendNrecv(std::string ADDR, unsigned char* data_out, size_t data_out_size, unsigned char* data_in, size_t data_in_size, int CMD)
{
	zmq::context_t context(1);
    zmq::socket_t socket(context,ZMQ_REQ);
    socket.connect(ADDR.c_str());
	
    unsigned char buffer_in[sizeof(CMD_SUCCESS)];
	unsigned char buffer_out[sizeof(CMD)];
	
    try
    {
        cout<< "	[ThreadSocket] Sending Command to"<< ADDR << endl;
        memcpy(buffer_out, &CMD,sizeof(CMD));
        socket.send(buffer_out, sizeof(CMD));
		cout<< "	[ThreadSocket] Command SENT! " << CMD <<endl;
        socket.recv(buffer_in, sizeof(CMD_SUCCESS));
		
		auto start = time_now;
		cout<< "	[ThreadSocket] Sending Data..." << endl;
		socket.send (data_out, data_out_size);
		cout<< "	[ThreadSocket] Data SENT!" << endl;
        if(data_in_size == 0)
            socket.recv(buffer_in,sizeof(CMD_SUCCESS));
        else
            socket.recv(data_in,data_in_size);
            
		auto end = time_now;
		if(thread_max < std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count())
			thread_max = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
	}
    catch (exception &ex)
    {
        cout<< "	[ThreadSocket] Socket error!"<<endl;
		exit(0);
    }
	socket.disconnect(ADDR.c_str());
	return 0;
}
