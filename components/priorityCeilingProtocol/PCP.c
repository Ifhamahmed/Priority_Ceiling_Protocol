// ***************************** File Includes *********************************** //
#include "PCP.h"
// ******************************************************************************* //
// **************************** Data Structures ********************************** //
// ******************************************************************************* //
// ******************************* Globals *************************************** //
static List_t xCeilingMutexListVar;
static List_t * xCeilingMutexList = &xCeilingMutexListVar;

#ifdef ESP_TRACE_CONFIG
esp_timer_handle_t traceTimer;
int64_t eventArray[MAX_NUM_OF_EVENTS][4];
int64_t eventIndex = 0;
void espTimerCallback(void *arg)  {return;}
void vApplicationIdleHook(void) {return;}
#endif
// ******************************************************************************* //

// ******************** Private Function Declarations **************************** //
static void PCPAddMutexToList(xCeilingMutexHandle * xMutexHandle);
static void PCPInitTCBListItem(extTCB_t * xTCB);
static void PCPInitMutexListItem(xCeilingMutexHandle * xMutexHandle);

#ifdef ESP_TRACE_CONFIG
void ESPSwitchedOutTrace(BaseType_t xTaskNumber, BaseType_t xTCBNumber);
void ESPSwitchedInTrace(BaseType_t xTaskNumber, BaseType_t xTCBNumber, TaskHandle_t xTaskHandle);
void ESPSwitchedInTraceNative(BaseType_t xTaskNumber, BaseType_t xTCBNumber, BaseType_t noOfMutexesHeld);
void ESPCSEnterTrace(BaseType_t xTaskNumber, BaseType_t noOfMutexesHeld);
void ESPCSExitTrace(BaseType_t xTaskNumber, BaseType_t noOfMutexesHeld);
#endif
// ******************************************************************************* //

// ******************** Private Function Definitions ***************************** //
static void PCPAddMutexToList(xCeilingMutexHandle * xMutexHandle)
{
    vListInitialiseItem(&xMutexHandle->xMutexListItem);

    listSET_LIST_ITEM_OWNER(&xMutexHandle->xMutexListItem, xMutexHandle);
    listSET_LIST_ITEM_VALUE(&xMutexHandle->xMutexListItem, xMutexHandle->xCeilingPriority);

    // insert to list
    vListInsert(xCeilingMutexList, &xMutexHandle->xMutexListItem);
}

static void PCPInitTCBListItem(extTCB_t * xTCB)
{
    // Init the list item
    vListInitialiseItem(&xTCB->xTaskListItem);
    listSET_LIST_ITEM_OWNER(&xTCB->xTaskListItem, xTCB);
    listSET_LIST_ITEM_VALUE(&xTCB->xTaskListItem, xTCB->xActivePriority);
}

static void PCPInitMutexListItem(xCeilingMutexHandle * xMutexHandle)
{
    vListInitialiseItem(&xMutexHandle->xMutexListItemHeldList);
    listSET_LIST_ITEM_OWNER(&xMutexHandle->xMutexListItemHeldList, xMutexHandle);
    listSET_LIST_ITEM_VALUE(&xMutexHandle->xMutexListItemHeldList, xMutexHandle->xCeilingPriority);
}

// ******************************************************************************* //
// ********************* Public Function Definitions ***************************** //
void PCPRegisterTask(TaskHandle_t * xTaskHandle, const char * taskName)
{
    extTCB_t * taskNode = (extTCB_t *)malloc(sizeof(extTCB_t));
    
    if (taskNode == NULL)
    {
        printf("[INFO] Memory could not be allocated.....\n");
        return;
    }
    
    taskNode->xTaskHandle = xTaskHandle;
    taskNode->xActivePriority = uxTaskPriorityGet(*xTaskHandle);
    taskNode->xBasePriority = uxTaskPriorityGet(*xTaskHandle);
    taskNode->xTaskName = taskName;
    vListInitialise(&taskNode->xMutexesHeld);
    
    // Init the TCB list item for global mutex storage
    PCPInitTCBListItem(taskNode);

    // Store task priorities in Task Local Storage
    vTaskSetThreadLocalStoragePointer(*xTaskHandle, LOCAL_STORAGE_INDEX, taskNode);
}

xCeilingMutexHandle * PCPCreateCeilingMutex(BaseType_t ceilingPriority, SemaphoreHandle_t * xMutexhandle)
{
    xCeilingMutexHandle * xCeilingMutexNode = (xCeilingMutexHandle *)malloc(sizeof(xCeilingMutexHandle));

    if ((xCeilingMutexNode == NULL))
    {
        printf("[INFO] Memory could not be allocated.....\n");
        vPortFree(xCeilingMutexNode);
        return xCeilingMutexNode;
    }

    xCeilingMutexNode->xCeilingPriority = ceilingPriority;
    xCeilingMutexNode->xMutexhandle = xMutexhandle;
    xCeilingMutexNode->xCurrentHolder = NULL;
    vListInitialise(&xCeilingMutexNode->xTasksWaitingForMutex);

    // Init the list item for TCB Local Storage
    PCPInitMutexListItem(xCeilingMutexNode);

    // Add mutex to global list for storage
    PCPAddMutexToList(xCeilingMutexNode);

    return xCeilingMutexNode;
}

BaseType_t PCPCeilingMutexTake(xCeilingMutexHandle * xCeilingMutex)
{
    // Loop thru the list to check ceiling rule
    // if priority greater than ceiling priority of all remaining taken semaphores, allow taking of sem

    // Also make sure that priority is inherited by task causing the block if it lower priority than calling task 
    ListItem_t * xMutexListItem = listGET_HEAD_ENTRY(xCeilingMutexList);
    const ListItem_t * xMutexListEndMarker = listGET_END_MARKER(xCeilingMutexList);

    BaseType_t accessAllowed = pdTRUE;
    BaseType_t result = pdFAIL;

    TaskHandle_t callingTask = xTaskGetCurrentTaskHandle();

    // get task list item
    extTCB_t * xCallingTCB = (extTCB_t *)pvTaskGetThreadLocalStoragePointer(callingTask, LOCAL_STORAGE_INDEX);

    // held mutex with highest ceiling prio
    xCeilingMutexHandle * xMutexhandle = NULL;
    xCeilingMutexHandle * xMutexhandleTmp = NULL;
    BaseType_t highestHeldCeilingPrio = 0;


    // check ceiling property, includes direct blocking
    while (xMutexListItem != xMutexListEndMarker)
    {
        xMutexhandleTmp = listGET_LIST_ITEM_OWNER(xMutexListItem);
        xMutexListItem = listGET_NEXT(xMutexListItem);

        if (xMutexhandleTmp->xCurrentHolder != NULL)
        {
            // Mutex is held
            if (xMutexhandleTmp->xCurrentHolder != xCallingTCB->xTaskHandle)
            {
                // Mutex held by task other than calling task
                if (xMutexhandleTmp->xCeilingPriority > highestHeldCeilingPrio)
                {
                    highestHeldCeilingPrio = xMutexhandleTmp->xCeilingPriority;
                    xMutexhandle = xMutexhandleTmp;
                }
            }
        }
    }

    if (xCallingTCB->xActivePriority <= highestHeldCeilingPrio)
    {
        accessAllowed = pdFALSE;
    }

    if (accessAllowed == pdFALSE)
    {
        printf("[INFO] Access not allowed\n");
        // Access not allowed due to failure to fulfill ceiling property
        // prepare to block task: transmit priority to task holding the requested mutex
        // Transmit priority here, inheritance
        // This ensures chain transmission/inheritance

        // get current holder TCB struct
        TaskHandle_t * mutexCurrentHolder = xMutexhandle->xCurrentHolder;
        extTCB_t * xMutexCurrentHolderTCB = (extTCB_t *)pvTaskGetThreadLocalStoragePointer(*mutexCurrentHolder, LOCAL_STORAGE_INDEX);

        BaseType_t prioToTransmit = xCallingTCB->xActivePriority;
        //if ((xMutexhandle->xCurrentHolder)->xActivePriority < prioToTransmit)
        if (xMutexCurrentHolderTCB->xActivePriority < prioToTransmit)
        {
            printf("[INFO] Task: %s priority increased from %d to %d\n", xMutexCurrentHolderTCB->xTaskName, xMutexCurrentHolderTCB->xActivePriority, prioToTransmit);
            xMutexCurrentHolderTCB->xActivePriority = prioToTransmit;

            // Increase current holder priority
            vTaskPrioritySet(*(xMutexhandle->xCurrentHolder), prioToTransmit);
        }

        // Add calling task to waiting list
        // get task list item
        vListInsert(&xCeilingMutex->xTasksWaitingForMutex, &xCallingTCB->xTaskListItem);

        // Blocking takes place here, block current task
        vTaskSuspend(NULL);
        // If task is resumed when the requested mutex is available, the code resumes from this point
    }

    // indicate mutex as held and update fields
    xCeilingMutex->xCurrentHolder = xCallingTCB->xTaskHandle;

    // insert mutex into TCB's list to show that calling task holds this mutex
    vListInsert(&xCallingTCB->xMutexesHeld, &xCeilingMutex->xMutexListItemHeldList);

    result = xSemaphoreTake(*(xCeilingMutex->xMutexhandle), DEFAULT_DELAY);

    #ifdef ESP_TRACE_CONFIG
    BaseType_t noOfMutexesHeld = listCURRENT_LIST_LENGTH(&xCallingTCB->xMutexesHeld);
    BaseType_t taskNumber = uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle());
    ESPCSEnterTrace(taskNumber, noOfMutexesHeld);
    #endif

    return result;
}

BaseType_t PCPCeilingMutexGive(xCeilingMutexHandle * xCeilingMutex)
{
    // give back semaphore directly
    BaseType_t result;
    // Revert priority to base prio, disinheritance if needed
    // obtain calling task TCB
    extTCB_t * xCallingTaskTCB = (extTCB_t *)pvTaskGetThreadLocalStoragePointer(*xCeilingMutex->xCurrentHolder, LOCAL_STORAGE_INDEX);

    // remove the current given mutex from held list in calling tasks TCB
    uxListRemove(&xCeilingMutex->xMutexListItemHeldList); 

    // Check for chained disinheritance
    ListItem_t * xTCBMutexListItem = listGET_HEAD_ENTRY(&xCallingTaskTCB->xMutexesHeld);
    ListItem_t * xTCBMutexListItemEndMarker = listGET_END_MARKER(&xCallingTaskTCB->xMutexesHeld);

    if (xTCBMutexListItem != xTCBMutexListItemEndMarker)
    {
        // needs possible chained disinheritance as calling task holds another mutex
        // get mutex with highest ceiling priority that calling task holds
        xCeilingMutexHandle * xHeldCeilingMutex = listGET_LIST_ITEM_OWNER(xTCBMutexListItem);
        // Obtain highest priority task waiting on this mutex
        ListItem_t * xHighestPrioTaskListItem = listGET_HEAD_ENTRY(&xHeldCeilingMutex->xTasksWaitingForMutex);
        ListItem_t * xHighestPrioTaskListEndMarker = listGET_HEAD_ENTRY(&xHeldCeilingMutex->xTasksWaitingForMutex);

        if (xHighestPrioTaskListItem != xHighestPrioTaskListEndMarker) 
        // TODO 
        // if this condition fails, disinheritance is not performed, rectify
        {
            // needs chained disinheritance
            extTCB_t * xHighestPrioTaskOnHeldMutex = listGET_LIST_ITEM_OWNER(xHighestPrioTaskListItem);
            BaseType_t prioToTransmit = xHighestPrioTaskOnHeldMutex->xActivePriority;

            // Perform Chained disinheritance
            if (xCallingTaskTCB->xActivePriority < prioToTransmit)
            {
                xHighestPrioTaskOnHeldMutex->xActivePriority = prioToTransmit;
                printf("[INFO] Task: %s holds another mutex, priority decreased from %d to %d\n", xCallingTaskTCB->xTaskName, xCallingTaskTCB->xActivePriority, xCallingTaskTCB->xBasePriority);
                vTaskPrioritySet(*xCallingTaskTCB->xTaskHandle, xHighestPrioTaskOnHeldMutex->xActivePriority);
            }
        }
        else
        {
            // No chained disinheritance
            // disinheritance to base prio, if needed
            if (xCallingTaskTCB->xActivePriority > xCallingTaskTCB->xBasePriority)
            {
                printf("[INFO] Task: %s priority decreased from %d to %d\n", xCallingTaskTCB->xTaskName, xCallingTaskTCB->xActivePriority, xCallingTaskTCB->xBasePriority);
                vTaskPrioritySet(*xCallingTaskTCB->xTaskHandle, xCallingTaskTCB->xBasePriority);
            }
        }
    }
    else
    {
        // No chained disinheritance
        // disinheritance to base prio, if needed
        if (xCallingTaskTCB->xActivePriority > xCallingTaskTCB->xBasePriority)
        {
            printf("[INFO] Task: %s priority decreased from %d to %d\n", xCallingTaskTCB->xTaskName, xCallingTaskTCB->xActivePriority, xCallingTaskTCB->xBasePriority);
            vTaskPrioritySet(*xCallingTaskTCB->xTaskHandle, xCallingTaskTCB->xBasePriority);
        }
    }
    // Indicate the mutex as returned
    xCeilingMutex->xCurrentHolder = NULL;


    // resume highest priority task waiting for this mutex
    ListItem_t * xTCBListItem = listGET_HEAD_ENTRY(&xCeilingMutex->xTasksWaitingForMutex);
    ListItem_t * xListEndMarker = listGET_END_MARKER(&xCeilingMutex->xTasksWaitingForMutex);

    if (xTCBListItem != xListEndMarker)
    {
        // Task(s) waiting on this mutex
        // resume highest priority task
        extTCB_t * xTCB = listGET_LIST_ITEM_OWNER(xTCBListItem);
        // Remove task from waiting list
        uxListRemove(xTCBListItem);
        // resume the task since mutex is now available
        vTaskResume(*xTCB->xTaskHandle);
    }
    else
    {
        // No Tasks waiting on this mutex
        // Do nothing extra
    }

    // Give Mutex back
    result = xSemaphoreGive(*(xCeilingMutex->xMutexhandle));

    #ifdef ESP_TRACE_CONFIG
    BaseType_t noOfMutexesHeld = listCURRENT_LIST_LENGTH(&xCallingTaskTCB->xMutexesHeld);
    BaseType_t taskNumber = uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle());
    ESPCSExitTrace(taskNumber, noOfMutexesHeld);
    #endif

    return result;

}

void PCPInit()
{
    #ifdef ESP_TRACE_CONFIG
    memset(eventArray, 0, sizeof(eventArray));
    const esp_timer_create_args_t timer_args = {
            .callback = &espTimerCallback,
            .name = "traceTimer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &traceTimer));
    ESP_ERROR_CHECK(esp_timer_start_once(traceTimer, TRACE_TIMER_TIMEOUT));
    #endif

    vTaskPrioritySet(NULL, MAX_SYS_PRIO);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    vListInitialise(xCeilingMutexList);
}
// ********************************************************************* //
// ******************* ESP TRACE Funtions ****************************** //
#ifdef ESP_TRACE_CONFIG

void ESPSwitchedOutTrace(BaseType_t xTaskNumber, BaseType_t xTCBNumber)
{
    if (eventIndex < MAX_NUM_OF_EVENTS)
    {
        // Periodic, Aperiodic Tasks and the Scheduler
        if (xTaskNumber != 0)
        {
            eventArray[eventIndex][2] = esp_timer_get_time();
            eventIndex++;
        }
        // Idle Task
        else if ((xTCBNumber == IDLE_TASK_NUM) && (xTaskNumber == 0))
        {
            eventArray[eventIndex][2] = esp_timer_get_time();
            eventIndex++;
        }
    }
}

void ESPSwitchedInTrace(BaseType_t xTaskNumber, BaseType_t xTCBNumber, TaskHandle_t xTaskHandle)
{
    if (eventIndex < MAX_NUM_OF_EVENTS)
    {
        // Periodic, Aperiodic Tasks and the Scheduler
        if (xTaskNumber != 0)
        {
            eventArray[eventIndex][0] = xTaskNumber;
            eventArray[eventIndex][1] = esp_timer_get_time();

            #if RUN_PCP == 1
            extTCB_t * xCallingTCB = (extTCB_t *)pvTaskGetThreadLocalStoragePointer(xTaskHandle, LOCAL_STORAGE_INDEX);
            BaseType_t noOfMutexesHeld = listCURRENT_LIST_LENGTH(&xCallingTCB->xMutexesHeld);
            eventArray[eventIndex][3] = noOfMutexesHeld;
            #endif

        }
        // Idle Task
        else if ((xTCBNumber == IDLE_TASK_NUM) && (xTaskNumber == 0))
        {
            eventArray[eventIndex][0] = IDLE_TASK_NUM;
            eventArray[eventIndex][1] = esp_timer_get_time();
            eventArray[eventIndex][3] = 0;
        }
    }
}

void ESPSwitchedInTraceNative(BaseType_t xTaskNumber, BaseType_t xTCBNumber, BaseType_t noOfMutexesHeld)
{
    if (eventIndex < MAX_NUM_OF_EVENTS)
    {
        // Periodic, Aperiodic Tasks and the Scheduler
        if (xTaskNumber != 0)
        {
            eventArray[eventIndex][0] = xTaskNumber;
            eventArray[eventIndex][1] = esp_timer_get_time();
            eventArray[eventIndex][3] = noOfMutexesHeld;

        }
        // Idle Task
        else if ((xTCBNumber == IDLE_TASK_NUM) && (xTaskNumber == 0))
        {
            eventArray[eventIndex][0] = IDLE_TASK_NUM;
            eventArray[eventIndex][1] = esp_timer_get_time();
            eventArray[eventIndex][3] = 0;
        }
    }
}

void ESPCSEnterTrace(BaseType_t xTaskNumber, BaseType_t noOfMutexesHeld)
{
    if (eventIndex < MAX_NUM_OF_EVENTS)
    {
        int64_t timeInstant = esp_timer_get_time();
        // end task recording
        eventArray[eventIndex][2] = timeInstant;
        eventIndex++;

        // start new record for same task with new CS field
        eventArray[eventIndex][0] = xTaskNumber;
        eventArray[eventIndex][1] = timeInstant;

        // increment number of critical sections currently accessed
        eventArray[eventIndex][3] = noOfMutexesHeld;
    }
}
void ESPCSExitTrace(BaseType_t xTaskNumber, BaseType_t noOfMutexesHeld)
{
    if (eventIndex < MAX_NUM_OF_EVENTS)
    {
        int64_t timeInstant = esp_timer_get_time();
        // end task recording
        eventArray[eventIndex][2] = timeInstant;
        eventIndex++;

        // start new record for same task with new CS field
        eventArray[eventIndex][0] = xTaskNumber;
        eventArray[eventIndex][1] = timeInstant;

        // decrement number of critical sections currently accessed
        eventArray[eventIndex][3] = noOfMutexesHeld;
    }
}

void ESPCSEnterTraceNative()
{
    if (eventIndex < MAX_NUM_OF_EVENTS)
    {
        int64_t timeInstant = esp_timer_get_time();
        BaseType_t noOfMutexesHeldCurrently = eventArray[eventIndex][3];
        BaseType_t xTaskNumber = eventArray[eventIndex][0];
        // end task recording
        eventArray[eventIndex][2] = timeInstant;
        eventIndex++;

        // start new record for same task with new CS field
        eventArray[eventIndex][0] = xTaskNumber;
        eventArray[eventIndex][1] = timeInstant;

        // increment number of critical sections currently accessed
        eventArray[eventIndex][3] = ++noOfMutexesHeldCurrently;
    }
}

void ESPCSExitTraceNative()
{
    if (eventIndex < MAX_NUM_OF_EVENTS)
    {
        int64_t timeInstant = esp_timer_get_time();
        BaseType_t noOfMutexesHeldCurrently = eventArray[eventIndex][3];
        BaseType_t xTaskNumber = eventArray[eventIndex][0];
        // end task recording
        eventArray[eventIndex][2] = timeInstant;
        eventIndex++;

        // start new record for same task with new CS field
        eventArray[eventIndex][0] = xTaskNumber;
        eventArray[eventIndex][1] = timeInstant;

        // decrement number of critical sections currently accessed
        eventArray[eventIndex][3] = --noOfMutexesHeldCurrently;
    }
}

#endif
// ******************************************************************** //
// ******************************************************************************* //


