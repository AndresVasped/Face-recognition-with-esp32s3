#include "spiffs_mod.h"
void spiffs_init()
{
    ESP_LOGI("SPIFFS", "Inicializando partición SPIFFS...");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",          // Este es el prefijo de la ruta
        .partition_label = NULL,         // Usa la partición spiffs por defecto de tu tabla
        .max_files = 5,                  // Número máximo de archivos abiertos a la vez
        .format_if_mount_failed = true   // ¡IMPORTANTE! Si está vacío o no tiene formato, lo formatea automáticamente la primera vez
    };
    // Registrar e inicializar el sistema de archivos en la Flash
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SPIFFS", "Error al montar o formatear el sistema de archivos");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("SPIFFS", "No se encontró la partición SPIFFS en la tabla de particiones");
        } else {
            ESP_LOGE("SPIFFS", "Error al inicializar SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI("SPIFFS", "SPIFFS montado exitosamente.");
}