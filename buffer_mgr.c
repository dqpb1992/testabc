//
//  buffer_mgr.c
//  testcode_1_1010
//
//  Created by PingyuXue on 10/12/16.
//  Copyright © 2016 pingyuXue. All rights reserved.
//


// from back up 10_25 Version

#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "dberror.h"
#include "stdio.h"
#include "stdlib.h"
#include "dt.h"

// cannot use for the right now
// #include "replacement_strategy.h"



// Structure for the Bufferpool and pageFrame
/***************************************************************
 * Function Name: Create Structrue
 *
 * Description: Create structure
 *
 * Parameters: NULL
 *
 * Return: NULL
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/25      Pingyu XUe                     Create
 ***************************************************************/
typedef struct Bufferpool{
    BM_BufferPool bufferPool_bm;
    int num_Read_IO;
    int num_Write_IO;
    int clock;
    
}BufferPool;


// merge with the strutue in "replaceent_mgr.c"
typedef struct pageFrame{
    BM_PageHandle pageHandle;
    bool dirty;
    int fixCounts;
    int *strategyAttribute;
    
    void *mgmTransferData;  // for the information transfer into mgmtDat
}pageFrame;


// Buffer Manager Interface Pool Handling
/***************************************************************
 * Function Name: initBufferPool
 *
 * Description: initialize the BufferPool
 *
 * Parameters: 
 *           BM_BufferPool *const bm,
 *           const char *const pageFileName,
 *           const int numPages,
 *           ReplacementStrategy strategy,
 *           void *stratData.
 *
 * Return: RC
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/25      Pingyu XUe                     Create
 *   2016/10/25      Pingyu XUe                     initialize BM
 ***************************************************************/
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData){
    //check :
    
    int i;
    
    if ( bm == NULL){
        printf("bm is empty! function locate: initBufferPool\n");
        return RC_UNESPECTED_ERROR;
    }
    
    if ( pageFileName == NULL){
        printf("pageFile is empty! function locate: initBufferPool\n");
        return RC_UNESPECTED_ERROR;
    }
    
    if ( numPages < 0){
        // should we consider numPage ==0 ?  (Q: pyx 10252016)
        printf("numPages is empty! function locate: initBufferPool\n");
        return RC_UNESPECTED_ERROR;
    }
    
    
    
    // step1 :  step up structure, buffer_pool and 
    // set up BM
    BufferPool BM;
    BM.bufferPool_bm = *bm;   // init BM  add: pingyuXue 10/25//2016
    
    // set up pf
     pageFrame *pf = (pageFrame *)calloc(numPages,sizeof(pageFrame));
    
    FILE *file;
    
    
    // step: init the bufferppl
    file = fopen(pageFileName, "r");
    
    if (file != NULL){
        
        BM.bufferPool_bm.pageFile = (char *) pageFileName;
        BM.bufferPool_bm.numPages = numPages;
        BM.bufferPool_bm.strategy = strategy;
        BM.bufferPool_bm.mgmtData = pf;
        BM.bufferPool_bm.mgmtData->mgmTransferData = stratData;
        
        
        // set up the pageFrame
        for ( i = 0; i< BM.bufferPool_bm.numPages; i++){
            (BM.bufferPool_bm.mgmtData +i)->dirty = 0;
            (BM.bufferPool_bm.mgmtData +i)->fixCounts =0;
            (BM.bufferPool_bm.mgmtData +i)->pageHandle.pageNum= -1;
            (BM.bufferPool_bm.mgmtData +i)->pageHandle.data = NULL;
            
            // need to set up for the in addtion structure
        }
        
        BM.num_Read_IO = 0;
        BM.num_Write_IO = 0;
        
        fclose(file);
        return RC_OK;
    }
    else
        printf("read file eror or other unknow error! function locate: initBufferPool\n");
        return RC_UNESPECTED_ERROR;
}

/***************************************************************
 * Function Name: shutdownBufferPool
 *
 * Description: shut down the BufferPool
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: RC
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/25      Pingyu XUe                     Create
 ***************************************************************/
RC shutdownBufferPool(BM_BufferPool *const bm){
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    pageFrame *pf = (pageFrame *)calloc(BM.bufferPool_bm.numPages,sizeof(pageFrame));
    
    int i;
    // check the buffer can be shut down or not.
    int *fixcount = getFixCounts(bm);
    for (i=0; i<BM.bufferPool_bm.numPages; ++i){
        if (* (fixcount +i)){
            free(fixcount);
            return RC_SHUT_DOWN_POOL_ERROR;
        }
    }
    
    // write the data into disk
    
    
    // I want to write the data into disk.
    for (i=0; i<BM.bufferPool_bm.numPages; i++){
        (pf +i)->pageHandle.pageNum = 0;
        free((pf +i)->pageHandle.data);
        (pf +i)->fixCounts = 0;
        free((pf +i)->mgmTransferData);
    }
    
    free(BM.bufferPool_bm.pageFile);
    BM.bufferPool_bm.strategy = -1;
    BM.bufferPool_bm.numPages = 0;
    
    // alredy free all the data in pf before.
    free(BM.bufferPool_bm.mgmtData);
    
    free(pf);
    
    return RC_OK;
}

/***************************************************************
 * Function Name: forceFlushPool
 *
 * Description:
 *
 * Parameters:
 *
 * Return:
 *
 * Author:
 *
 * History:
 ***************************************************************/
RC forceFlushPool(BM_BufferPool *const bm){
    bool *dirtyFlag;
    int *fixCount;
    
    int i;
    RC flag;
    
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    pageFrame *pf;
    
    dirtyFlag =getDirtyFlags(bm);
    fixCount =getFixCounts(bm);
    
    for (i=0; i<BM.bufferPool_bm.numPages; ++i) {
        if( *(dirtyFlag) + i){
            if ( *(fixCount) +i){
                continue;
            }
        }
        
        // pf location:
        else {
            pf = (BM.bufferPool_bm.mgmtData +i);
            flag = forcePage(bm, &pf->pageHandle);
            if ( flag != RC_OK){
                free(dirtyFlag);
                free(fixCount);
                return flag;
            }
            
        }
        
    }
    
    for (i=0;i<BM.bufferPool_bm.numPages; i++){
        if( *(dirtyFlag + i)){
            ((BM.bufferPool_bm.mgmtData + i)->dirty = 0); // ture of false
        }
    }
    
    free(dirtyFlag);
    free(fixCount);
    return RC_OK;
}


// Buffer Manager Interface Access Pages
/***************************************************************
 * Function Name: markDirty
 *
 * Description: mark a page as dirty page
 *
 * Parameters: 
  *          BM_BufferPool *const bm,
 *           BM_PageHandle *const page
 *
 * Return:RC
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/12       Pingyu XUe                    Create
 ***************************************************************/
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page){
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    // the pf here should add a new memeory ?(Q1: PingyuXue 10/25/2016)
//    
//    pageFrame *pf = (pageFrame* )calloc( BM.bufferPool_bm.numPages, sizeof(pageFrame));
    
    pageFrame *pf;
    pf->pageHandle = *page;
    
    int count = 0;
    
    while (count++ < BM.bufferPool_bm.numPages){
        if((BM.bufferPool_bm.mgmtData +count)->pageHandle.pageNum == page->pageNum){
            // is that both pf-> and BM.bufferPoll_bm.mgmData->data should be changed?
            pf->dirty = true;
            BM.bufferPool_bm.mgmtData->dirty= true;
            
            return RC_OK;
        }
//        count++;
    }
    
    // did not find the value
    return RC_UNESPECTED_ERROR;
}

/***************************************************************
 * Function Name: unpinPage
 *
 * Description:unpin a page
 *
 * Parameters:
 *           BM_BufferPool *const bm
 *           BM_PageHandle *const page
 *
 * Return:RC
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/25      Pingyu XUe                     Create
 ***************************************************************/
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page){
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
//    pageFrame *pf;
//    pf->pageHandle = *page;
    
    int count =0;
    
    while (count++ < BM.bufferPool_bm.numPages){
        if( (BM.bufferPool_bm.mgmtData + count)->pageHandle.pageNum == page->pageNum){
            
            if ((BM.bufferPool_bm.mgmtData + count)->fixCounts > 1){
                (BM.bufferPool_bm.mgmtData +count)->fixCounts--;
                return RC_OK;
            }
            else
                return RC_UNESPECTED_ERROR;
        }
        
    }
    
    // did not find the value
    return RC_UNESPECTED_ERROR;
}


/***************************************************************
 * Function Name: forcePage
 *
 * Description: write the current content of the page back to the page file on disk
 *
 * Parameters:
 *            BM_BufferPool *const bm
 *            BM_PageHandle *const page
 *
 * Return:RC
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/12      Pingyu XUe                     Create
 ***************************************************************/
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page){
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    pageFrame *pf;
    pf->pageHandle = *page;
    
    FILE *file;
    
    // step1: open file and find the locate of the file
    file = fopen(BM.bufferPool_bm.pageFile, "rb+");
    
    if( file != NULL){
        fseek(file, (page->pageNum)*PAGE_SIZE, SEEK_SET);
        fwrite(page->data, PAGE_SIZE, 1, file);
        BM.num_Write_IO++;
        fclose(file);
    }
    
    // step2: make the dirty clean
    int count = 0;
    
    while (count++ < BM.bufferPool_bm.numPages){
        if ((BM.bufferPool_bm.mgmtData + count)->pageHandle.pageNum == page->pageNum){
            (BM.bufferPool_bm.mgmtData + count)->dirty = false;
            pf->dirty = false;
            return RC_OK;
        }
    }
    
    // did not find the value
    return RC_UNESPECTED_ERROR;

}

/***************************************************************
 * Function Name: 
 *
 * Description:
 *
 * Parameters:
 *
 * Return:
 *
 * Author:
 *
 * History:
 ***************************************************************/
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page,
            const PageNumber pageNum){
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    pageFrame *pf;
    pf->pageHandle = *page;
    
    RC FindPage = 1;
    RC Not_FindPage = 2;
    
    
    
    int pnum = 0;
    int checkStatus = 0;  // check the status of the bufferpool
    
    int i,j;
    
    
    for(i=0; i< BM.bufferPool_bm.numPages; i++){
        // check the status of the page which being pined
        if ((BM.bufferPool_bm.mgmtData +i)->pageHandle.pageNum == -1){
            pnum = i;
            checkStatus = Not_FindPage;
            break;
        }
        
        if ((BM.bufferPool_bm.mgmtData +i)->pageHandle.pageNum == pageNum)
        {
            pnum = i;
            checkStatus = FindPage;
            break;
        }
        
        if (i == (BM.bufferPool_bm.numPages -1)){
            checkStatus = Not_FindPage;
            
            if (BM.bufferPool_bm.strategy == RS_FIFO) {
                pnum= strategyFIFOandLRU(BM);
                if ((BM.bufferPool_bm.mgmtData +pnum)->dirty){
                    
                    forcePage(bm, (BM_PageHandle *)BM.bufferPool_bm.mgmtData +pnum);  // warnning for the page
                }
            }
            
            if(BM.bufferPool_bm.strategy == RS_LRU){
                pnum = strategyFIFOandLRU(BM);
                if ((BM.bufferPool_bm.mgmtData +pnum)->dirty){
                    forcePage(bm, (BM_PageHandle *)BM.bufferPool_bm.mgmtData +pnum);  // warnning for the page
                }
            }
        }
    }
    
    if (checkStatus == Not_FindPage){
        FILE *fp;
        
        fp = fopen(BM.bufferPool_bm.pageFile, "r");
        fseek(fp, pageNum*PAGE_SIZE, SEEK_SET);
        fread((BM.bufferPool_bm.mgmtData)->pageHandle.data, sizeof(char), PAGE_SIZE, fp); // warning
        
        // let data in page will be change
        pf->pageHandle.data = (BM.bufferPool_bm.mgmtData + pnum)->pageHandle.data;
        
        // change the Buffer code
        BM.num_Read_IO++;
        (BM.bufferPool_bm.mgmtData + pnum)->fixCounts ++;
        (BM.bufferPool_bm.mgmtData +pnum)->pageHandle.pageNum = pageNum;
        
        pf->fixCounts = (BM.bufferPool_bm.mgmtData + pnum)->fixCounts;
        pf->pageHandle.pageNum = pageNum;
        pf->dirty = (BM.bufferPool_bm.mgmtData +pnum)->dirty;
        pf->strategyAttribute = (BM.bufferPool_bm.mgmtData + pnum)->strategyAttribute;
        updataAttribute(BM.bufferPool_bm.mgmtData + pnum);
        fclose( fp);
        
    }
    
    if (checkStatus == FindPage){
        pf->pageHandle.data = (BM.bufferPool_bm.mgmtData +pnum)->pageHandle.data;
        ((BM.bufferPool_bm.mgmtData +pnum)->fixCounts)++;
        pf->fixCounts =(BM.bufferPool_bm.mgmtData +pnum)->fixCounts;
        pf->dirty = (BM.bufferPool_bm.mgmtData +pnum)->dirty;
        pf->strategyAttribute = (BM.bufferPool_bm.mgmtData +pnum)->strategyAttribute;
        
    }
    
    
    return RC_OK;
}



// Statistics Interface
/***************************************************************
 * Function Name: getFrameContents
 *
 * Description: Returns an array of PageNumbers where the ith element is the number of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: PageNumber *
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/12      Pingyu Xue                     Create
 *   2016/10/25      Pingyu Xue                     re-write
 ***************************************************************/
PageNumber *getFrameContents (BM_BufferPool *const bm){
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    pageFrame *pf = BM.bufferPool_bm.mgmtData;
    int i =0;
    
    PageNumber *frameContents = (PageNumber*)malloc(BM.bufferPool_bm.numPages*sizeof(PageNumber));
    
    for ( i=0; i<BM.bufferPool_bm.numPages; i++){
        if((pf+i)->pageHandle.data == NULL){
            frameContents[i] = NO_PAGE;
        }
        else{
            frameContents[i] = (pf+i)->pageHandle.pageNum;
        }
    }
    
    return frameContents;
}

/***************************************************************
 * Function Name: getDirtyFlags
 *
 * Description: Returns an array of bools where the ith element is TRUE if the page stored in the ith page frame is dirty
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: bool 
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/12      Pingyu XUe                     Create
 *   2016/10/25      Pingyu Xue                     re-write
 ***************************************************************/
bool *getDirtyFlags (BM_BufferPool *const bm){
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    pageFrame *pf = BM.bufferPool_bm.mgmtData;

    bool *dirtyflag = (bool*) malloc (BM.bufferPool_bm.numPages * sizeof(bool));
    
    int i =0;
    for (i=0; i< BM.bufferPool_bm.numPages; i++){
        dirtyflag[i]= (pf + i)->dirty;
    }
    
    
    return dirtyflag;
}

/***************************************************************
 * Function Name: getFixCounts
 *
 * Description: Returns an array of ints where the ith element is the fix count of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: int *
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/12      Pingyu XUe                     Create
 *   2016/10/25      Pingyu XUe                     re-write
 ***************************************************************/
int *getFixCounts (BM_BufferPool *const bm){
    
    BufferPool BM;
    BM.bufferPool_bm = *bm ;
    
    pageFrame *pf = BM.bufferPool_bm.mgmtData;
    
    // set the memory for the fixcount
    int *FixCount = (int*) malloc (BM.bufferPool_bm.numPages * sizeof( int));
    
    int i;
    for ( i=0; i< BM.bufferPool_bm.numPages;i++){
        FixCount[i] = (pf+ i)->fixCounts;
    }
    
    return FixCount;
}

/***************************************************************
 * Function Name: getNumReadIO
 *
 * Description:  Returns an array of ints where the ith element is the fix count of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: int
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/12      Pingyu XUe                     Create
 *   2016/10/25      Pingyu XUe                     re-write
 ***************************************************************/
int getNumReadIO (BM_BufferPool *const bm){
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    return BM.num_Read_IO;
}

/***************************************************************
 * Function Name: getNumWriteIO
 *
 * Description: Returns an array of ints where the ith element is the fix count of the page stored in the ith page frame
 *
 * Parameters: BM_BufferPool *const bm
 *
 * Return: int
 *
 * Author: Pingyu Xue
 *
 * History:
 *      Date            Name                        Content
 *   2016/10/12      Pingyu Xue                     Create
 *   2016/10/25      Pingyu Xue                     re-write
 ***************************************************************/
int getNumWriteIO (BM_BufferPool *const bm){
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    return BM.num_Write_IO;
}


/************************************END OF FUNCTIONS********************************/






/***************************************************************
 * Function Name: getAttributionArray
 *
 * Description: return an array that includes all pages strategyAttribute.
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: int*
 *
 * Author: Xiaoliang Wu
 *
 * History:
 *      Date            Name                        Content
 *      16/02/27        Xiaoliang Wu                Complete.
 *
 ***************************************************************/

int *getAttributionArray(BM_BufferPool *bm) {
    int *attributes;
    int *flag;
    int i;
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    
    attributes = (int *)calloc(bm->numPages, sizeof((bm->mgmtData)->strategyAttribute));
    attributes = (int *)calloc (BM.bufferPool_bm.numPages, sizeof(BM.bufferPool_bm.mgmtData)->strategyAttribute);
    
    
    for (i = 0; i <BM.bufferPool_bm.numPages; ++i) {
        flag = attributes + i;
        *flag = * ((BM.bufferPool_bm.mgmtData +i)->strategyAttribute);
    }
    return attributes;
}


/***************************************************************
 * Function Name: strategyFIFOandLRU
 *
 * Description: decide use which frame to save data using FIFO.
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: int
 *
 * Author: Xiaoliang Wu
 *
 * History:
 *      Date            Name                        Content
 *      16/02/27        Xiaoliang Wu                Complete
 *
 ***************************************************************/

int strategyFIFOandLRU(BM_BufferPool *bm) {
    int * attributes;
    int * fixCounts;
    int i;
    int min, abortPage;
    
    BufferPool BM;
    BM.bufferPool_bm = *bm;
    
    attributes = (int *)getAttributionArray(&BM.bufferPool_bm);

    fixCounts = getFixCounts(bm);
    

    min = BM.clock;
    abortPage = -1;
    
    for (i = 0; i < BM.bufferPool_bm.numPages; ++i) {
        if (*(fixCounts + i) != 0) continue;
        
        if (min >= (*(attributes + i))) {
            abortPage = i;
            min = (*(attributes + i));
        }
    }
    
    if ((BM.clock) > 32000) {
        (BM.clock) -= min;
        for (i = 0; i < BM.bufferPool_bm.numPages; ++i) {
            *(BM.bufferPool_bm.mgmtData->strategyAttribute) -=min;
        }
    }
    return abortPage;
}

/***************************************************************
 * Function Name: freePagesBuffer
 *
 * Description: free all pages in pool.
 *
 * Parameters: BM_BufferPool *bm
 *
 * Return: RC
 *
 * Author: Xiaoliang Wu
 *
 * History:
 *      Date            Name                        Content
 *      16/02/26        Xiaoliang Wu                Complete.
 *
 ***************************************************************/

void freePagesBuffer(BM_BufferPool *bm) {
    BufferPool BM;
    BM.bufferPool_bm = *bm;

    int i;
    
    for (i = 0; i < BM.bufferPool_bm.numPages; ++i) {
        free((BM.bufferPool_bm.mgmtData +i)->pageHandle.data);
        free((BM.bufferPool_bm.mgmtData +i)->strategyAttribute);
    }
}

/***************************************************************
 * Function Name: updataAttribute
 *
 * Description: modify the attribute about strategy. FIFO only use this function when page initial. LRU use this function when pinPage occurs.
 *
 * Parameters: BM_BufferPool *bm, BM_PageHandle *pageHandle
 *
 * Return: RC
 *
 * Author: Xiaoliang Wu
 *
 * History:
 *      Date            Name                        Content
 *      16/02/26        Xiaoliang Wu                FIFO, LRU complete.
 *
 ***************************************************************/

RC updataAttribute(BM_BufferPool *bm, BM_PageHandle *pageHandle) {
    // initial page strategy attribute assign buffer
    
    BufferPool BM;
    BM.bufferPool_bm =*bm;
    pageFrame *pf;
    pf->pageHandle = *pageHandle;
    
    if(pf->strategyAttribute == NULL){
        if(BM.bufferPool_bm.strategy == RS_FIFO || BM.bufferPool_bm.strategy == RS_LRU){
            pf->strategyAttribute = calloc(1,sizeof(int));
        }
        
    }
    
    // assign number
    if(BM.bufferPool_bm.strategy == RS_FIFO || BM.bufferPool_bm.strategy == RS_LRU){
        int * compareTimer;
        compareTimer = (int *)pf->strategyAttribute;
        *compareTimer = (BM.clock);
        BM.clock++;
        return RC_OK;
    }
   
    return RC_UNESPECTED_ERROR;
}
