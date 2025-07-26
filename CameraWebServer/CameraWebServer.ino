#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "ESP32Servo.h"
#include <AsyncUDP.h> 

#define CAMERA_MODEL_AI_THINKER
const char* ssid = "josue";
const char* password = "J0su32003!";

const int panPin = 12;
const int tiltPin = 13;

const int UDP_PORT = 12345;

#include "camera_pins.h"

httpd_handle_t stream_httpd = NULL;
Servo panServo;
Servo tiltServo;
AsyncUDP udp;

static esp_err_t stream_handler(httpd_req_t *req);
void startCameraServer();

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[128];
    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=--frame");
    if(res != ESP_OK){ return res; }
    while(true){
        fb = esp_camera_fb_get();
        if (!fb) { res = ESP_FAIL; break; }
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){ esp_camera_fb_return(fb); res = ESP_FAIL; break; }
        } else { _jpg_buf_len = fb->len; _jpg_buf = fb->buf; }
        sprintf(part_buf, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
        if(httpd_resp_send_chunk(req, "--frame\r\n", 9) != ESP_OK || httpd_resp_send_chunk(req, part_buf, strlen(part_buf)) != ESP_OK || httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len) != ESP_OK || httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) { res = ESP_FAIL; }
        if(fb->format != PIXFORMAT_JPEG){ free(_jpg_buf); }
        esp_camera_fb_return(fb);
        if(res != ESP_OK){ break; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return res;
}

void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

void setup() {
    Serial.begin(115200);
    panServo.attach(panPin);
    tiltServo.attach(tiltPin);
    panServo.write(90); tiltServo.write(90);
    Serial.println("Servos inicializados.");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0; config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM; config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM; config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM; config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM; config.xclk_freq_hz = 20000000; config.pixel_format = PIXFORMAT_JPEG; config.frame_size = FRAMESIZE_128X128; config.jpeg_quality = 12; config.grab_mode = CAMERA_GRAB_LATEST; config.fb_count = 1; config.fb_location = CAMERA_FB_IN_PSRAM;
    if (esp_camera_init(&config) != ESP_OK) { Serial.println("Falha na câmera!"); return; }

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWi-Fi conectado!");
    Serial.print("Stream: http://"); Serial.print(WiFi.localIP()); Serial.println("/stream");
    Serial.printf("Escutando por comandos UDP na porta %d\n", UDP_PORT);

    if (udp.listen(UDP_PORT)) {
        udp.onPacket([](AsyncUDPPacket packet) {
            // Converte os dados do pacote para uma String
            String command = (char*)packet.data();
            command = command.substring(0, packet.length());
            command.trim();

            int xPos = command.indexOf('X');
            int yPos = command.indexOf('Y');

            if (xPos != -1 && yPos != -1) {
                String panAngleStr = command.substring(xPos + 1, yPos);
                String tiltAngleStr = command.substring(yPos + 1);
                
                int panAngle = panAngleStr.toInt();
                int tiltAngle = tiltAngleStr.toInt();

                panServo.write(panAngle);
                tiltServo.write(tiltAngle);

            }
        });
    }

    startCameraServer();
}

void loop() {
}
