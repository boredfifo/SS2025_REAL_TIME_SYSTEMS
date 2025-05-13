#include "FreeRTOS.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "task.h"
#include "queue.h"
#include "semphr.h"

static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

TaskHandle_t Task1, Task2, Task3, Task4;
QueueHandle_t PE_queue;
SemaphoreHandle_t servertoA, servertoB, taska;

//need variables to signal that task a and b are ready, then some variables for the credit at 
//each level

volatile int credit_A = 0; //credit at Task A priority level
volatile int credit_B = 0; //credit at Task B priority level
volatile int aperiodicReq = 0;
char lastDonor;

bool TaskAready = true, TaskBready = true;



void vTaskA(void *pvParameters);
void vTaskB(void *pvParameters);
void vPEServerTask(void *pvParameters);
void aperiodicRequest(void *pvParameters);


int main(){
    srand(time(NULL)); 
    PE_queue = xQueueCreate(10, sizeof(int));
    servertoA = xSemaphoreCreateBinary();
    servertoB = xSemaphoreCreateBinary();
    if (servertoA != NULL){
        printf("Semaphore A created\n");
    }
    if (servertoB != NULL){
        printf("Semaphore B created\n");
    }
    taska = xSemaphoreCreateBinary();
    xTaskCreate(vTaskA, "Task1", 2048, NULL, 3, &Task1);
    xTaskCreate(vTaskB, "Task2", 2048, NULL, 2, &Task2);
    xTaskCreate(vPEServerTask, "Task3", 2048, NULL, 4, &Task3);
    xTaskCreate(aperiodicRequest, "Task4", 2048, NULL, 1, &Task4);
    vTaskStartScheduler();
}

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer,
    uint32_t *pulTimerTaskStackSize )
{
*ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
*ppxTimerTaskStackBuffer = xTimerStack;
*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
StackType_t **ppxIdleTaskStackBuffer,
uint32_t *pulIdleTaskStackSize)
{
*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
*ppxIdleTaskStackBuffer = xIdleStack;
*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vTaskA(void *pvParameters){
    while(1){
        //if theres an aperiodic req outside the pe server period and credit_A is more than 1 then
        //aperiodicReq gets serviced first before TaskA runs. Obviously TaskA will only run it has
        //gotten the semaphore from the PE server.
        if(xSemaphoreTake(servertoA, portMAX_DELAY)== pdTRUE){
            TaskAready = false;
            printf("Task A running\n");
            xSemaphoreGive(taska);
            
        }
        if (credit_A > 0){
            credit_A--;
        }
        vTaskDelay(pdMS_TO_TICKS(6000)); //period A
        TaskAready = true;
        printf("Task A ready\n");
    }

}
void vTaskB(void *pvParameters){
    while(1){
        if((xSemaphoreTake(taska, portMAX_DELAY) == pdTRUE)
         && (xSemaphoreTake(servertoB, portMAX_DELAY)==pdTRUE)){
            TaskBready = false;
            printf("Task B running\n");
        }
        if (credit_B > 0){
            credit_B--;
        }

        vTaskDelay(pdMS_TO_TICKS(13000)); //period B
        TaskBready = true;
        printf("Task B ready\n");
    }
}

void vPEServerTask(void *pvParameters)
{
    while (1)
    {
        if (aperiodicReq > 0)  //any request
        {
            //make sure we hold at least one credit
            if (credit_A == 0 && credit_B == 0)
            {
                if (TaskAready) { 
                    credit_A++; 
                    xSemaphoreGive(servertoA); 
                    lastDonor = 'A'; 
                }
                else if (TaskBready) { 
                    credit_B++; 
                    xSemaphoreGive(servertoB); 
                    lastDonor = 'B'; 
                }
            }

            //if we now have credit, service one request 
            if (credit_A > 0 || credit_B > 0)
            {
                printf("PE servicing Aperiodic Request\n");
                aperiodicReq--;

                if (credit_A > 0) credit_A--;
                else              credit_B--;
            }
        }
        //no request waiting build credit
        else
        {
            if (TaskAready) { 
                credit_A++; 
                xSemaphoreGive(servertoA); 
                lastDonor = 'A'; 
            }
            else if (TaskBready) { 
                credit_B++; 
                xSemaphoreGive(servertoB); 
                lastDonor = 'B'; 
                
        }

        printf("Credit A: %d, Credit B: %d  PendingReq: %d\n",
               credit_A, credit_B, aperiodicReq);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

//create a function aperiodicReq that is random
void aperiodicRequest(void *pvParameters){
    vTaskDelay( pdMS_TO_TICKS( 20000 ) );
    while(1){
        aperiodicReq++;
        //printf("%d\n", aperiodicReq);
        int delayMs = 10000 + (rand() % 10001);  // 10,000 to 20,000 ms
        printf("Aperiodic Request\n");
        vTaskDelay(pdMS_TO_TICKS(delayMs));
    }
}