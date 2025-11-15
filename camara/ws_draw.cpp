#include "ws_draw.h"
#include "websocket_client.h"
#include "display.h"        // Tu driver TFT (tft.pushImage / drawRect / etc.)
#include <Arduino.h>
#include <freertos/queue.h>

// ============ Estado ============
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static Deteccion gDet[10];
static int gDetCount = 0;

// OPTIMIZADO: Double buffer en PSRAM (sin malloc/free)
#define FRAME_SIZE (240 * 240 * 2)  // 115200 bytes RGB565
static uint8_t* gFrameBuffer[2] = {nullptr, nullptr};  // 2 buffers
static int gWriteBuffer = 0;
static int gReadBuffer = 0;
static bool gFrameReady = false;

// Esta cola te la dejo por compat (si la usas)
static QueueHandle_t gDetectionQueue = nullptr;

// ============ Helpers de dibujo ============
static void drawFrame(uint8_t* buf){
    if(!buf) return;
    // OPTIMIZADO: Usar startWrite/endWrite para transferencia DMA más rápida
    tft.startWrite();
    // NO swap - la cámara ya entrega en el formato correcto
    tft.pushImage(0, 0, 240, 240, (uint16_t*)buf);
    tft.endWrite();
}


// --- REEMPLAZA SOLO ESTA FUNCIÓN ---
static void drawDetections(){
    // Copia local bajo lock muy corto; se dibuja fuera del lock
    Deteccion local[10];
    int n = 0;

    portENTER_CRITICAL(&mux);
    n = gDetCount;
    for(int i = 0; i < n; i++) local[i] = gDet[i];
    portEXIT_CRITICAL(&mux);

    if(n <= 0) return;

    for(int i = 0; i < n; i++){
        const Deteccion &d = local[i];
        // (Opcional) clamp defensivo si en algún caso llegan fuera de rango
        int x = d.x, y = d.y, w = d.w, h = d.h;
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x + w > 240) w = 240 - x;
        if (y + h > 240) h = 240 - y;

        tft.drawRect(x, y, w, h, TFT_RED);
        tft.setCursor(x, (y > 10 ? y - 10 : y));
        tft.setTextColor(TFT_RED);
        tft.setTextSize(1);
        tft.print(d.label);
    }
}


// ============ API ============
void ws_draw_init(){
    // OPTIMIZADO: Alojar buffers en PSRAM (double buffer)
    if(psramFound()) {
        gFrameBuffer[0] = (uint8_t*)ps_malloc(FRAME_SIZE);
        gFrameBuffer[1] = (uint8_t*)ps_malloc(FRAME_SIZE);
        if(gFrameBuffer[0] && gFrameBuffer[1]) {
            Serial.printf("[ws_draw] Double buffer en PSRAM OK (%d bytes x2)\n", FRAME_SIZE);
        } else {
            Serial.println("[ws_draw] ERROR: No se pudo alojar buffers en PSRAM");
        }
    } else {
        Serial.println("[ws_draw] ADVERTENCIA: PSRAM no encontrado!");
    }
    Serial.println("ws_draw_init: inicializado");
}

void start_ws_task(QueueHandle_t queue){
    // Compat con tu setup()
    gDetectionQueue = queue;
}

void ws_draw_set_frame(uint8_t* newFrame){
    // DEPRECATED: Mantener por compatibilidad pero no se usa
    if(newFrame) free(newFrame);
}

// OPTIMIZADO: Copia rápida a double buffer (NO bloquea dibujando)
void ws_draw_set_frame_direct(const uint8_t* cameraBuf, size_t len){
    if(!cameraBuf || len == 0 || !gFrameBuffer[0] || !gFrameBuffer[1]) return;
    if(len > FRAME_SIZE) len = FRAME_SIZE;

    // Copia rápida al buffer de escritura (mínimo tiempo en sección crítica)
    portENTER_CRITICAL(&mux);
    memcpy(gFrameBuffer[gWriteBuffer], cameraBuf, len);
    gFrameReady = true;
    // Intercambiar buffers
    int temp = gWriteBuffer;
    gWriteBuffer = gReadBuffer;
    gReadBuffer = temp;
    portEXIT_CRITICAL(&mux);
}

// --- REEMPLAZA SOLO ESTA FUNCIÓN ---
void ws_draw_update_detecciones(Deteccion* arr, int count){
    // Protege el estado compartido (gDet/gDetCount)
    portENTER_CRITICAL(&mux);
    if(!arr || count <= 0){
        gDetCount = 0;              // limpiar overlays
        portEXIT_CRITICAL(&mux);
        return;
    }
    if(count > 10) count = 10;
    for(int i = 0; i < count; i++) gDet[i] = arr[i];
    gDetCount = count;
    portEXIT_CRITICAL(&mux);
}


void ws_draw_loop(){
    // Mantener WS vivo
    websocket_loop();

    // OPTIMIZADO: Dibujar desde el buffer de lectura (sin free)
    bool ready = false;
    portENTER_CRITICAL(&mux);
    ready = gFrameReady;
    if(ready) gFrameReady = false;
    portEXIT_CRITICAL(&mux);

    if(ready && gFrameBuffer[gReadBuffer]){
        drawFrame(gFrameBuffer[gReadBuffer]);
        drawDetections();
    }
}
