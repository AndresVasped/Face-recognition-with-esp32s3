#include "face_model.h"
#include "esp_log.h"
#include "esp_https_server.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_system.h"

HumanFaceDetect *face_detector = nullptr;
HumanFaceRecognizer *face_recognizer = nullptr;


void telegram_send_message(const char *message)
{
    
    //send the telegram message parameteres
    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s", TELEGRAM_TOKEN,TELEGRAM_CHAT_ID, message);
    esp_http_client_config_t config={};
    config.url=url;
    config.method = HTTP_METHOD_GET;
    config.skip_cert_common_name_check = true;
    config.use_global_ca_store = false;
    config.crt_bundle_attach = NULL;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI("HTTP", "Mensaje enviado exitosamente");
    } else {
        ESP_LOGE("HTTP", "Error enviando mensaje: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
 
}
void telegram_send_image(const char *caption)
{
    camera_fb_t * fb = esp_camera_fb_get(); //fb is frame buzzer and it have a lot of parameters
    if (!fb) {
        ESP_LOGI("ESP-CAM", "Camera Capture Failed");
        return;
    }
    // Variables para el nuevo buffer JPEG
    uint8_t *jpeg_buf = NULL;
    size_t jpeg_len = 0;

    bool converted=frame2jpg(fb,80,&jpeg_buf,&jpeg_len);
    esp_camera_fb_return(fb);

    if (!converted || jpeg_len == 0) {
        ESP_LOGE("HTTP", "Fallo la compresión de RGB565 a JPEG");
        if (jpeg_buf) free(jpeg_buf);
        return;
    }
    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendPhoto", TELEGRAM_TOKEN); 

    esp_http_client_config_t config = {};
    config.url=url;
    config.method = HTTP_METHOD_POST;
    config.skip_cert_common_name_check = true;
    config.use_global_ca_store = false;
    config.crt_bundle_attach = NULL;
    config.timeout_ms=30000;

    esp_http_client_handle_t client = esp_http_client_init(&config);

    
    const char *boundary_val = "ESP32S3CAMBoundary";
    char content_type_header[128];
    snprintf(content_type_header, sizeof(content_type_header), "multipart/form-data; boundary=%s", boundary_val);
    esp_http_client_set_header(client, "Content-Type", content_type_header);

    char head[512];
    int head_len=snprintf(head, sizeof(head), 
             "--%s\r\n"
             "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n"
             "%s\r\n"
             "--%s\r\n"
             "Content-Disposition: form-data; name=\"caption\"\r\n\r\n"
             "%s\r\n"
             "--%s\r\n"
             "Content-Disposition: form-data; name=\"photo\"; filename=\"imagen.jpg\"\r\n"
             "Content-Type: image/jpeg\r\n\r\n",
             boundary_val, TELEGRAM_CHAT_ID, boundary_val, caption, boundary_val);

    char tail[64];
    int tail_len = snprintf(tail, sizeof(tail), "\r\n--%s--\r\n", boundary_val);

    int total_data_len = head_len + jpeg_len + tail_len;

    // Abrimos la conexión e informamos la longitud total de los datos
    esp_err_t err = esp_http_client_open(client, total_data_len);
    
    if (err == ESP_OK) {
        // Escribimos secuencialmente los bloques en el canal abierto
        esp_http_client_write(client, head, head_len);
        esp_http_client_write(client, (const char *)jpeg_buf, jpeg_len);
        esp_http_client_write(client, tail, tail_len);

        // Solicitamos recibir los headers de respuesta del servidor de Telegram
        int response_len = esp_http_client_fetch_headers(client);
        
        if (response_len >= 0) {
            int status_code = esp_http_client_get_status_code(client);
            if (status_code == 200) {
                ESP_LOGI("HTTP", "¡Imagen enviada exitosamente! Código: %d", status_code);
            } else {
                ESP_LOGE("HTTP", "Telegram rechazó la imagen. Código de estado HTTP: %d", status_code);
            }
        } else {
            ESP_LOGE("HTTP", "Error al leer las cabeceras de respuesta del servidor");
        }
    } else {
        ESP_LOGE("HTTP", "Error abriendo la conexión de datos: %s", esp_err_to_name(err));
    }
    free(jpeg_buf);
    esp_http_client_cleanup(client);
    
}

extern "C"{
    volatile bool g_enroll_requested = false;
}
extern "C" void models_init()//inicializar el modelo 
{
    ESP_LOGI("IA_INIT", "Fase 1: Instanciando Detector...");
    face_detector = new HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, false);
    ESP_LOGI("IA_INIT", "Detector instanciado con éxito.");

    ESP_LOGI("IA_INIT", "Fase 2: Instanciando Extractor...");
    face_recognizer = new HumanFaceRecognizer(
        "/spiffs/face.db",
        HumanFaceFeat::MFN_S8_V1,
        false
    );
    ESP_LOGI("IA_INIT", "Extractor instanciado con éxito.");

}


extern "C" void put_in_db()//vamos a poner coordenadas de la cara en la base de datos
{
    if (face_detector == nullptr) 
    {
        ESP_LOGW("IA_BRIDGE", "Modelos no listos todavía. Ignorando frame.");
        return;
    }

    camera_fb_t *fb=esp_camera_fb_get(); 
    if(!fb)
    {
        ESP_LOGW("IA", "Frame de cámara vacío en C++");
        return;
    }

    uint8_t *rgb_buffer = (uint8_t *)heap_caps_malloc(fb->width * fb->height * 3, MALLOC_CAP_SPIRAM);
    if (!rgb_buffer)  
    {
        ESP_LOGE("IA", "Sin memoria para rgb_buffer");
        esp_camera_fb_return(fb);
        return;
    }
    fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buffer);
    //liberamos la camara
    esp_camera_fb_return(fb);
    dl::image::img_t live_frame(rgb_buffer,fb->width,fb->height
    ,dl::image::DL_IMAGE_PIX_TYPE_RGB888);//parametros de la construccion del modelo en un frame en vivo

    //  Correr la detección y el reconocimiento local
    auto detect_results = face_detector->run(live_frame);

    if(detect_results.empty())//hay una cara?
    {
        ESP_LOGI("IA_DB","NO SE DETECTO CARA INTENTALO DE NUEVO");
    }
    else
    {
        esp_err_t result= face_recognizer->enroll(live_frame,detect_results);
        if(result== ESP_OK)
        {
            ESP_LOGI("IA_DB", "FACE SAVED IN DB");
        }
        else
        {
            ESP_LOGI("IA_DB", "ERROR TO SAVED THE FACE");
        }
    }
}

extern "C" void model_processing()
{
    if (face_detector == nullptr) 
    {
        ESP_LOGW("IA_BRIDGE", "Modelos no listos todavía. Ignorando frame.");
        return;
    }

    camera_fb_t *fb=esp_camera_fb_get(); 
    if(!fb)
    {
        ESP_LOGW("IA", "Frame de cámara vacío en C++");
        return;
    }

    uint8_t *rgb_buffer = (uint8_t *)heap_caps_malloc(fb->width * fb->height * 3, MALLOC_CAP_SPIRAM);
    if (!rgb_buffer)  
    {
        ESP_LOGE("IA", "Sin memoria para rgb_buffer");
        esp_camera_fb_return(fb);
        return;
    }
    fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buffer);
    //liberamos la camara
    esp_camera_fb_return(fb);

    dl::image::img_t live_frame(rgb_buffer,fb->width,fb->height
    ,dl::image::DL_IMAGE_PIX_TYPE_RGB888);//parametros de la construccion del modelo en un frame en vivo

    //  Correr la detección y el reconocimiento local
    ESP_LOGI("TRY","INCIAR IA");
    auto detect_results = face_detector->run(live_frame);
    ESP_LOGI("TRY","TOASTYYY");

    //liberamos memoria
    heap_caps_free(rgb_buffer);

    if(!detect_results.empty())//hay una cara?
    {
        ESP_LOGI("EUREKA","THERE ARE A FACE YEAHHHHHHH");
        if(g_enroll_requested)//si el enroll es true significa que vamos a guardar una cara en la base de datos
        {
            ESP_LOGI("IA_DB", "Bandera detectada (true). Guardando rostro en la base de datos...");
            esp_err_t result= face_recognizer->enroll(live_frame,detect_results);
            if(result== ESP_OK)
            {
                ESP_LOGI("IA_DB", "FACE SAVED IN DB");
            }
            else
            {
                ESP_LOGI("IA_DB", "ERROR TO SAVED THE FACE");
            }
            g_enroll_requested=false;//volvemos a ponerla en false

        }
        auto recog_results = face_recognizer->recognize(live_frame, detect_results);
        if(recog_results.empty())
        {
            ESP_LOGW("RECOGNIZE", "Cara desconocida.");
            //enviar datos a telegram
            telegram_send_message("Cara_Desconocida");
            telegram_send_image("Rotro_No_encontrado");
        }
        else
        {
            for (auto &r : recog_results)
            {
                ESP_LOGI("RECOGNIZE", "¡Cara reconocida! ID: %d, Similitud: %.2f", r.id, r.similarity);
                telegram_send_message("Cara_Encontrada_Por_favor_Entre");
                
            }
        }

    }
    
}



