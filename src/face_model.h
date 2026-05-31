#pragma once
#include "sdkconfig.h"
#include <stdint.h>

#define TELEGRAM_TOKEN "8806439059:AAGSX5yowqHFkg0_c7em_Q_6ye1brx7qGtc"
#define TELEGRAM_CHAT_ID "1303439449"

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_camera.h"

#ifdef __cplusplus
}

#endif

#ifdef __cplusplus
#include "dl_image_jpeg.hpp"
#include "dl_detect_base.hpp"
#include "dl_feat_base.hpp"
#include "dl_recognition_database.hpp"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include <string>
#endif


#ifdef __cplusplus
extern "C" {
#endif
    
    // Aquí declaras las funciones que quieres que el main.c pueda usar
    // Solo usa tipos de datos básicos de C (int, float, char, etc.)
    void model_processing();
    void models_init();
    void put_in_db();
#ifdef __cplusplus   
}
#endif