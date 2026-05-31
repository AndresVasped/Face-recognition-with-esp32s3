#include "esp_log.h"
#include "cam_mod.h"
#include "wifi.h"
#include "spiffs_mod.h"
#include "in_situ_mod.h"
#include "freertos/FreeRTOS.h"
#include "face_model.h"

extern volatile bool g_enroll_requested;

void face_recognition_task(void *pvParameters)
{
    for(;;)
    {
       model_processing();
       vTaskDelay(pdMS_TO_TICKS(300));   
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

void button_task(void *pvParameters)
{
    for(;;)
    {
        if(gpio_get_level(BUTTON_ADD_FACE)==0)
        {
            
            ESP_LOGI("BUTTON","YOU PUSH THE BUTTON");
            //put_in_db();
            g_enroll_requested=true;
            while(gpio_get_level(BUTTON_ADD_FACE) == 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void app_main()
{
    
    esp_wifi_set_max_tx_power(40);//limits the wifi range
    wifi_sta_init();//start the wifi
    ESP_LOGI("app", "Esperando IP...");
    while(!have_wifi())
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI("app", "IP obtenida, continuando...");
    spiffs_init();
    camera_init();
    models_init();
    http_init_server();//start the http server
    gpio_configuration();

    //Lanzar la tarea de reconocimiento fijada al núcleo 1
    xTaskCreatePinnedToCore(
        face_recognition_task,   // Función de la tarea
        "face_rec_task",         // Nombre descriptivo
        1024 * 8,                // Tamaño de la pila (Stack) -> ¡Dale al menos 8KB! La IA es pesada
        NULL,                    // Parámetros de la tarea
        5,                       // Prioridad de la tarea (Media/Alta)
        NULL,                    // Descriptor de la tarea
        1                        // <--- CORE 1 (Obligatorio para que no interfiera con el sistema)
    );
    
    xTaskCreate(button_task,"Button_task",2048,NULL,5,NULL);
}