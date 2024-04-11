/*
 * CacheFS.cpp
 *
 *  Author: Netanel Zakay, Operating Systems course 2016-2017, HUJI
 */

#include <cstdio>
#include <fcgi_stdio.h>
#include <vector>
#include <sys/stat.h>
#include "CacheFS.h"
#include <iostream>
#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <map>
#include <math.h>
#include <malloc.h>



// This enum represents a cache algorithm.
// The possible values are all the cache algorithms that the library supports.

cache_algo_t Cache_Algo;


using namespace std;


#define FAIL -1
#define SUCCESS 0
#define ZERO_BYTES 0;

typedef struct CacheState {
    ofstream logfile;
    int blockSize;
    int numberOfBlocks;
    double fOld;
    double fNew;

} CacheState ;

// maintain our file system state in here;

/**
 * blocks structure;
 */
typedef struct block{
    char* data;
    char* fileName;
    int blockId;
    int accessCounter = 0;
    int blockStart;
    int blockEnd;
    int blockIndex;
    int lruCnt ;
} Block;

/**
 * cache memory structure;
 */
typedef struct CacheMemory{
    vector <Block*> blockVec;
} CacheMemory;


CacheMemory CacheFS;
CacheState cacheState;


std::vector <pair<int,int>> lruBlocksVec;
std::map <string,vector <int> > fdFileName;
std::map <int,vector <int> > fdBlocks;


int cacheIsEmpty = 1;
int blockIndex=0;
int leastUsedCnt = 0;


int Hits_number = 0;
int Miss_number = 0;



/**
 Initializes the CacheFS.
 Assumptions:
	1. CacheFS_init will be called before any other function.
	2. CacheFS_init might be called multiple times, but only with CacheFS_destroy
  	   between them.

 Parameters:
	blocks_num   - the number of blocks in the buffer cache
	cache_algo   - the cache algorithm that will be used
    f_old        - the percentage of blocks in the old partition (rounding down)
				   relevant in FBR algorithm only
    f_new        - the percentage of blocks in the new partition (rounding down)
				   relevant in FBR algorithm only
 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. new).
		2. invalid parameters.
	Invalid parameters are:
		1. blocks_num is invalid if it's not a positive number (zero is invalid too).
		2. f_old is invalid if it is not a number between 0 to 1 or
		   if the size of the partition of the old blocks is not positive.
		3. fNew is invalid if it is not a number between 0 to 1 or
		   if the size of the partition of the new blocks is not positive.
		4. Also, fOld and fNew are invalid if the fOld+fNew is bigger than 1.

		Pay attention: bullets 2-4 are relevant (and should be checked)
		only if cache_algo is FBR.

 For example:
 CacheFS_init(100, FBR, 0.3333, 0.5)
 Initializes a CacheFS that uses FBR to manage the cache.
 The cache contains 100 blocks, 33 blocks in the old partition,
 50 in the new partition, and the remaining 17 are in the middle partition.
 */
int CacheFS_init(int blocks_num, cache_algo_t cache_algo, double f_old , double f_new  )
{

    if(!cacheIsEmpty)
    {
        return FAIL;
    }
    if(blocks_num <= 0)
    {
        return FAIL;
    }
    if(f_old > 1 || f_old < 0)
    {
        return FAIL;
    }
    if(cache_algo != LRU && cache_algo != LFU && cache_algo != FBR)
    {
        return FAIL;
    }
    Cache_Algo = cache_algo;

    if(cache_algo == FBR)
    {
        if(f_old + f_new <= 1)
        {
            cacheState.fNew = f_new;
            cacheState.fOld = f_old;
        }
        else
        {
            return FAIL;
        }
    }


    struct stat fi;
    stat("/tmp", &fi);
    int blksize = fi.st_blksize;

    if(blksize <= 0)
    {
        return FAIL;
    }
    cacheState.blockSize = blksize;
    cacheState.numberOfBlocks = blocks_num;
    cacheIsEmpty = 0;
    return SUCCESS;

}

/**
 * A function that frees the block and all its data.
 * @param block_index the index of the block in the blockVec.
 */
void freeBlock(int block_index)
{
    Block * b;
    for (std::vector<block*>::iterator it = CacheFS.blockVec.begin() ; it != CacheFS.blockVec.end(); ++it)
    {
        if((*it)->blockId == block_index)
        {
            b = (*it);
            CacheFS.blockVec.erase(it);
            free(b->data);
            delete [](b->fileName);
            delete (b);
            break;
        }
    }

}


/**
 Destroys the CacheFS.
 This function releases all the allocated resources by the library.

 Assumptions:
	1. CacheFS_destroy will be called only after CacheFS_init (one destroy per one init).
	2. After CacheFS_destroy is called,
	   the next CacheFS's function that will be called is CacheFS_init.
	3. CacheFS_destroy is called only after all the open files already closed.
	   In other words, it's the user responsibility to close the files before destroying
	   the CacheFS.

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail if a system call or a library function fails.
*/
int CacheFS_destroy()
{
    if(cacheIsEmpty)
    {
        cout << "entered destroy"<<endl;

        return FAIL;
    }
    if(!fdFileName.empty() || !fdFileName.empty())
    {
        return FAIL;
    }
    cacheState.logfile.close();
    Hits_number = 0;
    Miss_number = 0;
    for (std::vector<block*>::iterator it = CacheFS.blockVec.begin() ; it != CacheFS.blockVec.end(); ++it)
    {
        free((*it)->data);
        delete[]((*it)->fileName);
        delete ((*it));
    }
    CacheFS.blockVec.clear();
    cacheIsEmpty = 1;
    lruBlocksVec.clear();
    return SUCCESS;

}



/**
 * helper function that recives a path and checks if the path is legal and
 * that the path is under the folder tmp.
 * @param path the path to be checked.
 * @return -1 if the path illegal, 0 if legal.
 */
int checkPath(const char* path)
{
    if (!boost::filesystem::exists(path))// does filePath actually exist?
    {
        cout << "This is  invalid file" <<endl;
        return FAIL;
    }
    string filePath(path);
    std::size_t found = filePath.find("/tmp/");
    if (found==std::string::npos)
    {
        //return FAIL;
    }
    return SUCCESS;

}


/**
 File open operation.
 Receives a path for a file, opens it, and returns an id
 for accessing the file later

 Notes:
	1. You must open the file with the following flags: O_RDONLY | O_DIRECT | O_SYNC
	2. The same file might be opened multiple times.
	   Like in POISX, it's valid.
	3. The pathname is not unique per file, because:
		a. relative paths are not unique: "myFolder/../tmp" and "tmp".
		b. we might open a link ("short-cut") to the file

 Parameters:
    pathname - the path to the file that will be opened

 Returned value:
    In case of success:
		Non negative value represents the id of the file.
		This may be the file descriptor, or any id number that you wish to create.
		This id will be used later to read from the file and to close it.

 	In case of failure:
		Negative number.
		A failure will occur if:
			1. System call or library function fails (e.g. open).
			2. Invalid pathname. Pay attention that we support only files under
			   "/tmp" due to the use of NFS in the Aquarium.
 */
int CacheFS_open(const char *pathname)
{
    int legal = checkPath(pathname);
    if(legal < 0)
    {
        return FAIL;
    }
    int fd = open(pathname, O_RDONLY | O_SYNC);
    if (fd < 0)
    {
        return FAIL;
    }
    char* fPath = realpath((char*)pathname,NULL);
    string str(fPath);
    fdFileName[str].push_back(fd);
    fdBlocks[fd];

    std::vector <int> blocksVec;
    for(std::vector <int>:: iterator it = fdFileName[fPath].begin();it !=  fdFileName[fPath].end(); it ++ )
    {
        if(!fdBlocks[*it].empty())
        {
            blocksVec = fdBlocks[*it];
        }
    }
    fdBlocks[fd] = blocksVec;
    free(fPath);
    return fd;
}





/**
 File close operation.
 Receives id of a file, and closes it.

 Returned value:
	0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. a system call or a library function fails (e.g. close).
		2. invalid file_id. file_id is valid if"f it was returned by
		CacheFS_open, and it is not already closed.
 */
int CacheFS_close(int file_id)
{
    if(fdBlocks.count(file_id) <= 0)
    {
        return FAIL;
    }

    string fileName;
    for (std::map<string,vector<int>>::iterator it = fdFileName.begin(); it!=fdFileName.end(); ++it)
    {
        if(std::find(it->second.begin(), it->second.end(), file_id) != it->second.end())
        {
            fileName = it->first;
            for (std::vector<int>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2)
            {
                if(file_id == *it2){
                    fdBlocks.erase(*it2);
                    fdFileName[fileName].erase(it2);
                    break;
                }

            }
        }
    }

    if( fdFileName[fileName].empty()){
        fdFileName.erase(fileName);
    }


    close(file_id);
    return SUCCESS;
}

/**
 * get the file name by its id .
 * @param file_id  the id of the file .
 * @return
 */
char* getFileName(int file_id)
{
    string fileName;
    for (std::map<string,vector<int>>::iterator it = fdFileName.begin(); it!=fdFileName.end(); ++it)
    {
        if(std::find(it->second.begin(), it->second.end(), file_id) != it->second.end())
        {
            fileName = it->first;
            break;
        }
    }
    int len = fileName.length() + 1;
    char* final = new char [len];
    strcpy(final, fileName.c_str());
    return final;
}



/**
 * get total size of the file with the given file descriptor, in bytes.
 * @param fd the file descriptor of the file.
 * @return the size of the file in bytes, and -1 in case of ERR.
 */

/**
 * Checks if the file with the given id has been opened.
 * @param file_id id of the file
 * @return 0 if the file is open, else -1;
 */
int isFileOpen(int file_id)
{
    for (std::map<string,vector<int>>::iterator it = fdFileName.begin(); it!=fdFileName.end(); ++it)
    {
        if(std::find(it->second.begin(), it->second.end(), file_id) != it->second.end())
        {
            return 1;
        }
    }
    return FAIL;
}

/**
 * return the size of the file with the given fd.
 * @param fd the fd of the file
 * @return the total number of bytes of the file.
 */
int fileSize(int fd) {
    struct stat s;
    if (fstat(fd, &s) == -1) {
        return FAIL;
    }
    return(s.st_size);
}


/**
 *
 * @param block_id
 * @return
 */
Block* find_block(int block_id)
{
    for(std::vector<Block*>::iterator it = CacheFS.blockVec.begin(); it != CacheFS.blockVec.end(); ++it)
    {
        if((*it)->blockId == block_id)
        {
            return *it;
        }
    }

    return NULL;
}



void update_fdBlocks(int block_id)
{
    int counter = 0;
    for(map <int,vector <int> >::iterator it  = fdBlocks.begin(); it != fdBlocks.end(); it++)
    {
        for(vector <int> ::iterator it2 = it->second.begin() ; it2 != it->second.end(); it2++)
        {
            if(block_id == (*it2))
            {
                it->second.erase(it2);
                break;
            }
            counter ++;
        }
        counter = 0;
    }
}

int compare_by_lru(pair <int,int> a , pair <int,int> b)
{
    return a.second < b.second;
}


int compare_by_lfu(Block* a , Block* b)
{
    return a->accessCounter < b->accessCounter;
}



/**
 *
 * @param key
 * @return
 */
int return_index(int key)
{
    int counter = 0;
    for(std::vector<pair<int,int>>::iterator it = lruBlocksVec.begin(); it != lruBlocksVec.end(); ++it)
    {
        if(it->first == key)
        {
            return counter;
        }
        else
        {
            counter ++;
        }

    }
    return -1;
}


/**
 *
 */
void deleteBlock()
{
    int blockId;
    if(Cache_Algo == LRU)
    {
        sort(lruBlocksVec.begin(),lruBlocksVec.end(),compare_by_lru);
        if(!lruBlocksVec.empty()){
            blockId = lruBlocksVec[0].first;
            freeBlock(blockId);
            lruBlocksVec.erase(lruBlocksVec.begin());
            update_fdBlocks(blockId);
        } else{
            blockId = CacheFS.blockVec.at(0)->blockId;
            freeBlock(blockId);
            update_fdBlocks(blockId);
        }



    }
    else if (Cache_Algo == LFU)
    {
        sort(CacheFS.blockVec.begin(),CacheFS.blockVec.end(),compare_by_lfu);
        blockId = CacheFS.blockVec[0]->blockId;
        freeBlock(blockId);
        update_fdBlocks(blockId);

    }
    else
    {
        sort(lruBlocksVec.begin(),lruBlocksVec.end(),compare_by_lru);
        vector <Block*> oldP_vec;
        int old_p = cacheState.fOld * CacheFS.blockVec.size();
        for(int i = 0; i< old_p; i++)
        {
            oldP_vec.push_back(find_block(lruBlocksVec.at(i).first));
        }
        sort(oldP_vec.begin(),oldP_vec.end(),compare_by_lfu);
        blockId = oldP_vec[0]->blockId;
        freeBlock(blockId);
        update_fdBlocks(blockId);
        lruBlocksVec.erase(lruBlocksVec.begin() + return_index(blockId));

    }
}



/**
 * creates a new block and add it to the cache.
 * @param startBlock  number of bytes that represent where the block start.
 * @param endBlock number of bytes that represent where the block end.
 * @param fileName the name of the file that we are reading from.s
 * @param bytesToRead the number of bytes to store in the block data.
 */
Block* createBlock(int startBlock, int endBlock, char* fileName, int file_id)
{

    Miss_number ++;
    Block* newBlock = new Block;
    newBlock->blockStart = startBlock;
    newBlock->blockEnd = endBlock;
    newBlock->blockId = blockIndex;
    blockIndex++;
    newBlock->fileName = fileName;
    newBlock->blockIndex = newBlock->blockStart / cacheState.blockSize;
    newBlock-> lruCnt = leastUsedCnt;
    leastUsedCnt++;
    int bytesToStore = newBlock->blockEnd - newBlock->blockStart;
    newBlock -> data =(char*) aligned_alloc(bytesToStore,bytesToStore);

    if(Cache_Algo == FBR)
    {
        newBlock->accessCounter = 1;
    }
    int res = pread(file_id, newBlock->data, endBlock - startBlock, newBlock -> blockStart);

    if(res==-1) {
        cout<<"PREAD FAILED\n"<<endl;
        return nullptr;

    }
    char* file_name = getFileName(file_id);



    for(std::vector <int>:: iterator it = fdFileName[file_name].begin();it !=  fdFileName[file_name].end(); it ++ )
    {
        fdBlocks[*it].push_back(newBlock->blockId);
    }

    delete[](file_name);
    return newBlock;



}

/**
 * This function return all the blocks that have an Id that is in the blocksIndex vector.
 * @param blocksIndex a vector of ints that represent the id of the blocks that we want to retrun.
 * @return a vector of blocks.
 */
vector <Block*> blocks_vector(vector <int> blocksIndex)
{
    vector <Block*> blocksVec;
    for (std::vector<int>::iterator it = blocksIndex.begin(); it != blocksIndex.end(); ++it)
    {
        for(std::vector<Block*>::iterator it2 = CacheFS.blockVec.begin(); it2 != CacheFS.blockVec.end(); ++it2)
        {
            if((*it2)->blockId == *it)
            {
                blocksVec.push_back((*it2));
            }
        }
    }
    return blocksVec;

}

/**
 * A function that checks if a cache miss occurred.
 * @param occupiedBlocks the blocks that are in the cache for a specific file.
 * @param offset the start reading point.
 * @return 1 if  cache miss occurred, else 1;
 */
int cache_miss(vector <Block*> occupiedBlocks, int offset)
{
    for (std::vector<Block*>::iterator it = occupiedBlocks.begin(); it != occupiedBlocks.end(); ++it)
    {
        if((*it)->blockStart <= offset && offset < (*it)->blockEnd)
        {
            return -1;
        }
    }
    return 1;
}

/**
 * return the Block that contain the offset in it's range (startBlock - endBlock).
 * @param offset the start point to read.
 * @return return the block .
 */
Block* find_Block(int offset)
{
    for (std::vector<Block*>::iterator it = CacheFS.blockVec.begin(); it != CacheFS.blockVec.end(); ++it)
    {
        if((*it)->blockStart <= offset && offset < (*it)->blockEnd)
        {
            return *it;
        }
    }
    return NULL;
}



/**
 *  this function fell the missed (if there are) blocks in the cache.
 * @param occupiedBlocks the blocks in the Cache .
 * @param offset start point to read.
 * @param bytesToRead the number of bytes to store in the block data.
 * @param fileSize the size of the file in bytes.
 * @param file_id the id of the file
 */
void put_missed_blocks( vector <Block*> occupiedBlocks, int offset, int bytesToRead, int fileSize, int file_id)
{
    int curOffset = offset;
    int end = curOffset + bytesToRead;
    while(curOffset < end)
    {
        if(cache_miss(occupiedBlocks,curOffset) != -1)
        {
            int startBlock = (curOffset / cacheState.blockSize) * cacheState.blockSize;
            int endBlock = min(startBlock + cacheState.blockSize, fileSize);
            char* fileName = getFileName(file_id);
            Block* b = createBlock(startBlock,endBlock, fileName, file_id);
            int free_blocks = cacheState.numberOfBlocks - CacheFS.blockVec.size();
            if(free_blocks >=  1)
            {
                CacheFS.blockVec.push_back(b);
            } else
            {
                deleteBlock();
                CacheFS.blockVec.push_back(b);
            }
            curOffset = b->blockEnd + 1;
        }
        else
        {
            Hits_number++;
            Block* b = find_Block(curOffset);
            curOffset = b->blockEnd + 1;

        }

    }
}



/**
 *
 * @return
 */
int block_not_in_newP(int blockId)
{
    sort(lruBlocksVec.begin(),lruBlocksVec.end(),compare_by_lru);
    int old_plus_middle = CacheFS.blockVec.size() * (1 - cacheState.fNew);
    for(int i = 0; i < old_plus_middle; i++)
    {
        if(CacheFS.blockVec.at(i)->blockId == blockId)
        {
            return 1;
        }
    }
    return 0;

}






/**
 * A function that reads the data from the block in the catch.
 * @param fileBlocks a vector that have all the blocks for the file.
 * @param offset start point to read.
 * @param buf container for the readen data.
 * @param bytesToRead the number of bytes to store in the block data.
 */
void read_from_cache(vector <Block*> fileBlocks, int offset, void* buf, int bytesToRead)
{

    int bufCounter = 0;
    int curOffset = offset;
    char* tempBuf = nullptr;
    size_t buf_size = bytesToRead;
    tempBuf = (char*)aligned_alloc(bytesToRead,bytesToRead);
    int tempBufIndex = 0;
    for(std::vector<Block*>::iterator it = fileBlocks.begin(); it != fileBlocks.end(); ++it)
    {

        bufCounter=0;

        if(curOffset >= (*it)->blockStart && curOffset < (*it)->blockEnd)
        {
            int dataStart = curOffset - (*it)->blockStart;
            int dataEnd = min((*it)->blockEnd - (*it)->blockStart, curOffset + bytesToRead - (*it)->blockStart);
            for(int i = dataStart; i < dataEnd; i++)
            {
                tempBuf[tempBufIndex] = (*it)->data[i];
                bufCounter++;
                tempBufIndex++;
            }
            curOffset = min((*it)->blockEnd , curOffset + bytesToRead);
            bytesToRead = bytesToRead - bufCounter;

            if(Cache_Algo == FBR)
            {
                if(block_not_in_newP(find_Block(curOffset)->blockId))
                {
                    (*it)->accessCounter++;
                }
            }
            else
            {
                (*it)->accessCounter++;
            }

            int res = return_index((*it)->blockId);
            if(res != -1)
            {
                lruBlocksVec[res].second = leastUsedCnt;
                leastUsedCnt++;
            }
            else
            {
                lruBlocksVec.push_back(make_pair((*it)->blockId,leastUsedCnt));
                leastUsedCnt++;
            }

        }
    }

    memcpy(buf,tempBuf,buf_size);

    free( tempBuf);
    return;
}

bool compare_By_StarBlock(Block * first, Block * second)
{
    return first->blockStart < second->blockEnd;
}

/**
   Read data from an open file.

   Read should return exactly the number of bytes requested except
   on EOF or error. For example, if you receive size=100, offset=0,
   but the size of the file is 10, you will initialize only the first
   ten bytes in the buff and return the number 10.

   In order to read the content of a file in CacheFS,
   We decided to implement a function similar to POSIX's pread, with
   the same parameters.

 Returned value:
    In case of success:
		Non negative value represents the number of bytes read.
		See more details above.

 	In case of failure:
		Negative number.
		A failure will occur if:
			1. a system call or a library function fails (e.g. pread).
			2. invalid parameters
				a. file_id is valid if"f it was returned by
			       CacheFS_open, and it wasn't already closed.
				b. buf is invalid if it is NULL.
				c. offset is invalid if it's negative
				   [Note: offset after the end of the file is valid.
				    In this case, you need to return zero,
				    like posix's pread does.]
				[Note: any value of count is valid.]
 */
int CacheFS_pread(int file_id, void *buf, size_t count, off_t offset)
{

    if(file_id < 0 || count < 0 || offset < 0)
    {
        return FAIL;
    }
    if(!isFileOpen(file_id))
    {
        return FAIL;
    }
    if(buf == NULL)
    {
        return FAIL;
    }
    int file_size = fileSize(file_id);
    int bytesToRead = min(file_size - (int)offset, (int)count);
    int readenData =min(file_size - (int)offset, (int)count);
    if(bytesToRead <= 0 || offset > file_size || offset + bytesToRead > file_size)
    {
        return ZERO_BYTES;
    }

    int curCacheSize = CacheFS.blockVec.size();
    int maxBlocks = cacheState.numberOfBlocks;
    int endBlock = (bytesToRead + offset) / cacheState.blockSize;
    if ((bytesToRead + offset) % cacheState.blockSize != 0) {
        endBlock++;
    }
    int startBlock = offset / cacheState.blockSize;



    int start;
    int end;
    char *fileName;

    int tempBufIndex = 0;
    if(endBlock - startBlock > maxBlocks)
    {
        int bufCounter = 0;
        int curOffset = offset;
        char* tempBuf = nullptr;
        tempBuf = (char*)aligned_alloc(bytesToRead,bytesToRead);


        for (int j = startBlock; j < endBlock; j++) {

            if (maxBlocks - curCacheSize == 0)
            {
                deleteBlock();
                curCacheSize = CacheFS.blockVec.size();
            }
            start = j * cacheState.blockSize;
            end = min((j + 1) * cacheState.blockSize, file_size);
            fileName = getFileName(file_id);

            Block *b = createBlock(start, end, fileName, file_id);

            CacheFS.blockVec.push_back(b);
            curCacheSize = CacheFS.blockVec.size();


            int dataStart = curOffset - (b)->blockStart;
            int dataEnd = min((b)->blockEnd - (b)->blockStart, curOffset + bytesToRead - (b)->blockStart);

            int k=0;
            for(int i = dataStart; i < dataEnd; i++)
            {
                tempBuf[tempBufIndex] = (b)->data[k];
                bufCounter++;
                tempBufIndex++;
                k++;
            }
            curOffset = min((b)->blockEnd , curOffset + bytesToRead);
            bytesToRead = bytesToRead - bufCounter;

            if(Cache_Algo == FBR)
            {
                if(block_not_in_newP(find_Block(curOffset)->blockId))
                {
                    (b)->accessCounter++;
                }
            }
            else
            {
                (b)->accessCounter++;
            }

            int res = return_index((b)->blockId);
            if(res != -1)
            {
                lruBlocksVec[res].second = leastUsedCnt;
                leastUsedCnt++;
            }
            else
            {
                lruBlocksVec.push_back(make_pair(b->blockId,leastUsedCnt));
                leastUsedCnt++;
            }

        }

        memcpy(buf,tempBuf,readenData);
        return readenData;


    }






    if(fdBlocks[file_id].empty()) {

        curCacheSize = CacheFS.blockVec.size();
        maxBlocks = cacheState.numberOfBlocks;
        endBlock = (bytesToRead + offset) / cacheState.blockSize;
        if ((bytesToRead + offset) % cacheState.blockSize != 0) {
            endBlock++;
        }
        startBlock = offset / cacheState.blockSize;

        for (int j = startBlock; j < endBlock; j++) {

            if (maxBlocks - curCacheSize == 0)
            {
                deleteBlock();
                curCacheSize = CacheFS.blockVec.size();
            }
            start = j * cacheState.blockSize;
            end = min((j + 1) * cacheState.blockSize, file_size);
            fileName = getFileName(file_id);

            Block *b = createBlock(start, end, fileName, file_id);

            CacheFS.blockVec.push_back(b);
            curCacheSize = CacheFS.blockVec.size();
        }
    }


    else
    {
        vector <Block*> occupiedBlocks = blocks_vector(fdBlocks[file_id]);
        put_missed_blocks(occupiedBlocks, offset, bytesToRead, file_size, file_id);

    }
    vector <Block*> fileBlocks = blocks_vector(fdBlocks[file_id]);



    std::sort(fileBlocks.begin(),fileBlocks.end(),compare_By_StarBlock);

    read_from_cache(fileBlocks, offset, buf, bytesToRead);

    return bytesToRead;
}


/**
 *
 * @param a
 * @param b
 * @return
 */
int reverse_compare_by_lru(pair <int,int> a , pair <int,int> b)
{
    return a.second >=  b.second;
}


int reverse_compare_by_lfu(Block* a , Block* b)
{
    return a->accessCounter >= b->accessCounter;

}



/**
This function writes the current state of the cache to a file.
The function writes a line for every block that was used in the cache
(meaning, each block with at least one access).
Each line contains the following values separated by a single space.
	1. Full path of the file
	2. The number of the block. Pay attention: this is not the number in the cache,
	   but the enumeration within the file itself, starting with 0 for the first
	   block in each file.
For LRU and LFU The order of the entries is from the last block that will be evicted from the cache
to the first (next) block that will be evicted.
For FBR use the LRU order (the order of the stack).

Notes:
	1. If log_path is a path to existed file - the function will append the cache
	   state (described above) to the cache.
	   Otherwise, if the path is valid, but the file doesn't exist -
	   a new file will be created.
	   For example, if "/tmp" contains a single folder named "folder", then
			"/tmp/folder/myLog" is valid, while "/tmp/not_a_folder/myLog" is invalid.
	2. Of course, this operation doesn't change the cache at all.
	3. log_path doesn't have to be under "/tmp".
	3. This function might be useful for debugging purpose as well as auto-tests.
	   Make sure to follow the syntax and order as explained above.

 Parameter:
	log_path - a path of the log file. A valid path is either: a path to an existing
			   log file or a path to a new file (under existing directory).

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. open, write).
		2. log_path is invalid.
 */
int CacheFS_print_cache (const char *log_path)
{

    cacheState.logfile.open(log_path,std::fstream::out | std::fstream::app);

    if (!cacheState.logfile.is_open())
    {
         return FAIL;
    }

    if(Cache_Algo == LRU || Cache_Algo == FBR )
    {
        Block* b ;
        std::sort(lruBlocksVec.begin(), lruBlocksVec.end(), reverse_compare_by_lru);
        for(std::vector<pair<int,int>>::iterator it = lruBlocksVec.begin(); it != lruBlocksVec.end();it++ )
        {
            b = find_block((*it).first);

            cacheState.logfile << b->fileName;
            cacheState.logfile << " ";
            cacheState.logfile << std::to_string(b->blockIndex);
            cacheState.logfile << "\n";
        }
    }
    else
    {

        std::sort(CacheFS.blockVec.begin(), CacheFS.blockVec.end(), reverse_compare_by_lfu);

        for(std::vector<Block*>::iterator it = CacheFS.blockVec.begin(); it != CacheFS.blockVec.end(); it++)
        {
            cacheState.logfile << (*it)->fileName;
            cacheState.logfile << " ";
            cacheState.logfile << std::to_string((*it)->blockIndex);
            cacheState.logfile << "\n";
        }


    }
    cacheState.logfile.close();
    return SUCCESS;

}


/**
This function writes the statistics of the CacheFS to a file.
This function writes exactly the following lines:
Hits number: HITS_NUM.
Misses number: MISS_NUM.

Where HITS_NUM is the number of cache-hits, and MISS_NUM is the number of cache-misses.
A cache miss counts the number of fetched blocks from the disk.
A cache hit counts the number of required blocks that were already stored
in the cache (and therefore we didn't fetch them from the disk again).

Notes:
	1. If log_path is a path to existed file - the function will append the cache
	   state (described above) to the cache.
	   Otherwise, if the path is valid, but the file doesn't exist -
	   a new file will be created.
	   For example, if "/tmp" contains a single folder named "folder", then
			"/tmp/folder/myLog" is valid, while "/tmp/not_a_folder/myLog" is invalid.
	2. Of course, this operation doesn't change the cache at all.
	3. log_path doesn't have to be under "/tmp".

 Parameter:
	log_path - a path of the log file. A valid path is either: a path to an existing
			   log file or a path to a new file (under existing directory).

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. open, write).
		2. log_path is invalid.
 */
int CacheFS_print_stat (const char *log_path)
{


    cacheState.logfile.open(log_path, std::fstream::out | std::fstream::app);

    if (!cacheState.logfile.is_open())
    {
        return FAIL;
    }
    char* miss = (char*)"Misses number:";
    char* hit = (char*)"Hits number:";
    cacheState.logfile.write(hit, strlen(hit));
    cacheState.logfile.write(" ", 1);
    cacheState.logfile.write(to_string(Hits_number).c_str(), strlen(to_string(Hits_number).c_str()));
    cacheState.logfile.write("\n", strlen("\n"));
    cacheState.logfile.write(miss, strlen(miss));
    cacheState.logfile.write(" ", 1);
    cacheState.logfile.write(to_string(Miss_number).c_str(), strlen(to_string(Miss_number).c_str()));
    cacheState.logfile.write("\n", strlen("\n"));
    cacheState.logfile.close();
    return SUCCESS;
}


