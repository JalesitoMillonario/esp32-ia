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

        // OPTIMIZADO: Mayor stack (12KB) y prioridad alta (5) para evitar bloqueos
        xTaskCreatePinnedToCore(
            loopTask_camera,
            "loopTask_camera",
            12288,      // Stack aumentado (8KB -> 12KB)
            nullptr,
            5,          // Prioridad ALTA (1 -> 5, mayor que WiFi/WS)
            &cameraTaskHandle,
            1           // Core 1 (separado de WiFi que está en core 0)
        );
    }
}

void loopTask_camera(void *pvParameters) {
    const int frameDelay = 66; // ~15 FPS (~100ms para estabilidad extra)

    while(camera_task_flag) {
        fb = esp_camera_fb_get();
        if(fb) {
            // 1. Copiar RGB565 a display (rápido, double buffer)
            ws_draw_set_frame_direct(fb->buf, fb->len);

            // 2. Enviar RGB565 por WebSocket de forma ASÍNCRONA (no bloquea)
            websocket_send_frame_async(fb->buf, fb->len);

            // 3. Retornar buffer a cámara inmediatamente
            esp_camera_fb_return(fb);
        }

        // Feed watchdog implícitamente con vTaskDelay
        vTaskDelay(frameDelay / portTICK_PERIOD_MS);
    }
    vTaskDelete(cameraTaskHandle);
}

void stop_camera_task(void) {
    camera_task_flag = 0;
}
