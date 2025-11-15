#include "camera_ui.h"
#include "ws_draw.h"
#include "camera.h"
#include "display.h"
#include "websocket_client.h"

camera_fb_t *fb = nullptr;
TaskHandle_t cameraTaskHandle = nullptr;
static int camera_task_flag = 0;

// Compat: si usas estas referencias en otro lado
static QueueHandle_t captureQueueLocal;
static QueueHandle_t detectionQueueLocal;
static SemaphoreHandle_t captureMutexLocal;

void loopTask_camera(void *pvParameters);

void create_camera_task(QueueHandle_t captureQueue, QueueHandle_t detectionQueue, SemaphoreHandle_t captureMutex) {
    if(camera_task_flag == 0) {
        camera_task_flag   = 1;
        captureQueueLocal   = captureQueue;
        detectionQueueLocal = detectionQueue;
        captureMutexLocal   = captureMutex;
        xTaskCreate(loopTask_camera, "loopTask_camera", 8192, nullptr, 1, &cameraTaskHandle);
    }
}

void loopTask_camera(void *pvParameters) {
    const int frameDelay = 66; // ~15 FPS
    while(camera_task_flag) {
        fb = esp_camera_fb_get();
        if(fb) {
            // 1) Enviar frame por WebSocket (copia interna en su task)
            websocket_send_frame(fb->buf, fb->len);

            // 2) Mostrar en TFT: entrega copia a ws_draw (ws_draw la libera)
            uint8_t* localFrame = (uint8_t*)malloc(fb->len);
            if(localFrame) {
                memcpy(localFrame, fb->buf, fb->len);
                ws_draw_set_frame(localFrame);
            }

            esp_camera_fb_return(fb);
        }
        vTaskDelay(frameDelay / portTICK_PERIOD_MS);
    }
    vTaskDelete(cameraTaskHandle);
}

void stop_camera_task(void) {
    camera_task_flag = 0;
}
