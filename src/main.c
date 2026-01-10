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

uint8_t digitTarget = 6;

QueueHandle_t leibnizQueue;
QueueHandle_t eulerQueue;

TaskHandle_t leibnizTaskHandle = NULL;
TaskHandle_t eulerTaskHandle = NULL;

EventGroupHandle_t piCalcEventGroup;
#define LEIBNIZ_START      (1 << 0)  // bit 0
#define EULER_START         (1 << 1)  // bit 1
#define RACE_START          (1 << 2)  // bit 2
#define RESET               (1 << 5)  // bit 5

// Forward declarations
void leibnizTask(void* param);
void eulerTask(void* param);

int checkPiDigits__(double calculatedPi, double referencePi) {
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

void drawColoredPi__(const FontxFile *font, uint16_t x, uint16_t y, double calculatedPi, double referencePi) {
    char calcStr[32];
    char refStr[32];
    char singleChar[2] = {0, 0};
    
    sprintf(calcStr, "%.10f", calculatedPi);
    sprintf(refStr, "%.10f", referencePi);
    
    uint16_t xOffset = x;
    bool afterDecimal = false;
    
    // Draw "Pi = " label in white
    lcdDrawString(font, xOffset, y, "Pi = ", WHITE);
    xOffset += 5*12;
    
    for(int i = 0; calcStr[i] != '\0'; i++) {
        singleChar[0] = calcStr[i];
        uint16_t color = WHITE;
        
        if(calcStr[i] == '.') {
            afterDecimal = true;
            color = WHITE;
        } else if(afterDecimal && calcStr[i] == refStr[i]) {
            color = GREEN;
        }
        
        lcdDrawString(font, xOffset, y, singleChar, color);
        xOffset += 12;
    }
}

void inputTask(void* param) {
    int32_t rotationChange = 0;
    uint32_t eventBits;
    for(;;) {
        eventBits = xEventGroupGetBits(piCalcEventGroup);
        if(button_get_state(SW0, true) == SHORT_PRESSED) {
            if(!(eventBits & EULER_START || eventBits & RACE_START)) {
                xEventGroupSetBits(piCalcEventGroup, LEIBNIZ_START);
            }
        }
        if(button_get_state(SW1, true) == SHORT_PRESSED) {
            if(!(eventBits & LEIBNIZ_START || eventBits & RACE_START)) {
                xEventGroupSetBits(piCalcEventGroup, EULER_START);
            }
        }
        if(button_get_state(SW2, true) == SHORT_PRESSED) {
            if(!(eventBits & LEIBNIZ_START || eventBits & EULER_START)) {
                xEventGroupSetBits(piCalcEventGroup, RACE_START);
            }
        }
        if(button_get_state(SW3, true) == SHORT_PRESSED) {
            xEventGroupSetBits(piCalcEventGroup, RESET);
        }
        rotationChange = rotary_encoder_get_rotation(true);
        if(!(eventBits & LEIBNIZ_START || eventBits & EULER_START || eventBits & RACE_START)) {
            if(rotationChange != 0) {
                if(rotationChange > 0) {
                    digitTarget ++;
                } else {
                    digitTarget --;
                }
                if(digitTarget < 1) {
                    digitTarget = 1;
                }
                if(digitTarget > 10) {
                    digitTarget = 10;
                }
            }
        }

        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

void controlTask(void* param) {
    piResult_t leibnizResult;
    uint8_t leibnizDigits = 0;
    leibnizResult.iterations = 0;
    leibnizResult.piValue = 0.0;
    leibnizResult.tickCount = 0;
    piResult_t eulerResult;
    uint8_t eulerDigits = 0;
    eulerResult.iterations = 0;
    eulerResult.piValue = 0.0;
    eulerResult.tickCount = 0;
    EventBits_t eventBits;
    EventBits_t eventBitsLast = 0;
    uint16_t xpos = 20;
	uint16_t ypos = 50;
    char displayTicksLeibniz[24];
    char displayIterationsLeibniz[24];
    char displayTimeLeibniz[24];
    char displayMatchingDigitsLeibniz[24];
    char displayTicksEuler[24];
    char displayIterationsEuler[24];
    char displayTimeEuler[24];
    char displayMatchingDigitsEuler[24];
	uint16_t color = WHITE;

    for(;;) {
        eventBits = xEventGroupGetBits(piCalcEventGroup);
        if(eventBits & LEIBNIZ_START && !(eventBitsLast & LEIBNIZ_START)) {
            led_set(LED0, 1);
            if(leibnizTaskHandle != NULL && eTaskGetState(leibnizTaskHandle) == eSuspended) {
                vTaskResume(leibnizTaskHandle);
            }
        }
        if(eventBits & EULER_START && !(eventBitsLast & EULER_START)) {
            led_set(LED1, 1);
            if(eulerTaskHandle != NULL && eTaskGetState(eulerTaskHandle) == eSuspended) {
                vTaskResume(eulerTaskHandle);
            }
        }
        if(eventBits & RACE_START && !(eventBitsLast & RACE_START)) {
            led_set(LED0, 1);
            led_set(LED1, 1);
            if(leibnizTaskHandle != NULL && eTaskGetState(leibnizTaskHandle) == eSuspended) {
                vTaskResume(leibnizTaskHandle);
            }
            if(eulerTaskHandle != NULL && eTaskGetState(eulerTaskHandle) == eSuspended) {
                vTaskResume(eulerTaskHandle);
            }
        }
        if(eventBits & RESET && !(eventBitsLast & RESET)) {
            xEventGroupClearBits(piCalcEventGroup, LEIBNIZ_START | EULER_START  | RACE_START | RESET);
            led_set(LED0, 0);
            led_set(LED1, 0);
            
            // Delete and recreate tasks to fully reset their internal state
            if(leibnizTaskHandle != NULL) {
                vTaskDelete(leibnizTaskHandle);
                leibnizTaskHandle = NULL;
            }
            if(eulerTaskHandle != NULL) {
                vTaskDelete(eulerTaskHandle);
                eulerTaskHandle = NULL;
            }
            
            // Clear queues
            xQueueReset(leibnizQueue);
            xQueueReset(eulerQueue);
            
            // Recreate tasks in suspended state
            xTaskCreatePinnedToCore(leibnizTask, "leibnizTask", 2*2048, NULL, 1, &leibnizTaskHandle, 1);
            xTaskCreatePinnedToCore(eulerTask, "eulerTask", 2*2048, NULL, 1, &eulerTaskHandle, 1);
            vTaskSuspend(leibnizTaskHandle);
            vTaskSuspend(eulerTaskHandle);
            
            leibnizResult.iterations = 0;
            leibnizResult.piValue = 0.0;
            leibnizResult.tickCount = 0;
            leibnizDigits = 0;
            eulerResult.iterations = 0;
            eulerResult.piValue = 0.0;
            eulerResult.tickCount = 0;
            eulerDigits = 0;
            
            eventBits = xEventGroupGetBits(piCalcEventGroup);
        }

        eventBitsLast = eventBits;

        if(leibnizDigits < digitTarget) {
            if(xQueueReceive(leibnizQueue, &leibnizResult, 0) == pdTRUE) {
                xQueueReset(leibnizQueue);
                leibnizDigits = checkPiDigits__(leibnizResult.piValue, piReference);
            }
        } else {
            led_set(LED0, 0);
            if(leibnizTaskHandle != NULL && eTaskGetState(leibnizTaskHandle) != eSuspended) {
                vTaskSuspend(leibnizTaskHandle);
            }
        }
        
        if(eulerDigits < digitTarget) {
            if(xQueueReceive(eulerQueue, &eulerResult, 0) == pdTRUE) {
                xQueueReset(eulerQueue);
                eulerDigits = checkPiDigits__(eulerResult.piValue, piReference);
            }
        } else {
            led_set(LED1, 0);
            if(eulerTaskHandle != NULL && eTaskGetState(eulerTaskHandle) != eSuspended) {
                vTaskSuspend(eulerTaskHandle);
            }
        }

        lcdFillScreen(BLACK);

        sprintf((char*)displayTicksLeibniz, "Ticks = %d", (int)leibnizResult.tickCount);
        sprintf((char*)displayIterationsLeibniz, "Passes = %d", (int)leibnizResult.iterations);
        sprintf((char*)displayTimeLeibniz, "Time = %.3fs", ((float)(leibnizResult.tickCount * portTICK_PERIOD_MS)) / 1000);
        sprintf((char*)displayMatchingDigitsLeibniz, "Digits = %d / %d", leibnizDigits, digitTarget);

        lcdDrawString(fx32G, xpos, ypos, "Leibniz", color);
        drawColoredPi__(fx24G, xpos, ypos+50, leibnizResult.piValue, piReference);
        lcdDrawString(fx24G, xpos, ypos+100, &displayTicksLeibniz[0], color);
        lcdDrawString(fx24G, xpos, ypos+150, &displayIterationsLeibniz[0], color);
        lcdDrawString(fx24G, xpos, ypos+200, &displayTimeLeibniz[0], color);
        lcdDrawString(fx24G, xpos, ypos+250, &displayMatchingDigitsLeibniz[0], color);
        if(leibnizDigits >= digitTarget) {
            lcdDrawRect(xpos-10, ypos+10, ypos+180, ypos+260, GREEN);
        } else if (leibnizDigits == 0) {
            lcdDrawRect(xpos-10, ypos+10, ypos+180, ypos+260, BLUE);
        } else {
            lcdDrawRect(xpos-10, ypos+10, ypos+180, ypos+260, RED);
        }

        sprintf((char*)displayTicksEuler, "Ticks = %d", (int)eulerResult.tickCount);
        sprintf((char*)displayIterationsEuler, "Passes = %d", (int)eulerResult.iterations);
        sprintf((char*)displayTimeEuler, "Time = %.3fs", ((float)(eulerResult.tickCount * portTICK_PERIOD_MS)) / 1000);
        sprintf((char*)displayMatchingDigitsEuler, "Digits = %d / %d", eulerDigits, digitTarget);

        lcdDrawString(fx32G, xpos+230, ypos, "Euler", color);
        drawColoredPi__(fx24G, xpos+230, ypos+50, eulerResult.piValue, piReference);
        lcdDrawString(fx24G, xpos+230, ypos+100, &displayTicksEuler[0], color);
        lcdDrawString(fx24G, xpos+230, ypos+150, &displayIterationsEuler[0], color);
        lcdDrawString(fx24G, xpos+230, ypos+200, &displayTimeEuler[0], color);
        lcdDrawString(fx24G, xpos+230, ypos+250, &displayMatchingDigitsEuler[0], color);
        if(eulerDigits >= digitTarget) {
            lcdDrawRect(xpos+220, ypos+10, ypos+410, ypos+260, GREEN);
        } else if (eulerDigits == 0) {
            lcdDrawRect(xpos+220, ypos+10, ypos+410, ypos+260, BLUE);
        } else {
            lcdDrawRect(xpos+220, ypos+10, ypos+410, ypos+260, RED);
        }

        lcdUpdateVScreen();
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

void leibnizTask(void* param) {
    piResult_t piResult;
    
    uint32_t iterator = 0;
    double sum = 0;
    double divisionValue = 0;
    vTaskDelay(100);
    TickType_t startTick = xTaskGetTickCount();
    for(;;) {
        divisionValue = 1.0 / (((double)iterator * 2.0) + 1.0);
        if(iterator % 2) {
            sum -= divisionValue;
        } else {
            sum += divisionValue;
        }
        iterator++;
        piResult.tickCount = xTaskGetTickCount() - startTick;
        piResult.piValue = sum * 4.0;
        piResult.iterations = iterator;
        xQueueSendToFront(leibnizQueue, &piResult, 0);
        if(iterator % 500 == 0) {
            vTaskDelay(1);
        } else {
            taskYIELD();
        }
    }
}

void eulerTask(void* param) {
    piResult_t piResult;
    
    uint32_t n = 1;
    double sum = 0;
    
    vTaskDelay(100);
    TickType_t startTick = xTaskGetTickCount();
    
    for(;;) {
        sum += 1.0 / ((double)n * (double)n);
        
        piResult.tickCount = xTaskGetTickCount() - startTick;
        piResult.piValue = sqrt(6.0 * sum);
        piResult.iterations = n;
        xQueueSendToFront(eulerQueue, &piResult, 0);
        
        n++;
        
        if(n % 500 == 0) {
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
    eulerQueue = xQueueCreate(100, sizeof(piResult_t));
    
    //Create templateTask
    xTaskCreatePinnedToCore(inputTask, "inputTask", 2*2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(controlTask, "controlTask", 2*2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(leibnizTask, "leibnizTask", 2*2048, NULL, 1, &leibnizTaskHandle, 1);
    xTaskCreatePinnedToCore(eulerTask, "eulerTask", 2*2048, NULL, 1, &eulerTaskHandle, 1);
    
    // Initially suspend both calculation tasks
    vTaskSuspend(leibnizTaskHandle);
    vTaskSuspend(eulerTaskHandle);

    return;
}