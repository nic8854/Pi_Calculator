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

EventGroupHandle_t piCalcEventGroup;
#define LEIBNIZ_START      (1 << 0)  // bit 0
#define EULER_START         (1 << 1)  // bit 1
#define RACE_START          (1 << 2)  // bit 2
#define RESET               (1 << 5)  // bit 5

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

        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

void controlTask(void* param) {
    piResult_t leibnizResult;
    piResult_t eulerResult;
    EventBits_t eventBits;
    uint16_t xpos = 20;
	uint16_t ypos = 50;
    char displayTicks[24];
    char displayIterations[24];
    char displayTime[24];
    char displayMathingDigits[24];
	uint16_t color = WHITE;
    for(;;) {
        eventBits = xEventGroupGetBits(piCalcEventGroup);
        if(eventBits & LEIBNIZ_START) {
            led_set(LED0, 1);
        }
        if(eventBits & EULER_START) {
            led_set(LED1, 1);
        }
        if(eventBits & RACE_START) {
            led_set(LED2, 1);
        }
        if(eventBits & RESET) {
            xEventGroupClearBits(piCalcEventGroup, LEIBNIZ_START | EULER_START  | RACE_START | RESET);
            led_set(LED0, 0);
            led_set(LED1, 0);
            led_set(LED2, 0);
        }

        lcdFillScreen(BLACK);
        if(xQueueReceive(leibnizQueue, &leibnizResult, portMAX_DELAY) == pdTRUE) {
            sprintf((char*)displayTicks, "Ticks = %d", (int)leibnizResult.tickCount);
            sprintf((char*)displayIterations, "Passes = %d", (int)leibnizResult.iterations);
            sprintf((char*)displayTime, "Time = %.3fs", ((float)(leibnizResult.tickCount * portTICK_PERIOD_MS)) / 1000);
            sprintf((char*)displayMathingDigits, "Digits = %d / %d", checkPiDigits__(leibnizResult.piValue, piReference), digitTarget);

            lcdDrawString(fx32G, xpos, ypos, "Leibniz", color);
            drawColoredPi__(fx24G, xpos, ypos+50, leibnizResult.piValue, piReference);
            lcdDrawString(fx24G, xpos, ypos+100, &displayTicks[0], color);
            lcdDrawString(fx24G, xpos, ypos+150, &displayIterations[0], color);
            lcdDrawString(fx24G, xpos, ypos+200, &displayTime[0], color);
            lcdDrawString(fx24G, xpos, ypos+250, &displayMathingDigits[0], color);
            lcdDrawRect(xpos-10, ypos+10, ypos+180, ypos+260, BLUE);

            xQueueReset(leibnizQueue);
        }
        if(xQueueReceive(eulerQueue, &eulerResult, portMAX_DELAY) == pdTRUE) {
            sprintf((char*)displayTicks, "Ticks = %d", (int)eulerResult.tickCount);
            sprintf((char*)displayIterations, "Passes = %d", (int)eulerResult.iterations);
            sprintf((char*)displayTime, "Time = %.3fs", ((float)(eulerResult.tickCount * portTICK_PERIOD_MS)) / 1000);
            sprintf((char*)displayMathingDigits, "Digits = %d / %d", checkPiDigits__(eulerResult.piValue, piReference), digitTarget);

            lcdDrawString(fx32G, xpos+230, ypos, "Euler", color);
            drawColoredPi__(fx24G, xpos+230, ypos+50, eulerResult.piValue, piReference);
            lcdDrawString(fx24G, xpos+230, ypos+100, &displayTicks[0], color);
            lcdDrawString(fx24G, xpos+230, ypos+150, &displayIterations[0], color);
            lcdDrawString(fx24G, xpos+230, ypos+200, &displayTime[0], color);
            lcdDrawString(fx24G, xpos+230, ypos+250, &displayMathingDigits[0], color);
            lcdDrawRect(xpos+220, ypos+10, ypos+410, ypos+260, BLUE);

            xQueueReset(eulerQueue);
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
    xTaskCreatePinnedToCore(leibnizTask, "leibnizTask", 2*2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(eulerTask, "eulerTask", 2*2048, NULL, 1, NULL, 1);

    return;
}