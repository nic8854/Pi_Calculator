#pragma once

#ifdef CONFIG_ENABLE_AN0
    #define AN0    0
#endif
#ifdef CONFIG_ENABLE_AN1
    #define AN1    1
#endif
#ifdef CONFIG_ENABLE_AN2
    #define AN2    2
#endif
#ifdef CONFIG_ENABLE_AN3
    #define AN3    3
#endif
#ifdef CONFIG_ENABLE_AN4
    #define AN4    4
#endif

uint32_t adc_get_raw(uint8_t adc_channel);
uint32_t adc_get_voltage_mv(uint8_t adc_channel);
uint32_t adc_get_buffer(uint8_t adc_channel, uint16_t* buffer);
void adc_set_stream_callback(void* stream_callback_function);
void eduboard_init_adc();