/********************************************************************************************* */
//    Eduboard2 ESP32-S3 Template with BSP
//    Author: Martin Burger
//    Juventus Technikerschule
//    Version: 1.0.0
//    
//    This is the ideal starting point for a new Project. BSP for most of the Eduboard2
//    Hardware is included under components/eduboard2.
//    Hardware support can be activated/deactivated in components/eduboard2/eduboard2_config.h
/********************************************************************************************* */
#include "eduboard2.h"
#include "memon.h"

#include "math.h"

#define TAG "TEMPLATE"

#define UPDATETIME_MS 100

typedef struct {
    double     piValue;
    TickType_t tickCount;
} piResult_t;

EventGroupHandle_t piCalcEventGroup;
QueueHandle_t leibnizQueue;

void controlTask(void* param) {
    piResult_t lebnizResult;
    uint16_t xpos = 20;
	uint16_t ypos = 50;
	char displayPi[24];
    char displayTicks[24];
    char displayTime[24];
	uint16_t color = GREEN;
    for(;;) {
        if(xQueueReceive(leibnizQueue, &lebnizResult, portMAX_DELAY) == pdTRUE) {
            xQueueReset(leibnizQueue);
            sprintf((char*)displayPi, "Pi = %f", lebnizResult.piValue);
            sprintf((char*)displayTicks, "Ticks = %d", (int)lebnizResult.tickCount);
            sprintf((char*)displayTime, "Time = %.3fs", ((float)(lebnizResult.tickCount * portTICK_PERIOD_MS)) / 1000);
            lcdFillScreen(BLACK);
            lcdDrawString(fx32L, xpos, ypos, &displayPi[0], color);
            lcdDrawString(fx32L, xpos, ypos+50, &displayTicks[0], color);
            lcdDrawString(fx32L, xpos, ypos+100, &displayTime[0], color);
            lcdUpdateVScreen();
        }
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void leibnizTask(void* param) {
    piResult_t piResult;
    piResult.piValue = 0;
    vTaskDelay(100);

    for(;;) {
        piResult.tickCount = xTaskGetTickCount();
        piResult.piValue++;
        xQueueSendToFront(leibnizQueue, &piResult, 0);

        taskYIELD();  // Yield to allow IDLE task brief execution
    }
}

void app_main()
{
    //Initialize Eduboard2 BSP
    eduboard2_init();

    piCalcEventGroup = xEventGroupCreate();
    leibnizQueue = xQueueCreate(100, sizeof(piResult_t));
    
    //Create templateTask
    xTaskCreatePinnedToCore(controlTask, "controlTask", 2*2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(leibnizTask, "leibnizTask", 2*2048, NULL, 1, NULL, 1);

    return;
}