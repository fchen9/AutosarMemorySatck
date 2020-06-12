/*******************************************************************************
**                                                                            **
**  FILENAME     : NvM.c                                                      **
**                                                                            **
**  VERSION      : 4.3.1                                                      **
**                                                                            **
**  DATE         : 2020-6-3                                                   **
**                                                                            **
**  PLATFORM     : TIVA C                                                     **
**                                                                            **
**  AUTHOR       : Yomna Mokhtar                                              **
**                                                                            **
**                                                                            **
*******************************************************************************/

/*****************************************************************************************/
/*                                   Include headers                                     */
/*****************************************************************************************/
#include "NvM_Shared.h"
/*Those must be replaced by "MemIf.h" */
#include "MemIf_Types.h"


/*****************************************************************************************/
/*                       Local Functions Implementation                                  */
/*****************************************************************************************/

/****************************************************************************************/
/*    Function Name           : Init_Queue                                              */
/*    Function Description    : put queue in it's initialized state                     */
/*    Parameter in            : none                                                    */
/*    Parameter inout         : none                                                    */
/*    Parameter out           : none                                                    */
/*    Return value            : none                                                    */
/****************************************************************************************/

void Init_Queue(void)
{
    /*counter to loop over queue elements*/
    uint16 counter ;

    Stand_Queue_Indeces.Head = 0 ;
    Stand_Queue_Indeces.Tail = 0 ;

    Standard_Queue_Empty = TRUE ;
    Standard_Queue_FULL = FALSE ;

    #if(NVM_JOB_PRIORITIZATION == STD_ON)
      Immed_Queue_Indeces.Head = 0 ;
      Immed_Queue_Indeces.Tail = 0 ;

      Immediate_Queue_Empty = TRUE ;
      Immediate_Queue_FULL = FALSE ;
    #endif

    for(counter = 0; counter < NVM_SIZE_STANDARD_JOB_QUEUE; counter++){
        Standard_Job_Queue[counter].ServiceId = NVM_INIT_API_ID ;
        Standard_Job_Queue[counter].Block_Id = 0 ;
        Standard_Job_Queue[counter].RAM_Ptr = NULL ;
    }
    #if(NVM_JOB_PRIORITIZATION == STD_ON)
        for(counter = 0; counter < NVM_SIZE_IMMEDIATE_JOB_QUEUE; counter++){
            Immediate_Job_Queue[counter].ServiceId = NVM_INIT_API_ID ;
            Immediate_Job_Queue[counter].Block_Id = 0 ;
            Immediate_Job_Queue[counter].RAM_Ptr = NULL ;
        }
    #endif
}

/****************************************************************************************/
/*    Function Name           : Search_Queue                                            */
/*    Function Description    : Search for a passed Block Id in the queue,              */
/*                              and find if it exists or not.                           */
/*                              If existed -> return E_OK                               */
/*                              If not existed -> return E_NOT_OK                       */
/*    Parameter in            : BlockId                                                 */
/*    Parameter inout         : none                                                    */
/*    Parameter out           : none                                                    */
/*    Return value            : Std_ReturnType                                          */
/****************************************************************************************/

Std_ReturnType Search_Queue(NvM_BlockIdType BlockId)
{
    /*counter to loop over queue elements*/
    uint16 counter ;
    /*Variable to save return value*/
    Std_ReturnType Return_Val = E_NOT_OK ;

    #if(NVM_JOB_PRIORITIZATION == STD_ON)
        for(counter = 0; counter < NVM_SIZE_IMMEDIATE_JOB_QUEUE; counter++){
           if(Immediate_Job_Queue[counter].Block_Id == BlockId){
               Return_Val = E_OK ;
               break ;
           }
        }
    #endif
    for(counter = 0; counter < NVM_SIZE_STANDARD_JOB_QUEUE; counter++){
       if(Standard_Job_Queue[counter].Block_Id == BlockId){
           Return_Val = E_OK ;
           break;
       }
    }

    return Return_Val ;
}

/****************************************************************************************/
/*    Function Name           : Job_Enqueue                                             */
/*    Function Description    : Add jobs to the queue to be executed later              */
/*    Parameter in            : Job                                                     */
/*    Parameter inout         : none                                                    */
/*    Parameter out           : none                                                    */
/*    Return value            : Std_ReturnType                                          */
/****************************************************************************************/

Std_ReturnType Job_Enqueue(Job_Parameters Job)
{

  /*[SWS_NvM_00378]
   * In case of priority based job processing order,
   * the NvM module shall use two queues, one for immediate write jobs (crash data)
   * another for all other jobs
   */
  // Case1 : Immediate Job
  if(NvMBlockDescriptor[Job.Block_Id].NvMBlockJobPriority == 0){
    #if (NVM_JOB_PRIORITIZATION == STD_ON)
      if(Immediate_Queue_FULL == TRUE){
        return E_NOT_OK ;
      }

      Immediate_Job_Queue[Immed_Queue_Indeces.Tail] = Job ;

      if(Immediate_Queue_Empty == TRUE){
        Immediate_Queue_Empty = FALSE ;
      }

      Immed_Queue_Indeces.Tail++ ;

      //When Tail reaches queue end
      if(Immed_Queue_Indeces.Tail == NVM_SIZE_IMMEDIATE_JOB_QUEUE){
        /*Go to index 0 of the queue again
         *As the queue is circular
         */
        Immed_Queue_Indeces.Tail = 0 ;
      }

      //When Tail reaches Head while enqueing, the queue is full
      if(Immed_Queue_Indeces.Tail == Immed_Queue_Indeces.Head){
        Immediate_Queue_FULL = TRUE ;
      }
      return E_OK ;
    #endif
  }

  // Case2 : Standard Job
  else{

    if(Standard_Queue_FULL == TRUE){
      return E_NOT_OK ;
    }

    // if queue is empty, so insert your job directly
    if(Standard_Queue_Empty == TRUE){
      Standard_Job_Queue[Stand_Queue_Indeces.Tail] = Job ;
      Standard_Queue_Empty = FALSE ;
    }
    else{ // Queue is not full and not empty


      #if (NVM_JOB_PRIORITIZATION == STD_ON)

        uint16 counter ;   //internal variable to store the loop index
        //intermediate variable to store ID of the compared job in each cycle
        NvM_BlockIdType Compared_Job_Id ;

        /*insert the new job based on priority.
        *loop over queue elements starting from tail until you reach head,
        *or reach a higher priority job
        */
        for(counter = Stand_Queue_Indeces.Tail ; counter != Stand_Queue_Indeces.Head; counter--){
          // if counter = 0
          if(counter == 0){
            Compared_Job_Id = Standard_Job_Queue[NVM_SIZE_STANDARD_JOB_QUEUE -1].Block_Id ;
            if(NvMBlockDescriptor[Job.Block_Id].NvMBlockJobPriority < NvMBlockDescriptor[Compared_Job_Id].NvMBlockJobPriority){
              Standard_Job_Queue[counter] = Standard_Job_Queue[NVM_SIZE_STANDARD_JOB_QUEUE -1] ;
              counter = NVM_SIZE_STANDARD_JOB_QUEUE ;
            }
            else{
              break ;
            }
          }
          // if counter != 0
          else{
            Compared_Job_Id = Standard_Job_Queue[counter-1].Block_Id ;
            if(NvMBlockDescriptor[Job.Block_Id].NvMBlockJobPriority < NvMBlockDescriptor[Compared_Job_Id].NvMBlockJobPriority){
              Standard_Job_Queue[counter] = Standard_Job_Queue[counter-1] ;
            }
            else{
              break ;
            }
          }
        }
        Standard_Job_Queue[counter] = Job ;

    /*
     * [SWS_NvM_00379]
     * If priority based job processing is disabled via configuration,
     * the NvM module shall not support immediate write jobs. In this case,
     * the NvM module processes all jobs in FCFS order
     */
      #else
        Standard_Job_Queue[Stand_Queue_Indeces.Tail] = Job ;
      #endif
    }

    Stand_Queue_Indeces.Tail++ ;

    //When Tail reaches queue end
    if(Stand_Queue_Indeces.Tail == NVM_SIZE_STANDARD_JOB_QUEUE){
      Stand_Queue_Indeces.Tail = 0 ;
    }

      //When Tail reaches Head while enqueing, the queue is full
    if(Stand_Queue_Indeces.Tail == Stand_Queue_Indeces.Head){
      Standard_Queue_FULL = TRUE ;
    }
  }
  return E_OK;
}

/****************************************************************************************/
/*    Function Name           : Job_Dequeue                                             */
/*    Function Description    : take a job out of the queue to be executed              */
/*    Parameter in            : none                                                    */
/*    Parameter inout         : none                                                    */
/*    Parameter out           : Job                                                     */
/*    Return value            : Std_ReturnType                                          */
/****************************************************************************************/

Std_ReturnType Job_Dequeue(Job_Parameters* Job)
{
 #if (NVM_JOB_PRIORITIZATION == STD_ON)
   //Immediate queue is not empty, so dequeue immediate job
   if(Immediate_Queue_Empty == FALSE){

     *Job = Immediate_Job_Queue[Immed_Queue_Indeces.Head] ;
     Immediate_Job_Queue[Immed_Queue_Indeces.Head].Block_Id = 0 ;
     Immediate_Job_Queue[Immed_Queue_Indeces.Head].ServiceId = NVM_INIT_API_ID ;
     Immediate_Job_Queue[Immed_Queue_Indeces.Head].RAM_Ptr = NULL ;

     if(Immediate_Queue_FULL == TRUE){
       Immediate_Queue_FULL = FALSE ;
     }

     Immed_Queue_Indeces.Head++ ;

     if(Immed_Queue_Indeces.Head == NVM_SIZE_IMMEDIATE_JOB_QUEUE){
       Immed_Queue_Indeces.Head = 0 ;
     }

     if(Immed_Queue_Indeces.Head == Immed_Queue_Indeces.Tail){
       Immediate_Queue_Empty = TRUE ;
     }
     return E_OK ;
  }
 #endif

  //Immediate queue is empty and standard queue is empty, so return error
  if(Standard_Queue_Empty == TRUE){
    return E_NOT_OK ;
  }
  //Immediate queue is empty and standard queue is not empty,
  //so dequeue standard jobs
  else{
    *Job = Standard_Job_Queue[Stand_Queue_Indeces.Head] ;
    Standard_Job_Queue[Stand_Queue_Indeces.Head].Block_Id = 0 ;
    Standard_Job_Queue[Stand_Queue_Indeces.Head].ServiceId = NVM_INIT_API_ID ;
    Standard_Job_Queue[Stand_Queue_Indeces.Head].RAM_Ptr = NULL ;

    if(Standard_Queue_FULL == TRUE){
      Standard_Queue_FULL = FALSE ;
    }

    Stand_Queue_Indeces.Head++ ;

    if(Stand_Queue_Indeces.Head == NVM_SIZE_STANDARD_JOB_QUEUE){
      Stand_Queue_Indeces.Head = 0 ;
    }

    if(Stand_Queue_Indeces.Head == Stand_Queue_Indeces.Tail){
      Standard_Queue_Empty = TRUE ;
    }

    return E_OK ;
  }

}

/*****************************************************************************************/
/*                              Global Functions Implementation                          */
/*****************************************************************************************/

/****************************************************************************************/
/*    Function Name           : NvM_WriteBlock                                          */
/*    Function Description    : Service to copy the data of the RAM block to its        */
/*                              corresponding NV block.                                 */
/*    Parameter in            : BlockId, NvM_SrcPtr                                     */
/*    Parameter inout         : none                                                    */
/*    Parameter out           : none                                                    */
/*    Return value            : Std_ReturnType                                          */
/*    Requirement              : SWS_NvM_00455                                          */
/*    Notes                   :                                                         */
/****************************************************************************************/
Std_ReturnType NvM_WriteBlock( NvM_BlockIdType BlockId, const void* NvM_SrcPtr )
{
    /*Variable to save return value*/
    Std_ReturnType Return_Val = E_OK ;


    /* [SWS_NvM_00619] If development error detection is enabled for NvM module,
     * the function NvM_WriteBlock shall report the DET error NVM_E_NOT_INITIALIZED
     * when NVM not yet initialized.
     */
    if(ModuleState == MODULE_UNINITIALIZED){
       #if(NVM_DEV_ERROR_DETECT == STD_ON)
           Det_ReportError(NVRAM_MANAGER_ID, NVRAM_MANAGER_INSTANCE, NVM_WRITEBLOCK_API_ID, NVM_E_NOT_INITIALIZED) ;
       #endif
       Return_Val = E_NOT_OK ;
    }

    /* [SWS_NvM_00620]
     * If development error detection is enabled for NvM module,
     * the function NvM_WriteBlock shall report the DET error NVM_E_BLOCK_PENDING
     * when NVRAM block identifier is already queued or currently in progress.
     * */
    else if(Search_Queue(BlockId) == E_OK){
            #if(NVM_DEV_ERROR_DETECT == STD_ON)
                Det_ReportError(NVRAM_MANAGER_ID, NVRAM_MANAGER_INSTANCE, NVM_WRITEBLOCK_API_ID, NVM_E_BLOCK_PENDING) ;
            #endif
            Return_Val = E_NOT_OK ;
    }

    /* [SWS_NvM_00622] If development error detection is enabled for NvM module,
     * the function NvM_WriteBlock shall report the DET error NVM_E_PARAM_ADDRESS
     * when no permanent RAM block and no explicit synchronization are configured,
     * and a NULL pointer is passed via the parameter NvM_SrcPtr
     */
    else if((NvMBlockDescriptor[BlockId].NvMRamBlockDataAddress == NULL) && (NvMBlockDescriptor[BlockId].NvMBlockUseSyncMechanism == FALSE) && (NvM_SrcPtr == NULL)){
             #if(NVM_DEV_ERROR_DETECT == STD_ON)
                 Det_ReportError(NVRAM_MANAGER_ID, NVRAM_MANAGER_INSTANCE, NVM_WRITEBLOCK_API_ID, NVM_E_PARAM_ADDRESS) ;
             #endif
    }
    /* [SWS_NvM_00624]
     * If development error detection is enabled for NvM module,
     * the function NvM_WriteBlock shall report the DET error NVM_E_PARAM_BLOCK_ID
     * when the passed BlockID is out of range
     */
    else if(BlockId > NUMBER_OF_NVM_BLOCKS){
            #if(NVM_DEV_ERROR_DETECT == STD_ON)
                Det_ReportError(NVRAM_MANAGER_ID, NVRAM_MANAGER_INSTANCE, NVM_WRITEBLOCK_API_ID, NVM_E_PARAM_BLOCK_ID) ;
            #endif
    }


    /* [SWS_NvM_00411]
     * The function NvM_WriteBlock shall test the write protection attribute of the
     * NV block in the administrative part of the corresponding RAM block.
     * In case of failure an NVM_E_WRITE_PROTECTED / (during production) error shall be reported.
     */
//    else if(AdministrativeBlock[BlockId].WriteProtect == TRUE){
//            /*Report error to the Dem Module*/
//            /* [SWS_NvM_00217] */
//            Return_Val = E_NOT_OK ;
//    }

    /* [SWS_NvM_00208]
     * The function NvM_WriteBlock shall take over the given parameters,
     * queue the write request in the job queue and return.
     */
    else{

        Job_Parameters WriteJob ;
        WriteJob.ServiceId = NVM_WRITEBLOCK_API_ID ;
        WriteJob.Block_Id = BlockId ;

        /* [SWS_NvM_00210]
         * If the function NvM_WriteBlock is provided with a valid RAM block address then it is used
         */
        if(NvM_SrcPtr != NULL){
            WriteJob.RAM_Ptr = (void*)NvM_SrcPtr ;
        }

        /*[SWS_NvM_00900]
         * If the function NvM_WriteBlock is provided with NULL_PTR as a RAM block address
         * and it has a permanent RAM block configured then the permanent RAM block is used.
         */
        else if(NvMBlockDescriptor[BlockId].NvMRamBlockDataAddress != NULL){
                WriteJob.RAM_Ptr = (uint32*)NvMBlockDescriptor[BlockId].NvMRamBlockDataAddress ;
        }

        /*[SWS_NvM_00901]
         * If the function NvM_WriteBlock is provided with NULL_PTR as a RAM block address
         * and it has the explicit synchronization configured then the explicit synchronization is used
         */
        else if (NvMBlockDescriptor[BlockId].NvMBlockUseSyncMechanism == TRUE){
                 /*Use explicit sync*/
                 /*Use RAM mirror pointer*/
        }

        Return_Val = Job_Enqueue(WriteJob) ;

        /* [SWS_NvM_00300]
         * The function NvM_WriteBlock shall cancel a pending job immediately in a destructive way
         * if the passed BlockId references a NVRAM block configured to have immediate priority.
         * The immediate job shall be the next active job to be processed
         */
        if(Return_Val == E_OK && NvMBlockDescriptor[BlockId].NvMBlockJobPriority == 0){
           /*Cancel a pending job if it is not also an immediate job*/
           if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockJobPriority != 0){
               NvM_CancelJobs(Current_Job.Block_Id) ;
           }
        }
    }

    return Return_Val;
}


/****************************************************************************************/
/*    Function Name           : NvM_Main_Write                                          */
/*    Function Description    : Local Service to to execute write jobs                  */
/*    Parameter in            : none                                                    */
/*    Parameter inout         : none                                                    */
/*    Parameter out           : none                                                    */
/*    Return value            : none                                                    */
/*    Requirement             : SWS_NvM_00455 & SWS_NvM_00464                           */
/*    Notes                   :                                                         */
/****************************************************************************************/

void NvM_Main_Write(void)
{
    static uint32 CRC_Val = 0 ;
    static uint8 Retry_Counter = 0;
    static uint8 redundant_block_Num = 0;
    uint16 counter = 0 ;

    /* Variable to hold the current state of the function
     * We have 3 states:
     *** 0 -> Initial state : Send the Write request to the Memory interface layer and wait until the result is not pending.
     *** 1 -> the write is done and result is OK (Memory interface returns MEMIF_JOB_OK)
     *** 2 -> the write can't be done (Memory interface returns MEMIF_JOB_FAILED or the write request itself returns E_NOT_OK)
     */
    static uint8 Current_State = 0;

    uint16 Fee_Ea_Block_Num ;

    switch (Current_State){

     /* Case 0 :
      * Initial state
      */
     case 0 :

         /* [SWS_NvM_00212]
          * The job of the function NvM_WriteBlock shall request a CRC recalculation
          * before the RAM block will be copied to NV memory if the NV block is configured with CRC
          */
         if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockUseCrc == TRUE){

             /* [SWS_NvM_00852]
              * The job of the function NvM_WriteBlock shall skip writing and consider the job as
              * successfully finished if the NvMBlockUseCRCCompMechanism attribute of the NVRAM Block
              * is set to true and the RAM block CRC calculated by the write job is equal to the CRC
              * calculated during the last successful read or write job.
              * This mechanism shall not be applied to blocks for which a loss of redundancy has been detected.
              */

             /* if the RAM block is not permanent , calculate CRC immediately */

             if(NvMBlockDescriptor[Current_Job.Block_Id].NvMRamBlockDataAddress != Current_Job.RAM_Ptr){

                /* CRC_Val = Calculate_CRC() ; */
                if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockUseCRCCompMechanism == TRUE){

                   if(CRC_Val == AdministrativeBlock[Current_Job.Block_Id].PrevCRCVal){

                        redundant_block_Num = 1 ;
                        Current_State = 1 ;
                        break ;

                    }
                }

             }

             /* ECUC_NvM_00119 */
             /*else if the ram block is permanent and NvMCalcRamBlockCrc is true , calculate CRC */
             else if(NvMBlockDescriptor[Current_Job.Block_Id].NvMCalcRamBlockCrc == TRUE){

                 /* CRC_Val = Calculate_CRC() ; */
                 if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockUseCRCCompMechanism == TRUE){

                    if(CRC_Val == AdministrativeBlock[Current_Job.Block_Id].PrevCRCVal){

                       redundant_block_Num = 1 ;
                       Current_State = 1 ;
                       break ;

                    }
                 }
             }
         }

         if(MemIf_GetStatus(NvMBlockDescriptor[Current_Job.Block_Id].NvMNvramDeviceId) == MEMIF_IDLE){

            if(CRC_Val != 0){

               for (counter = 0 ; counter < NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockLength ; counter ++){

                   TempBuffer[counter] = *((uint8 *)Current_Job.RAM_Ptr) ;
                   (uint8 *)(Current_Job.RAM_Ptr) ++;

               }

               TempBuffer[NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockLength] = (CRC_Val & 0xFF000000) ;
               TempBuffer[NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockLength + 1] = (CRC_Val & 0x00FF0000) ;
               TempBuffer[NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockLength + 1] = (CRC_Val & 0x0000FF00) ;
               TempBuffer[NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockLength + 1] = (CRC_Val & 0x000000FF) ;

            }

            /* [SWS_NvM_00303]
             * The job of the function NvM_WriteBlock shall assume a referenced permanent RAM block or the RAM mirror
             * in the NvM module in case of explicit synchronization to be valid when the request is passed to the NvM module.
             * If the permanent RAM block is still in an invalid state, the function NvM_WriteBlock shall validate it automatically
             * before copying the RAM block contents to NV memory or after calling explicit synchronization callback
             */
            if((NvMBlockDescriptor[Current_Job.Block_Id].NvMRamBlockDataAddress == Current_Job.RAM_Ptr) && (AdministrativeBlock[Current_Job.Block_Id].PRAMStatus != VALID)){

                AdministrativeBlock[Current_Job.Block_Id].PRAMStatus = VALID ;

            }

            /*Calculate FEE/EA Block Number to send to the MemIf Module*/
            /*Native Block*/
            if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockManagement == NVM_BLOCK_NATIVE){

               Fee_Ea_Block_Num = NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockBaseNumber << NVM_DATASET_SELECTION_BITS ;

            }

            /* [SWS_NvM_00338]
             * The job of the function NvM_WriteBlock shall copy the RAM block to the corresponding NV block
             * which is selected via the data index in the administrative block
             * if the NVRAM block management type of the given NVRAM block is NVM_BLOCK_DATASET.
             */
            /*DataSet Block*/
            else if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockManagement == NVM_BLOCK_DATASET){

                Fee_Ea_Block_Num = (NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockBaseNumber << NVM_DATASET_SELECTION_BITS) + AdministrativeBlock[Current_Job.Block_Id].DataSetIndex ;

            }
            /* [SWS_NvM_00760] The job of the function NvM_WriteBlock shall copy the data content of the RAM block
             * to both corresponding NV blocks if the NVRAM block management type of the processed NVRAM block
             * is NVM_BLOCK_REDUNDANT.
             */
            /*Redundant Block*/
            else if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockManagement == NVM_BLOCK_REDUNDANT){
                Fee_Ea_Block_Num = (NvMBlockDescriptor[Current_Job.Block_Id].NvMNvBlockBaseNumber << NVM_DATASET_SELECTION_BITS) + redundant_block_Num ;
            }

            Std_ReturnType InitWrite ;

            /*Call MemIf_Write function*/
            if(CRC_Val != 0){

                InitWrite = MemIf_Write(NvMBlockDescriptor[Current_Job.Block_Id].NvMNvramDeviceId, Fee_Ea_Block_Num, TempBuffer) ;

            }
            else{

                InitWrite = MemIf_Write(NvMBlockDescriptor[Current_Job.Block_Id].NvMNvramDeviceId, Fee_Ea_Block_Num, Current_Job.RAM_Ptr) ;

            }


            AdministrativeBlock[Current_Job.Block_Id].BlockStatus = NVM_REQ_PENDING ;

            if(InitWrite == E_NOT_OK){
                /*Job Failed*/
                Current_State = 2 ;
            }
         }

         /* Case 1 : If the job is done and result is OK */
         if(MemIf_GetJobResult(NvMBlockDescriptor[Current_Job.Block_Id].NvMNvramDeviceId) == MEMIF_JOB_OK){
             Current_State = 1 ;
         }
         /* Case 2 : If the job failed */
         else if (MemIf_GetJobResult(NvMBlockDescriptor[Current_Job.Block_Id].NvMNvramDeviceId) == MEMIF_JOB_FAILED){
                  Current_State = 2 ;
         }

         break ;

     /*Case 1 :
      * If the job is done and result is OK
      */
     case 1 :

         Retry_Counter = 0 ;
         Current_State = 0 ;

         /* [SWS_NvM_00284]
          * The job of the function NvM_WriteBlock shall set NVM_REQ_OK as request result
          * if the passed BlockId references a NVRAM block of type NVM_BLOCK_REDUNDANT
          * and at least one of the NV blocks has been written successfully.
          */

         if(NvMBlockDescriptor[Current_Job.Block_Id].NvMBlockManagement == NVM_BLOCK_REDUNDANT && redundant_block_Num == 0){

            redundant_block_Num = 1 ;
            AdministrativeBlock[Current_Job.Block_Id].BlockStatus = NVM_REQ_OK ;

         }

         else{

             CRC_Val = 0 ;
             redundant_block_Num = 0;
             AdministrativeBlock[Current_Job.Block_Id].BlockStatus = NVM_REQ_OK ;
             AdministrativeBlock[Current_Job.Block_Id].PRAMStatus = INVALID ;

             if(CRC_Val != 0){
                 AdministrativeBlock[Current_Job.Block_Id].PrevCRCVal = CRC_Val ;
                 CRC_Val = 0 ;
             }

             Job_Dequeue( &Current_Job ) ;

             if(Standard_Queue_Empty != TRUE){
                 Current_Job = Standard_Job_Queue[Stand_Queue_Indeces.Head] ;
             }
             else {
                 Current_Job.ServiceId = NVM_INIT_API_ID ;
                 Current_Job.Block_Id = 0 ;
                 Current_Job.RAM_Ptr = NULL ;
             }

         }

         break ;

     /* Case 2 :
      * If the job failed
      */
     case 2 :

         /* [SWS_NvM_00213]
          * The job of the function NvM_WriteBlock shall check the number of write retries using a write retry counter
          * to avoid infinite loops. Each negative result reported by the memory interface shall be followed by
          * an increment of the retry counter. In case of a retry counter overrun,
          * the job of the function NvM_WriteBlock shall set the request result to NVM_REQ_NOT_OK.
          */
         Retry_Counter++ ;
         Current_State = 0 ;

         if(Retry_Counter >= NvMBlockDescriptor[Current_Job.Block_Id].NvMMaxNumOfWriteRetries){

             AdministrativeBlock[Current_Job.Block_Id].BlockStatus = NVM_REQ_NOT_OK ;
             AdministrativeBlock[Current_Job.Block_Id].PRAMStatus = INVALID ;

             /*report NVM_E_REQ_FAILED to the DEM.*/

             CRC_Val = 0 ;
             Retry_Counter = 0 ;
             redundant_block_Num = 0 ;

             if(CRC_Val != 0){
                 AdministrativeBlock[Current_Job.Block_Id].PrevCRCVal = CRC_Val ;
                 CRC_Val = 0 ;
             }

             Job_Dequeue( &Current_Job ) ;

             if(Standard_Queue_Empty != TRUE){
             Current_Job = Standard_Job_Queue[Stand_Queue_Indeces.Head] ;
             }
             else {

                 Current_Job.ServiceId = NVM_INIT_API_ID ;
                 Current_Job.Block_Id = 0 ;
                 Current_Job.RAM_Ptr = NULL ;
             }

         }

         break ;
    }

}




