#include "in_situ_mod.h"
#include "esp_log.h"

void gpio_configuration()
{
    gpio_config_t io_config={
        .pin_bit_mask = (1ULL << BUTTON_ADD_FACE), 
        .mode = GPIO_MODE_INPUT,            // input mode
        .pull_up_en = GPIO_PULLUP_ENABLE,  
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE      
    };
    gpio_config(&io_config);
}

void led_gpio_configuration()
{
    gpio_config_t io_config={
        .pin_bit_mask = (1ULL << LED_PIN_GREEN), 
        .mode = GPIO_MODE_OUTPUT,            // input mode
        .pull_up_en = GPIO_PULLUP_DISABLE,  
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE      
    };
    gpio_config(&io_config);

}