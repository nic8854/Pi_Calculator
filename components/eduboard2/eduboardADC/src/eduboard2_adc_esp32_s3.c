#include "../../eduboard2.h"
#include "../eduboard2_adc.h"

#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"

#define TAG "ADC_Driver"

#define _ADC_UNIT_STR(unit)         #unit
#define ADC_UNIT_STR(unit)          _ADC_UNIT_STR(unit)
#define ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define ADC_GET_DATA(p_data)        ((p_data)->type2.data)

#define ADC_ATTENUATION ADC_ATTEN_DB_12
// #if defined(ADC_ATTEN_DB_12)
//     #define ADC_ATTENUATION ADC_ATTEN_DB_12
// #else
//     #define ADC_ATTENUATION ADC_ATTEN_DB_11
// #endif

SemaphoreHandle_t hADCMutex;

adc_cali_handle_t adc1_cali_handle = NULL;
static int* adc_raw;
static int* voltage;
static uint16_t** adcbuffer = NULL;
void (*adcStreamCallbackFunction)() = NULL;

int8_t channelmap_ANX_TO_ADCX[5] = {-1,-1,-1,-1,-1};
int8_t channelmap_ADCX_TO_ANX[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
adc_channel_t* channels;



static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }
    return calibrated;
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle, uint32_t samplerate_us, uint32_t buffersize)
{
    adc_continuous_handle_t handle = NULL;
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = buffersize*2,
        .conv_frame_size = buffersize,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    uint32_t sample_freq = (1000000/samplerate_us)*channel_num;
    ESP_LOGI(TAG, "Set ADC Sampling Frequency to %iHz" ,(int)(sample_freq));
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = sample_freq,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = ADC_ATTENUATION;
        adc_pattern[i].channel = channel[i];
        adc_pattern[i].unit = ADC_UNIT;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));
    *out_handle = handle;
}

uint8_t initADCChannels() {
    uint8_t activeChannels = 0;
    #ifdef CONFIG_ENABLE_AN0
    channelmap_ADCX_TO_ANX[AN0_CHANNEL] = activeChannels;
    activeChannels++;
    channelmap_ANX_TO_ADCX[AN0] = AN0_CHANNEL;
    #endif
    #ifdef CONFIG_ENABLE_AN1
    channelmap_ADCX_TO_ANX[AN1_CHANNEL] = activeChannels;
    activeChannels++;
    channelmap_ANX_TO_ADCX[AN1] = AN1_CHANNEL;
    #endif
    #ifdef CONFIG_ENABLE_AN2
    channelmap_ADCX_TO_ANX[AN2_CHANNEL] = activeChannels;
    activeChannels++;
    channelmap_ANX_TO_ADCX[AN2] = AN2_CHANNEL;
    #endif
    #ifdef CONFIG_ENABLE_AN3
    channelmap_ADCX_TO_ANX[AN3_CHANNEL] = activeChannels;
    activeChannels++;
    channelmap_ANX_TO_ADCX[AN3] = AN3_CHANNEL;
    #endif
    #ifdef CONFIG_ENABLE_AN4
    channelmap_ADCX_TO_ANX[AN4_CHANNEL] = activeChannels;
    activeChannels++;
    channelmap_ANX_TO_ADCX[AN4] = AN4_CHANNEL;
    #endif
    if(activeChannels > 0) {
        channels = malloc(sizeof(adc_channel_t)*activeChannels);
        #ifdef CONFIG_ENABLE_ADC_STREAMING
        adcbuffer = malloc(sizeof(uint16_t**) * activeChannels);
        for(int i = 0; i < activeChannels; i++) {
            adcbuffer[i] = malloc(sizeof(uint16_t)*CONFIG_ADC_STREAMING_BUFFERSIZE);
        }
        #endif
        adc_raw = malloc(activeChannels * sizeof(int));
        voltage = malloc(activeChannels * sizeof(int));
        memset(adc_raw, 0, activeChannels * sizeof(int));
        memset(voltage, 0, activeChannels * sizeof(int));
    } else {
        channels = NULL;
    }
    uint8_t i = 0;
    #ifdef CONFIG_ENABLE_AN0
    channels[i] = AN0_CHANNEL;
    channelmap_ADCX_TO_ANX[AN0_CHANNEL] = i;
    i++;
    #endif
    #ifdef CONFIG_ENABLE_AN1
    channels[i] = AN1_CHANNEL;
    channelmap_ADCX_TO_ANX[AN1_CHANNEL] = i;
    i++;
    #endif
    #ifdef CONFIG_ENABLE_AN2
    channels[i] = AN2_CHANNEL;
    channelmap_ADCX_TO_ANX[AN2_CHANNEL] = i;
    i++;
    #endif
    #ifdef CONFIG_ENABLE_AN3
    channels[i] = AN3_CHANNEL;
    channelmap_ADCX_TO_ANX[AN3_CHANNEL] = i;
    i++;
    #endif
    #ifdef CONFIG_ENABLE_AN4
    channels[i] = AN4_CHANNEL;
    channelmap_ADCX_TO_ANX[AN4_CHANNEL] = i;
    i++;
    #endif
    return activeChannels;
}

void setADCDataValue(uint8_t channel, int value) {
    if(channelmap_ADCX_TO_ANX[channel] == -1) return;
    xSemaphoreTake(hADCMutex, portMAX_DELAY);
    adc_raw[channelmap_ADCX_TO_ANX[channel]] = value;
    adc_cali_raw_to_voltage(adc1_cali_handle, value, &voltage[channelmap_ADCX_TO_ANX[channel]]);
    xSemaphoreGive(hADCMutex);
}
int getADCRawDataValue(uint8_t channel) {
    if(channelmap_ANX_TO_ADCX[channel] != -1) {
        return adc_raw[channelmap_ADCX_TO_ANX[channelmap_ANX_TO_ADCX[channel]]];
    }
    return 0;
}
int getADCVoltageValue(uint8_t channel) {
    if(channelmap_ANX_TO_ADCX[channel] != -1) {
        return voltage[channelmap_ADCX_TO_ANX[channelmap_ANX_TO_ADCX[channel]]];
    }
    return 0;
}

void adcTask(void * parameter) {
    esp_err_t ret;
    ESP_LOGI(TAG, "init ADC...");
    hADCMutex = xSemaphoreCreateMutex();
    uint32_t ret_num = 0;
    adcbuffer = NULL;
    uint8_t numberofchannels = initADCChannels();
    adc_calibration_init(ADC_UNIT, ADC_ATTENUATION, &adc1_cali_handle);
    uint8_t* result;
    uint32_t buffersize = 0;
    uint32_t n[5] = {0,0,0,0,0};
#ifdef CONFIG_ENABLE_ADC_STREAMING    
    buffersize = CONFIG_ADC_STREAMING_BUFFERSIZE*SOC_ADC_DIGI_RESULT_BYTES*numberofchannels;
    result = malloc(buffersize);
    memset(result, 0xcc, buffersize);
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channels, numberofchannels, &handle, ADC_STREAM_SAMPLERATE_US, buffersize);
    adc_continuous_start(handle);    
    for(;;) {
        for(int i = 0; i < 5; i++) {
            n[i] = 0;
        }
        ret = adc_continuous_read(handle, result, buffersize, &ret_num, portMAX_DELAY);
        if (ret == ESP_OK) {
            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                uint32_t chan_num = ADC_GET_CHANNEL(p);
                uint32_t data = ADC_GET_DATA(p);
                if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT)) {
                    adcbuffer[channelmap_ADCX_TO_ANX[chan_num]][n[channelmap_ADCX_TO_ANX[chan_num]]++] = data;
                } else {
                    ESP_LOGE(TAG, "Invalid ADC Data");
                }
            }
            if(adcStreamCallbackFunction != NULL) {
                (*adcStreamCallbackFunction)();
            }
            if(n[0] < CONFIG_ADC_STREAMING_BUFFERSIZE) {
                ESP_LOGE(TAG, "Not enough Data received!");
            }
        }
    }
#else
    buffersize = 1*SOC_ADC_DIGI_RESULT_BYTES*numberofchannels;
    result = malloc(buffersize);
    memset(result, 0xcc, buffersize);
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channels, numberofchannels, &handle, 1000, buffersize);
    adc_continuous_start(handle);
    for(;;) {
        for(int i = 0; i < 5; i++) {
            n[i] = 0;
        }
        ret = adc_continuous_read(handle, result, buffersize, &ret_num, portMAX_DELAY);
        if (ret == ESP_OK) {
            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                uint32_t chan_num = ADC_GET_CHANNEL(p);
                uint32_t data = ADC_GET_DATA(p);
                if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT)) {
                    setADCDataValue(chan_num, data);
                    n[channelmap_ADCX_TO_ANX[chan_num]]++;
                } else {
                    ESP_LOGE(TAG, "Invalid ADC Data");
                }
            }
        }
    }
#endif
}

uint32_t adc_get_raw(uint8_t adc_channel) {
    uint32_t returnValue = 0;
    xSemaphoreTake(hADCMutex, portMAX_DELAY);
    returnValue = getADCRawDataValue(adc_channel);
    xSemaphoreGive(hADCMutex);
    return returnValue;
}
uint32_t adc_get_voltage_mv(uint8_t adc_channel) {
    uint32_t returnValue = 0;
    xSemaphoreTake(hADCMutex, portMAX_DELAY);
    returnValue = getADCVoltageValue(adc_channel);
    xSemaphoreGive(hADCMutex);
    return returnValue;
}
uint32_t adc_get_buffer(uint8_t adc_channel, uint16_t* buffer) {
#ifdef CONFIG_ADC_STREAMING_BUFFERSIZE
    if(channelmap_ANX_TO_ADCX[adc_channel] == -1) return 0;
    for(uint32_t i = 0; i < CONFIG_ADC_STREAMING_BUFFERSIZE; i++) {
        buffer[i] = adcbuffer[channelmap_ADCX_TO_ANX[channelmap_ANX_TO_ADCX[adc_channel]]][i];
    }
    return CONFIG_ADC_STREAMING_BUFFERSIZE;
#else
    return 0;
#endif
}

void adc_set_stream_callback(void* stream_callback_function) {
#ifdef CONFIG_ENABLE_ADC_STREAMING
    adcStreamCallbackFunction = stream_callback_function;
#endif
}

void eduboard_init_adc() {
    xTaskCreate(adcTask, "adcTask", 4 * 2048, NULL, 5, NULL);
}