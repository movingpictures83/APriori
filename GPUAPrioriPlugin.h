#include "Plugin.h"
#include "PluginProxy.h"
#include <string>
#include <map>

#define THREADS_PER_BLOCK 1024
#define MAX_NUMBER_BLOCKS 2496


class GPUAPrioriPlugin : public Plugin {

	public:
		void input(std::string file);
		void run();
		void output(std::string file);
	private:
                std::string inputfile;
		std::string outputfile;
		float* a;
		int N;
                std::map<std::string, std::string> parameters;
	/************************************************************************************
	*                                  Variable Declarations                            *
	************************************************************************************/	
	FILE *fPointer;
	int max = 0; 
	int size = 0; //Contains the number of lines in the given database
	int cardinality = 1; //Contains the initial cardinality of the item sets
    	int temp;
	int i = 0;
	int j, k, num, count;
	int mSupport = 8000; //Contains the support count; set to approx 10% of all transactions
	int numBlocks = 0;
       char* cTable;	
       int* fTable;
       char* gpuT;

};

__global__ void validSets(int* fTable, int cardinality, int nCr, int mSupport){
	int tIndex = blockIdx.x * blockDim.x + threadIdx.x;
	if((tIndex < (cardinality + 1) * nCr) && (tIndex % (cardinality + 1) == cardinality)){
		if(fTable[tIndex] < mSupport){
			fTable[tIndex] = 0;
		}
	}
}

__global__ void counting(int* fTable, char* tTable, int row, int col, int nCr, int cardinality){
	
	__shared__ int cache[THREADS_PER_BLOCK]; //cache memory that is shared by all the threads within a block
	int bIndex = blockIdx.x; //the index value of the core
	int cacheIndex = threadIdx.x; //each thread within a core has a corresponding cache index where it stores its values

	//enter a block loop where the core index must remain lower than the amount of item sets present in the frequency table
	//at the end of each iteration the core index is increased by the amount of cores being used and loops again if possible
	for(int h = bIndex; h < nCr; h+= gridDim.x){
		
		int tIndex = threadIdx.x; //the index value of the individual thread
		int sum = 0; //keeps track of how many times an item set has been found
		int found; //a boolean value that indicates whether an item set is present within a transaction; either 0 or 1

		//enter a thread loop where i represents which transaction being scanned. Each thread within a core scans a
		// different transaction; the loop is necessary since there aren't enough threads for each transaction. Whenever
		// a scan is done i is incremented by th number of threads per block
		for(int i = tIndex; i < row; i+= blockDim.x){

			found = 1;

			//enter a loop where j represents the specific item within an item set; the iterations within the for loop
			// is dependent on the cardinality of the item sets
			for(int j = 0; j < cardinality; j++){
				
				//if an item indicated in the frequency table is not found in the transaction found is set to 0; i.e. false
				if(tTable[i * col + (fTable[bIndex * (cardinality + 1) + j])] != '1'){
					found = 0;
				}
			}	

			//if found equals 1 then the sum variable is incremented by 1
			if(found == 1){
				sum++;
			}	
		}
		
		//once any given thread exits the thread the thread loop it stores its sum value to its corresponding cache index 
		cache[cacheIndex] = sum;
		
		//the threads are synced before the overall sum is calculated to ensure all threads have finished counting;
		__syncthreads();

		//the cache is then reduced to obtain the total sum for any given item set every iteration adds two cache location 
		//together until the sum is stored at cache[0]
		int k = THREADS_PER_BLOCK/2;
		while(k != 0){
			if(cacheIndex < k){
				cache[cacheIndex] += cache[cacheIndex + k];
			}
			__syncthreads();
			k /= 2;
		}

		//takes the overall of the item set for the core index that is monitoring this specific item set and enters it into the 
		//corresponding count column within the frequency table
		if(cacheIndex == 0){
			fTable[bIndex * (cardinality + 1) + cardinality] = cache[0];
		}
		__syncthreads();
		//the core index value is incremented by the number of cores being used
		bIndex += gridDim.x;
	}
}

