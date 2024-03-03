#ifndef _TRACE_MACROS_H_
#define _TRACE_MACROS_H_

#define ESP_TRACE_CONFIG


#ifdef ESP_TRACE_CONFIG

#define traceTASK_SWITCHED_OUT()\
extern void ESPSwitchedOutTrace(BaseType_t xTaskNumber, BaseType_t xTCBNumber);\
ESPSwitchedOutTrace(pxCurrentTCB[0]->uxTaskNumber, pxCurrentTCB[0]->uxTCBNumber);

/*#if RUN_PCP == 1
#define traceTASK_SWITCHED_IN()\
extern void ESPSwitchedInTrace(BaseType_t xTaskNumber, BaseType_t xTCBNumber, TaskHandle_t taskHandle);\
ESPSwitchedInTrace(pxCurrentTCB[0]->uxTaskNumber, pxCurrentTCB[0]->uxTCBNumber, pxCurrentTCB[0]);
#else*/

#define traceTASK_SWITCHED_IN()\
extern void ESPSwitchedInTraceNative(BaseType_t xTaskNumber, BaseType_t xTCBNumber, BaseType_t noOfMutexesHeld);\
ESPSwitchedInTraceNative(pxCurrentTCB[0]->uxTaskNumber, pxCurrentTCB[0]->uxTCBNumber, pxCurrentTCB[0]->uxMutexesHeld);

/*#define traceQUEUE_SEND(pxQueue)\
extern void ESPCSExitTraceNative();\
ESPCSExitTraceNative();

#define traceQUEUE_RECEIVE(pxQueue)\
extern void ESPCSEnterTraceNative();\
ESPCSEnterTraceNative();*/

//#endif

#endif



#endif //_TRACE_MACROS_H_