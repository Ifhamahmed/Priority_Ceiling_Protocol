/* FreeRTOS Real Time Stats Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "PCP.h"
#include "esp_err.h"


#define NUM_OF_INSTR_NORMAL        1000000  //Actual CPU cycles used will depend on compiler optimization
#define NUM_OF_INSTR_CRITICAL      10000000
#define NUM_OF_TASKS               3
#define RUN_PCP                    0// 1: PCP 0:Only Inheritance
#define RUN_DL_EX                  1 // 0: Inheritance Example, 1: Deadlock Example

static char task_names[NUM_OF_TASKS][configMAX_TASK_NAME_LEN];
static TaskHandle_t taskHandles[NUM_OF_TASKS];

const char * task1Name = "Task 1";
const char * task2Name = "Task 2";
const char * task3Name = "Task 3";
const char * task4Name = "Task 4";
const char * task5Name = "Task 5";

// Define the Mutexes as globals
// Task 1, highest priority = 25
SemaphoreHandle_t S3_mutex;
SemaphoreHandle_t S2_mutex;
xCeilingMutexHandle * S3;
xCeilingMutexHandle * S2;

#if RUN_DL_EX == 0
// Define Task Set of 3 tasks with 3 semaphores
// Same example as in lecture
// Inheritance task set
static void Task1(void *params)
{
    for (;;)
    {
        // Normal Section 1
        vTaskDelay(1650 / portTICK_PERIOD_MS);
        printf("[INFO] Executing Task 1 Normal Section 1 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        // Critical Section S3
        #if RUN_PCP == 1
        if (PCPCeilingMutexTake(S3) == pdTRUE)
        #else
        if (xSemaphoreTake(S3_mutex, DEFAULT_DELAY) == pdTRUE)
        #endif
        {
            ESPCSEnterTraceNative();
            printf("[INFO] Executing Task 1 Critical Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            for (int i = 0; i < (NUM_OF_INSTR_CRITICAL); i++) 
            {
                __asm__ __volatile__("NOP");
            }
            printf("[INFO] Completed Task 1 Critical Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            #if RUN_PCP == 1
            PCPCeilingMutexGive(S3);
            #else
            ESPCSExitTraceNative();
            xSemaphoreGive(S3_mutex);
            #endif
        }

        // Normal Section 2
        printf("[INFO] Executing Task 1 Normal Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        // Critical Section S2
        #if RUN_PCP == 1
        if (PCPCeilingMutexTake(S2) == pdTRUE)
        #else
        if (xSemaphoreTake(S2_mutex, DEFAULT_DELAY) == pdTRUE)
        #endif
        {
            ESPCSEnterTraceNative();
            printf("[INFO] Executing Task 1 Critical Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            for (int i = 0; i < (NUM_OF_INSTR_CRITICAL); i++) 
            {
                __asm__ __volatile__("NOP");
            }
            printf("[INFO] Completed Task 1 Critical Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            #if RUN_PCP == 1
            PCPCeilingMutexGive(S2);
            #else
            ESPCSExitTraceNative();
            xSemaphoreGive(S2_mutex);
            #endif
        }

        // Normal Section 3
        printf("[INFO] Executing Task 1 Normal Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        vTaskDelete(NULL);
    }
}

static void Task2(void *params)
{
    for (;;)
    {
        vTaskDelay(1250 / portTICK_PERIOD_MS);
        // Normal Section 1
        printf("[INFO] Executing Task 2 Normal Section 1.....\n");
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        // Critical Section S2
        #if RUN_PCP == 1
        if (PCPCeilingMutexTake(S2) == pdTRUE)
        #else
        if (xSemaphoreTake(S2_mutex, DEFAULT_DELAY) == pdTRUE)
        #endif
        {
            ESPCSEnterTraceNative();
            printf("[INFO] Executing Task 2 Critical Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            for (int i = 0; i < (NUM_OF_INSTR_CRITICAL); i++) 
            {
                __asm__ __volatile__("NOP");
            }
            printf("[INFO] Completed Task 2 Critical Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            #if RUN_PCP == 1
            PCPCeilingMutexGive(S2);
            #else
            ESPCSExitTraceNative();
            xSemaphoreGive(S2_mutex);
            #endif
            
        }

        // Normal Section 2
        printf("[INFO] Executing Task 2 Normal Section 2.....\n");
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        vTaskDelete(NULL);

    }
}

static void Task3(void *params)
{
    for (;;)
    {
        // Normal Section 1
        printf("[INFO] Executing Task 3 Normal Section 1 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        // Critical Section S3
        #if RUN_PCP == 1
        if (PCPCeilingMutexTake(S3) == pdTRUE)
        #else
        if (xSemaphoreTake(S3_mutex, DEFAULT_DELAY) == pdTRUE)
        #endif
        {
            ESPCSEnterTraceNative();
            printf("[INFO] Executing Task 3 Critical Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            for (int i = 0; i < (NUM_OF_INSTR_CRITICAL * 4); i++) 
            {
                __asm__ __volatile__("NOP");
            }
            printf("[INFO] Completed Task 3 Critical Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            #if RUN_PCP == 1
            PCPCeilingMutexGive(S3);
            #else
            ESPCSExitTraceNative();
            xSemaphoreGive(S3_mutex);
            #endif
        }

        // Normal Section 2
        printf("[INFO] Executing Task 3 Normal Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        vTaskDelete(NULL);

    }
}

#else
// Deadlock example with nested critical sections
static void Task4(void *params)
{
    for (;;)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        // Normal Section 1
        printf("[INFO] Executing Task 4 Normal Section 1 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        // Critical Section S3
        #if RUN_PCP == 1
        if (PCPCeilingMutexTake(S3) == pdTRUE)
        #else
        if (xSemaphoreTake(S3_mutex, DEFAULT_DELAY) == pdTRUE)
        #endif
        {
            ESPCSEnterTraceNative();
            printf("[INFO] Executing Task 4 Critical Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            vTaskDelay(5 / portTICK_PERIOD_MS);
            for (int i = 0; i < (NUM_OF_INSTR_CRITICAL * 2); i++) 
            {
                __asm__ __volatile__("NOP");
            }
            // Critical Section S2
            #if RUN_PCP == 1
            if (PCPCeilingMutexTake(S2) == pdTRUE)
            #else
            if (xSemaphoreTake(S2_mutex, DEFAULT_DELAY) == pdTRUE)
            #endif
            {
                ESPCSEnterTraceNative();
                printf("[INFO] Executing Task 4 Nested Critical Section 3 and 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
                vTaskDelay(5 / portTICK_PERIOD_MS);
                for (int i = 0; i < (NUM_OF_INSTR_CRITICAL * 1); i++) 
                {
                    __asm__ __volatile__("NOP");
                }
                printf("[INFO] Completed Task 4 Nested Critical Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
                #if RUN_PCP == 1
                PCPCeilingMutexGive(S2);
                #else
                ESPCSExitTraceNative();
                xSemaphoreGive(S2_mutex);
                #endif
            }
            printf("[INFO] Completed Task 4 Critical Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            #if RUN_PCP == 1
            PCPCeilingMutexGive(S3);
            #else
            ESPCSExitTraceNative();
            xSemaphoreGive(S3_mutex);
            #endif
        }

        // Normal Section 2
        printf("[INFO] Executing Task 4 Normal Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }
        vTaskDelete(NULL);
    }
}

static void Task5(void *params)
{
    for (;;)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        // Normal Section 1
        printf("[INFO] Executing Task 5 Normal Section 1 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }

        // Critical Section S2
        #if RUN_PCP == 1
        if (PCPCeilingMutexTake(S2) == pdTRUE)
        #else
        if (xSemaphoreTake(S2_mutex, DEFAULT_DELAY) == pdTRUE)
        #endif
        {
            ESPCSEnterTraceNative();
            printf("[INFO] Executing Task 5 Critical Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            vTaskDelay(5 / portTICK_PERIOD_MS);
            for (int i = 0; i < (NUM_OF_INSTR_CRITICAL * 2); i++) 
            {
                __asm__ __volatile__("NOP");
            }
            // Critical Section S3
            #if RUN_PCP == 1
            if (PCPCeilingMutexTake(S3) == pdTRUE)
            #else
            if (xSemaphoreTake(S3_mutex, DEFAULT_DELAY) == pdTRUE)
            #endif
            {
                ESPCSEnterTraceNative();
                printf("[INFO] Executing Task 5 Nested Critical Section 2 and 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
                vTaskDelay(5 / portTICK_PERIOD_MS);
                for (int i = 0; i < (NUM_OF_INSTR_CRITICAL * 1); i++) 
                {
                    __asm__ __volatile__("NOP");
                }
                printf("[INFO] Completed Task 5 Nested Critical Section 3 with priority: %d.....\n", uxTaskPriorityGet(NULL));
                #if RUN_PCP == 1
                PCPCeilingMutexGive(S3);
                #else
                ESPCSExitTraceNative();
                xSemaphoreGive(S3_mutex);
                #endif
            }
            printf("[INFO] Completed Task 5 Critical Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
            #if RUN_PCP == 1
            PCPCeilingMutexGive(S2);
            #else
            ESPCSExitTraceNative();
            xSemaphoreGive(S2_mutex);
            #endif
        }

        // Normal Section 2
        printf("[INFO] Executing Task 5 Normal Section 2 with priority: %d.....\n", uxTaskPriorityGet(NULL));
        for (int i = 0; i < (NUM_OF_INSTR_NORMAL); i++) 
        {
            __asm__ __volatile__("NOP");
        }
        vTaskDelete(NULL);
    }
}

#endif

#ifdef ESP_TRACE_CONFIG
static void printFunc2(void *pvParameters)
{
    printf("********\n");
    printf("[INFO] Schedule Data Gathered. Printing data......\n");

    for (int i = 0; i < MAX_NUM_OF_EVENTS; i++)
    {
        if ((i > 1) && !eventArray[i][0] && !eventArray[i][1] && !eventArray[i][2] && !eventArray[i][3]) break;
        printf("Event No: %d, Task No: %lld, Entry at: %lld us, Exit at: %lld us after startup. Task held %lld CSs at exit\n", i, eventArray[i][0], eventArray[i][1], eventArray[i][2], eventArray[i][3]);
    }
    printf("\n[INFO] Schedule Printed!!!\n");
    printf("\n********");

    vTaskDelete(NULL);
}
#endif




void app_main(void)
{
    //Allow other core to finish initialization
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("[INFO] In Main Task.....\n");

    PCPInit();

    int TaskNumStart = TASK_NUM_START;

    BaseType_t Task1Prio = 20;
    BaseType_t Task2Prio = 19;
    BaseType_t Task3Prio = 18;
    BaseType_t Task4Prio = 20;
    BaseType_t Task5Prio = 19;

    TaskHandle_t TaskHandle1;
    TaskHandle_t TaskHandle2;
    TaskHandle_t TaskHandle3;
    TaskHandle_t TaskHandle4;
    TaskHandle_t TaskHandle5;

    // Create FreeRTOS Mutexes
    S2_mutex = xSemaphoreCreateMutex();
    S3_mutex = xSemaphoreCreateMutex();

    #if RUN_PCP == 1
    // Create the Mutexes
    // Task 1, highest priority = 25
    S3 = PCPCreateCeilingMutex(Task1Prio, &S3_mutex);
    S2 = PCPCreateCeilingMutex(Task1Prio, &S2_mutex);
    #endif

    printf("[INFO] In Main Task, created mutexes.....\n");

    // Create the tasks
    #if RUN_DL_EX == 0
    xTaskCreate(Task1, task1Name, 3000, NULL, Task1Prio, &TaskHandle1);
    vTaskSetTaskNumber(TaskHandle1, TaskNumStart++);
    xTaskCreate(Task2, task2Name, 3000, NULL, Task2Prio, &TaskHandle2);
    vTaskSetTaskNumber(TaskHandle2, TaskNumStart++);
    xTaskCreate(Task3, task3Name, 3000, NULL, Task3Prio, &TaskHandle3);
    vTaskSetTaskNumber(TaskHandle3, TaskNumStart++);
    #else
    xTaskCreate(Task4, task1Name, 3000, NULL, Task4Prio, &TaskHandle4);
    vTaskSetTaskNumber(TaskHandle4, TASK_NUM_START + 3);
    xTaskCreate(Task5, task2Name, 3000, NULL, Task5Prio, &TaskHandle5);
    vTaskSetTaskNumber(TaskHandle5, TASK_NUM_START + 4);
    #endif

    printf("[INFO] Created Tasks.....\n");
    #if RUN_PCP == 1
    // Register Tasks
    #if RUN_DL_EX == 0
    PCPRegisterTask(&TaskHandle1, task1Name);
    PCPRegisterTask(&TaskHandle2, task2Name);
    PCPRegisterTask(&TaskHandle3, task3Name);
    #else
    PCPRegisterTask(&TaskHandle4, task4Name);
    PCPRegisterTask(&TaskHandle5, task5Name);
    #endif
    #endif

    printf("[INFO] Registered tasks, passing control.....\n");
    vTaskDelay(8000/ portTICK_PERIOD_MS);
    // In this time, the created tasks will run and data will be collected on the schedule
    xTaskCreate(printFunc2, "Print Task", 2048, NULL, 1, NULL);
}
