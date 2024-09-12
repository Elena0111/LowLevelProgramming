
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



typedef enum { dm, fa } cache_map_t;
typedef enum { uc, sc } cache_org_t;
typedef enum { instruction, data } access_t;

typedef struct {
  uint32_t tag;
  uint32_t offset;
  int valid;
} block;

typedef struct {
   block* Icache;
   block* Dcache; 
} split_cache;

typedef struct {
  uint32_t address;
  access_t accesstype;
} mem_access_t;


typedef struct {
  uint64_t accesses;
  uint64_t hits;
  //added:
  uint64_t misses;
  uint64_t evicts;

  uint64_t D_hits;
  uint64_t D_access;
  uint64_t D_evicts;

  uint64_t I_hits;
  uint64_t I_access;
  uint64_t I_evicts;
  
} cache_stat_t;




uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

int offset_bits;
int index_bits;
int address_size = 32;
int tag_bits;

//Index can't be negative
uint32_t index_d=0;
uint32_t index_i = 0;
uint32_t index_u = 0;


//Function to get the offset from the address
int get_offset(int offset_bits, uint32_t address){
  //in this way we obtain a number of 1 bits equal to the offset lenght, that can be used as bitmask
  uint32_t bit_mask = (1 << offset_bits) - 1;
  //using and with each bit, we keep only the the bits corresponding to the offset
  uint32_t offset = address & bit_mask;
  return offset;
}

//Function to get the tag from the address
int get_tag(int tag_bits, uint32_t address, int offset_bits, int index_bits){ 
  //in this way we obtain a number of 1 bits equal to the tag lenght, that can be used as bitmask
  uint32_t bit_mask = (1 << tag_bits) - 1;
  //using and with each bit, we keep only the the bits corresponding to the tag
  uint32_t tag = (address >> (offset_bits + index_bits)) & bit_mask;
  return tag;
}

//Function to get the index from the address
uint32_t get_index(uint32_t cache_size, uint32_t address, int offset_bits){ 
  //in this way we obtain a number of 1 bits equal to the index lenght, that can be used as bitmask
   uint32_t bit_mask = (1 << (index_bits)) - 1;
   //using and with each bit, we keep only the the bits corresponding to the index
   uint32_t index = (address >> offset_bits) & bit_mask;
   return index;
}



cache_stat_t cache_statistics;


mem_access_t read_transaction(FILE* ptr_file) {
  char type;
  mem_access_t access;

  if (fscanf(ptr_file, "%c %x\n", &type, &access.address) == 2) {
    if (type != 'I' && type != 'D') {
      printf("Unkown access type\n");
      exit(0);
    }
    access.accesstype = (type == 'I') ? instruction : data;
    return access;
  }

  /* If there are no more entries in the faile,
   * return an address 0 that will terminate the infinite loop in main
   */
  access.address = 0;
  return access;
}

//Initialize all the valid bits equal to 0 and all the tags to zero
void set_valid(block cache[], uint32_t cache_size){
  
    for (int i = 0; i < cache_size/block_size; i++){
      cache[i].valid = 0;
      
   }
   
   for (int i = 0; i < cache_size/block_size; i++){
      cache[i].tag = 0;
      
   }

}

//Initialise the split cache allocating memory to instruction and data caches, and setting the valid bits to 0
void init_split_cache(split_cache* cache, uint32_t cache_size){
 
  cache->Icache = (block*)malloc((cache_size/block_size) * sizeof(block));
  cache->Dcache = (block*)malloc((cache_size/block_size) * sizeof(block));
  for(int i=0; i<cache_size/block_size; i++){
    set_valid(cache->Dcache, cache_size);
    set_valid(cache->Icache, cache_size);
  }
}


//Updating the evicts statistics
void set_evicts(mem_access_t access){
  
  if(access.accesstype == instruction && cache_org == sc){
    cache_statistics.I_evicts++;
  }else if (access.accesstype == data && cache_org == sc){
    cache_statistics.D_evicts++;
  }else{
    
    cache_statistics.evicts++;
    
  }
}


//Function that handles the direct mapped cache access
int dm_access(mem_access_t access, block *cache, uint32_t cache_size, uint32_t index, uint32_t tag){

//Checks if the data's tag is the same as the tag stored in the cache and if the data is valid. In this case, it returns 1
 if ((cache[index].tag == tag)  && (cache[index].valid == 1)){
        return 1;
      }else{
        //Handle misses: updates the evicts from the cache
          if(cache[index].valid == 1){
            set_evicts(access);   
          }
          //Overwrites the tag and set the valid bit to 1
          cache[index].tag = tag;
          cache[index].valid = 1;
          return 0;
          
     }
}


//Checks if the index is not negative
void check_index(uint32_t index){
  if(index < 0 ){
    printf("\nIndex cannot be negative\n");
    exit(0);
  }
}

//Return the global variable index corresponding to the specified cache
uint32_t set_index(mem_access_t access){
 if(access.accesstype == instruction && cache_org == sc){
    return index_i;
  }else if (access.accesstype == data && cache_org == sc){
    return index_d;
  }else{
   return index_u;
  }
} 


//Update the value of the index corresponding to the cache organization (unified or split) and in the latter case, the access type
void update_index(uint32_t index, mem_access_t access){

  if(access.accesstype == instruction && cache_org == sc){
    index_i = index;
  }else if (access.accesstype == data && cache_org == sc){
    index_d= index;
  }else{
   index_u = index;
  }

}

//This function returns 1 if the access to the fully associative cache is a hit, 0 if it is a miss
int fa_access(mem_access_t access, block* cache, uint32_t cache_size, uint32_t tag){

 uint32_t last_index = set_index(access);
 
 //Checks if the data's tag is the same as the tag stored in the cache and if the data is valid. In this case, it returns 1
 for (int i = 0; i < cache_size/block_size; i++){
        if((cache[i].tag == tag) && (cache[i].valid == 1)){     
          return 1;
          break;       
  }
 }
 //For loop to check if there is any invalid block available to store the data. If so, it also updates the index of the last inserted data
  for (int j = 0; j < cache_size/block_size; j++){
      
        if(cache[j].valid == 0){
          cache[j].valid = 1;
          cache[j].tag = tag;
          //store the index of the last inserted data
          last_index = (uint32_t) j;
          update_index(last_index, access);
          return 0;
          break;
         
     }
  }

  //If we are here it means there are all the blocks are full, so we need to evict the first data inserted.
  //It also updates the index of the last inserted data, sets the valid bit to 1 and updates the evicts statistics.

  //Since we access the blocks array always sequentially, the last inserted data, that we need to evicts according to FIFO, is the one next to the last inserted data index     
  last_index = (last_index +1) % (cache_size/block_size);
  update_index(last_index, access);
            
  cache[last_index].valid = 1;
  cache[last_index].tag = tag;
  set_evicts(access);
  return 0;
  }
       
  
  
void main(int argc, char** argv) {
  // Reset statistics:
  memset(&cache_statistics, 0, sizeof(cache_stat_t));



  if (argc != 4) { /* argc should be 2 for correct execution */
    printf(
        "Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] "
        "[cache organization: uc|sc]\n");
    exit(0);
  } else {
    /* argv[0] is program name, parameters start with argv[1] */

   /* Set cache size */
   cache_size = atoi(argv[1]);
   
    /* Set Cache Mapping */
    if (strcmp(argv[2], "dm") == 0) {
      cache_mapping = dm;
      
    } else if (strcmp(argv[2], "fa") == 0) {

      cache_mapping = fa;

    } else {

      printf("Unknown cache mapping\n");

      exit(0);
    }

    /* Set Cache Organization */
    if (strcmp(argv[3], "uc") == 0) {
      cache_org = uc;

    } else if (strcmp(argv[3], "sc") == 0) {

      cache_org = sc;

    } else {
      printf("Unknown cache organization\n");
      exit(0);
    } 
}
  
  /* Open the file mem_trace.txt to read memory accesses */
  FILE* ptr_file;
  ptr_file = fopen("mem_trace.txt", "r");
  if (!ptr_file) {
    printf("Unable to open the trace file\n");
    exit(1);
  }

  
  block* cache = NULL;
  split_cache* s_cache = NULL;
  
  //sets the cache size depending on the type of cache
  

  if(cache_org==sc){
//if it's a split cache, we initialize che cache_size with half of its space
    cache_size = cache_size/2;

    s_cache = (split_cache*)malloc(sizeof(split_cache));

    init_split_cache(s_cache, cache_size);

  }else if(cache_org==uc){

     cache = (block*)calloc((cache_size),sizeof(block));


     set_valid(cache, cache_size/block_size);
  }
 
  mem_access_t access;
  //int last_index = 0;

  while (1) {
    access = read_transaction(ptr_file);
    // If no transactions left, break out of loop
     
    if (access.address == 0) break;
     
    /* Do a cache access */
    // ADD YOUR CODE HERE
   
   //Compute the number of bits for offset, index and tag
    offset_bits = log2(block_size);
    index_bits = log2(cache_size / block_size);
    tag_bits = (int) address_size - (int)index_bits - (int) offset_bits;
   
  
    cache_statistics.accesses = cache_statistics.accesses +1;


  //Collect the statistics related to the access if it's  direct mapped cache   
  if (cache_mapping == dm){

    uint32_t offset = get_offset(offset_bits, access.address);
   
    uint32_t index =get_index(cache_size, access.address, offset_bits);
    
    uint32_t tag = get_tag(tag_bits, access.address, offset_bits, index_bits);
    
     
  //Collect the statistics if it's a split cache and the access type is an instruction
  if(cache_org == sc && access.accesstype == instruction){

  //this method dm_access return 0 if the access results in a miss, otherwise return 1
      int access_instr = dm_access(access, s_cache->Icache, cache_size, index, tag);
     //updates the statistics
      cache_statistics.hits += access_instr;
      cache_statistics.I_hits += access_instr;
      cache_statistics.I_access = cache_statistics.I_access +1;
      
  //Collect the statistics if it's a split cache and the access type is data    
  }else if(cache_org == sc && access.accesstype == data){

  
      int access_data = dm_access(access, s_cache->Dcache, cache_size, index, tag);
       //updates the statistics
      cache_statistics.hits = cache_statistics.hits + access_data;
      cache_statistics.D_hits = cache_statistics.D_hits + access_data;
      cache_statistics.D_access = cache_statistics.D_access +1;

  }else if(cache_org == uc){
      //Collect the statistics if it's a unified cache
      int access_data = dm_access(access, cache, cache_size, index, tag);
      cache_statistics.hits = cache_statistics.hits + access_data;
     
  }

 //Collect the statistics related to the access if it's a fully associative cache  
 
  }else if (cache_mapping == fa){
   //Since it's fully associative the index becomes part of the tag
   //Therefore, index bits, offset bits and tag bits mantain always the same value 
       
      
    uint32_t offset = get_offset(6, access.address);
  
    uint32_t tag = get_tag(26, access.address, 6, 0);
  //Updates the statistics

    if(cache_org == sc && access.accesstype == instruction){

      int access_instr = fa_access(access, s_cache->Icache, cache_size, tag);

      cache_statistics.hits = cache_statistics.hits + access_instr;
      cache_statistics.I_hits = cache_statistics.I_hits + access_instr;
      cache_statistics.I_access = cache_statistics.I_access +1;

    }else if(cache_org == sc && access.accesstype == data){

      int access_data = fa_access(access, s_cache->Dcache, cache_size, tag);

      cache_statistics.hits = cache_statistics.hits + access_data;
      cache_statistics.D_hits = cache_statistics.D_hits + access_data;
      cache_statistics.D_access = cache_statistics.D_access +1;

    }else if(cache_org == uc){
      
      int access_data = fa_access(access, cache, cache_size, tag);
      cache_statistics.hits = cache_statistics.hits + access_data;
    }
  

    }
    
  }

//Free the memory allocated to the cache
if(cache_org==sc){
  free(s_cache->Icache);
  free(s_cache->Dcache);
  free(s_cache);
}else{
  free(cache);
}

 
  printf("\nCache Statistics\n");
  printf("-----------------\n\n");
  printf("Accesses: %ld\n", cache_statistics.accesses);
  printf("Hits:     %ld\n", cache_statistics.hits);
  printf("Hit Rate: %.4f\n",
         (double)cache_statistics.hits / cache_statistics.accesses);

        
 
  cache_statistics.misses = cache_statistics.accesses - cache_statistics.hits;
  printf("Evicts unified cache: %ld\n", cache_statistics.evicts);

  printf("DCache Accesses: %ld\n", cache_statistics.D_access);
  printf("DCache Hits:     %ld\n", cache_statistics.D_hits);
  printf("Evicts: %ld\n", cache_statistics.D_evicts);
  printf("ICache Accesses: %ld\n", cache_statistics.I_access);
  printf("ICache Hits:     %ld\n", cache_statistics.I_hits);
  printf("Evicts: %ld\n", cache_statistics.I_evicts);       
   
  printf("\n Misses: %ld", cache_statistics.misses);
  

  /* Close the trace file */
  fclose(ptr_file);

}
