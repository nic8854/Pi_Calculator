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
QueueHandle_t chudnovskyQueue;

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

void controlTask(void* param) {
    piResult_t leibnizResult;
    piResult_t chudnovskyResult;
    uint16_t xpos = 20;
	uint16_t ypos = 50;
    char displayTicks[24];
    char displayIterations[24];
    char displayTime[24];
    char displayMathingDigits[24];
	uint16_t color = WHITE;
    for(;;) {
        lcdFillScreen(BLACK);
        if(xQueueReceive(leibnizQueue, &leibnizResult, portMAX_DELAY) == pdTRUE) {
            sprintf((char*)displayTicks, "Ticks = %d", (int)leibnizResult.tickCount);
            sprintf((char*)displayIterations, "Passes = %d", (int)leibnizResult.iterations);
            sprintf((char*)displayTime, "Time = %.3fs", ((float)(leibnizResult.tickCount * portTICK_PERIOD_MS)) / 1000);
            sprintf((char*)displayMathingDigits, "Digits = %d", checkPiDigits__(leibnizResult.piValue, piReference));

            lcdDrawString(fx32G, xpos, ypos, "Leibniz", color);
            drawColoredPi__(fx24G, xpos, ypos+50, leibnizResult.piValue, piReference);
            lcdDrawString(fx24G, xpos, ypos+100, &displayTicks[0], color);
            lcdDrawString(fx24G, xpos, ypos+150, &displayIterations[0], color);
            lcdDrawString(fx24G, xpos, ypos+200, &displayTime[0], color);
            lcdDrawString(fx24G, xpos, ypos+250, &displayMathingDigits[0], color);
            lcdDrawRect(xpos-10, ypos+10, ypos+180, ypos+260, BLUE);

            xQueueReset(leibnizQueue);
        }
        if(xQueueReceive(chudnovskyQueue, &chudnovskyResult, portMAX_DELAY) == pdTRUE) {
            sprintf((char*)displayTicks, "Ticks = %d", (int)chudnovskyResult.tickCount);
            sprintf((char*)displayIterations, "Passes = %d", (int)chudnovskyResult.iterations);
            sprintf((char*)displayTime, "Time = %.3fs", ((float)(chudnovskyResult.tickCount * portTICK_PERIOD_MS)) / 1000);
            sprintf((char*)displayMathingDigits, "Digits = %d", checkPiDigits__(chudnovskyResult.piValue, piReference));

            lcdDrawString(fx32G, xpos+230, ypos, "Chudnovsky", color);
            drawColoredPi__(fx24G, xpos+230, ypos+50, chudnovskyResult.piValue, piReference);
            lcdDrawString(fx24G, xpos+230, ypos+100, &displayTicks[0], color);
            lcdDrawString(fx24G, xpos+230, ypos+150, &displayIterations[0], color);
            lcdDrawString(fx24G, xpos+230, ypos+200, &displayTime[0], color);
            lcdDrawString(fx24G, xpos+230, ypos+250, &displayMathingDigits[0], color);
            lcdDrawRect(xpos+220, ypos+10, ypos+410, ypos+260, BLUE);

            xQueueReset(chudnovskyQueue);
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

    for(;;) {
        divisionValue = 1.0 / (((double)iterator * 2.0) + 1.0);
        if(iterator % 2) {
            sum -= divisionValue;
        } else {
            sum += divisionValue;
        }
        iterator++;
        piResult.tickCount = xTaskGetTickCount();
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

void chudnovskyTask(void* param) {
    piResult_t piResult;
    
    uint32_t k = 0;
    long double sum = 0;
    long double K = 6.0L;
    long double M = 1.0L;
    long double X = 1.0L;
    long double L = 13591409.0L;
    long double S = 13591409.0L;
    
    vTaskDelay(100);
    
    const long double C = 426880.0L * sqrtl(10005.0L);
    const uint32_t MAX_ITERATIONS = 2;  //hard fix to make double not overflow
    
    for(;;) {
        if(k > 0 && k <= MAX_ITERATIONS) {
            K += 12.0L;
            M = M * (K*K*K - 16.0L*K) / ((k+1)*(k+1)*(k+1));
            L += 545140134.0L;
            X *= -262537412640768000.0L;
            S += M * L / X;
        }
        
        piResult.tickCount = xTaskGetTickCount();
        piResult.piValue = (double)(C / S);
        piResult.iterations = k + 1;
        printf("pi = %0.10f\n", piResult.piValue);
        xQueueSendToFront(chudnovskyQueue, &piResult, 0);
        
        k++;
        
        // Stop after reaching double precision limit
        if(k > MAX_ITERATIONS) {
            vTaskDelay(100);  // Just update display periodically
        } else if(k % 10 == 0) {
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
    chudnovskyQueue = xQueueCreate(100, sizeof(piResult_t));
    
    //Create templateTask
    xTaskCreatePinnedToCore(controlTask, "controlTask", 2*2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(leibnizTask, "leibnizTask", 2*2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(chudnovskyTask, "chudnovskyTask", 2*2048, NULL, 1, NULL, 1);

    return;
}