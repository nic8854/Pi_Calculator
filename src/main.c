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
    uint32_t   iterations;
} piResult_t;

const double piReference = 3.141592653589793238;

EventGroupHandle_t piCalcEventGroup;
QueueHandle_t leibnizQueue;

int checkPiDigits(double calculatedPi, double referencePi) {
    char calcStr[32];
    char refStr[32];
    
    sprintf(calcStr, "%.15f", calculatedPi);
    sprintf(refStr, "%.15f", referencePi);
    
    int matchingDigits = 0;
    bool afterDecimal = false;
    
    for(int i = 0; calcStr[i] != '\0' && refStr[i] != '\0'; i++) {
        if(calcStr[i] == '.') {
            afterDecimal = true;
            continue;
        }
        
        if(afterDecimal) {
            if(calcStr[i] == refStr[i]) {
                matchingDigits++;
            } else {
                break;
            }
        }
    }
    
    return matchingDigits;
}

void controlTask(void* param) {
    piResult_t lebnizResult;
    uint16_t xpos = 20;
	uint16_t ypos = 50;
	char displayPi[24];
    char displayTicks[24];
    char displayIterations[24];
    char displayTime[24];
    char displayMathingDigits[24];
	uint16_t color = WHITE;
    for(;;) {
        if(xQueueReceive(leibnizQueue, &lebnizResult, portMAX_DELAY) == pdTRUE) {
            sprintf((char*)displayPi, "Pi = %f", lebnizResult.piValue);
            sprintf((char*)displayTicks, "Ticks = %d", (int)lebnizResult.tickCount);
            sprintf((char*)displayIterations, "Iterations = %d", (int)lebnizResult.iterations);
            sprintf((char*)displayTime, "Time = %.3fs", ((float)(lebnizResult.tickCount * portTICK_PERIOD_MS)) / 1000);
            sprintf((char*)displayTime, "matchingDigits = %d", checkPiDigits(lebnizResult.piValue, piReference));
            lcdFillScreen(BLACK);
            lcdDrawString(fx32L, xpos, ypos, &displayPi[0], color);
            lcdDrawString(fx32L, xpos, ypos+50, &displayTicks[0], color);
            lcdDrawString(fx32L, xpos, ypos+100, &displayIterations[0], color);
            lcdDrawString(fx32L, xpos, ypos+150, &displayTime[0], color);
            lcdUpdateVScreen();
            xQueueReset(leibnizQueue);
        }
        vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

void leibnizTask(void* param) {
    piResult_t piResult;
    
    uint32_t iterator = 0;
    double sum = 0;
    double divisionValue = 0;
    vTaskDelay(100);

    for(;;) {
        divisionValue = 1.0 / (((double)iterator * 2.0) + 1.0);
        if(iterator % 2) {
            sum -= divisionValue;
            printf("pi = pi - %f\n", divisionValue);
        } else {
            sum += divisionValue;
            printf("pi = pi + %f\n", divisionValue);
        }
        printf("iterator = %ld\n", iterator);
        iterator++;
        piResult.tickCount = xTaskGetTickCount();
        piResult.piValue = sum * 4.0;
        piResult.iterations = iterator;
        xQueueSendToFront(leibnizQueue, &piResult, 0);
        if(iterator % 1000 == 0) {
            vTaskDelay(1);
        } else {
            taskYIELD();
        }
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