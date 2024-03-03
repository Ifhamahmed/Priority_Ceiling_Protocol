/*
    Author: Ifham Ahmed
    Institution: Albert Ludwigs University of Freiburg
    Course: Advanced Real-Time Scheduling with FreeRTOS

    This library implements the Priority Ceiling Protocol (PCP) in User Space as a wrapper on FreeRTOS. The protocol includes 
    chained inheritance and disinheritance of task priorities. The library has not been optimized as it is only a first attempt.

    Instructions:
        The user must provide the ceiling priority to the library as the calculation of this is not possible dynamically 
        without static code analysis.

        1) Call PCPInit() to initialize the structures
        2) Create the mutexes using FreeRTOS API and use the library function to create the Ceiling Mutexes, pass ceiling priority as well
        3) Register the tasks the library needs to keep track of as part of the Ceiling Protocol
        4) Use the library Mutex Give and Take functions as normal when entering and exiting a critical section
    
    Information on Porting:
        The library is light weight and can be ported to any platform as only FreeRTOS functionality is used to implement the protocol
*/

#ifndef _PCP_H_
#define _PCP_H
// ******************************* System Includes ********************************* //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// ********************************************************************************* //
// ******************************** Freertos Includes ****************************** //
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/list.h"
#include "freertos/event_groups.h"
// ********************************************************************************* //
// ************************ Priority Ceiling Protocol Defines ********************** //
#define USE_PCP                                         1
#define LOCAL_STORAGE_INDEX                             0
#define ESP_TRACE_CONFIG
#define TASK_NUM_START                                  4 // 2 for MAIN TASK and 3 for IDLE TASK as TCB Numbers
#define IDLE_TASK_NUM                                   TASK_NUM_START - 1
#define MAIN_TASK_NUM                                   IDLE_TASK_NUM - 1
#define DEFAULT_DELAY                                   portMAX_DELAY
#define MAX_SYS_PRIO                                    configMAX_PRIORITIES - 3

// ********************************************************************************* //
// ********************************* Data Structures ******************************* //

typedef struct extTCB
{

    TaskHandle_t * xTaskHandle;
    BaseType_t xBasePriority;
    BaseType_t xActivePriority;
    ListItem_t xTaskListItem;
    List_t xMutexesHeld;  // List to store held mutexes, references xMutexListItemHeldList member of mutex struct
    const char * xTaskName;

} extTCB_t;

typedef struct CeilingMutex
{

    SemaphoreHandle_t * xMutexhandle;
    BaseType_t xCeilingPriority;
    TaskHandle_t * xCurrentHolder;
    ListItem_t xMutexListItem;
    ListItem_t xMutexListItemHeldList;
    List_t xTasksWaitingForMutex; // Tasks blocked on this mutex

} xCeilingMutexHandle;

//typedef xCeilingMutexHandle xCeilingMutexHandle_t;

#ifdef ESP_TRACE_CONFIG
#define TRACE_TIMER_TIMEOUT                 5 * 1000 * 1000 // 5 seconds max
#define MAX_NUM_OF_EVENTS                   1000           

extern int64_t eventArray[MAX_NUM_OF_EVENTS][4];     // Structure of eventArray at each index  | -- Task Number -- | -- Entry Time -- | -- Exit Time -- | -- No Of CSs Currently accessed  -- |
extern int64_t eventIndex;
extern esp_timer_handle_t traceTimer;
#endif

// ********************************************************************************* //
// **************************** Public Function Declarations *********************** //
void PCPInit();
void PCPRegisterTask(TaskHandle_t * xTaskHandle, const char * taskName);
xCeilingMutexHandle * PCPCreateCeilingMutex(BaseType_t ceilingPriority, SemaphoreHandle_t * xMutexhandle);
BaseType_t PCPCeilingMutexTake(xCeilingMutexHandle * xCeilingMutex);
BaseType_t PCPCeilingMutexGive(xCeilingMutexHandle * xCeilingMutex);

// For tracing only as it is quite difficult to gather this info for native freertos critical section implementation, not required for PCP
void ESPCSEnterTraceNative();
void ESPCSExitTraceNative();
// ********************************************************************************* //

#endif //_PCP_H