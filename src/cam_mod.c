#include "cam_mod.h"

//define pins of the camera, frequency 
static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 10000000,

    .pixel_format = PIXFORMAT_RGB565,//YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA, //Reduce resolution to increase throughput 

    .jpeg_quality = 12,
    .fb_count = 2, 
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY, //cut the most recent frame usefull to stream and less lag 
    .fb_location=CAMERA_FB_IN_PSRAM //this is for the s3
};

typedef enum {
    CAM_MODE_STREAM,
    CAM_MODE_CAPTURE
} cam_mode_t;

static cam_mode_t current_cam_mode=CAM_MODE_STREAM;
//mode of the camera
esp_err_t camera_set_mode(cam_mode_t mode)
{
    //if the current mode there is, not changes
    if(current_cam_mode == mode)
    {
        return ESP_OK;
    }
    
    esp_camera_deinit();

    if(mode == CAM_MODE_STREAM)
    {
        camera_config.frame_size=FRAMESIZE_QVGA;//320X240
        camera_config.jpeg_quality= 15;
        camera_config.fb_count = 2;
        camera_config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else//mode capture
    {
        camera_config.frame_size=FRAMESIZE_SVGA;//800X600
        camera_config.jpeg_quality= 5;
        camera_config.fb_count = 1;
        camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGI("ESP-CAM", "Camera Init Failed");
        return err;
    }
     current_cam_mode=mode;
    return ESP_OK;

}


esp_err_t camera_init(){
    //PWDN is a signal to turn on the camera or turn of, so we can turn on the camera  take a picture and the turn off
    //power up the camera if PWDN pin is defined
    if(CAM_PIN_PWDN != -1){
        gpio_config_t io_conf={
            .pin_bit_mask=(1ULL<< CAM_PIN_PWDN),
            .mode=GPIO_MODE_OUTPUT//OUTPUT
        };
        gpio_config(&io_conf);
        gpio_set_level(CAM_PIN_PWDN,0);//LOW 
    }

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGI("ESP-CAM", "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

esp_err_t camera_capture(httpd_req_t *req){
    //mode
    camera_set_mode(CAM_MODE_CAPTURE);
    //acquire a frame
    camera_fb_t * fb = esp_camera_fb_get(); //fb is frame buzzer and it have a lot of parameters
    if (!fb) {
        ESP_LOGI("ESP-CAM", "Camera Capture Failed");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req,"image/jpeg");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
  
    //send the frame data first
    esp_err_t err = httpd_resp_send(req,(const char *)fb->buf,fb->len);
    
    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    return err;
}

//content for http
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=" PART_BOUNDARY; // this is a stream so there are many parts
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n"; // frames separation
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";//header 

esp_err_t stream_handler(httpd_req_t *req){
    camera_set_mode(CAM_MODE_STREAM);

    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char part_buf[128];
    static int64_t last_frame = 0;

    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE("ESP-CAM", "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);//convert the frame in jpeg
            if(!jpeg_converted){
                ESP_LOGE("ESP-CAM", "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);//free buffer
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        ESP_LOGI("ESP-CAM", "MJPG: %uKB %ums (%.1ffps)",
            (uint32_t)(_jpg_buf_len/1024),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
    }

    last_frame = 0;
    return res;
}

void http_init_server()
{
    httpd_handle_t server = NULL;//server http
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();//default config
    httpd_start(&server, &cfg);//start the server

    static const httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = camera_capture,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &capture_uri); //endpoint structure

    static const httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &stream_uri);
}