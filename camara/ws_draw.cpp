#include "ws_draw.h"
#include "websocket_client.h"
#include "display.h"        // Tu driver TFT (tft.pushImage / drawRect / etc.)
#include <Arduino.h>
#include <freertos/queue.h>

// ============ Estado ============
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static Deteccion gDet[10];
static int gDetCount = 0;

static uint8_t* gFrame = nullptr;

// Esta cola te la dejo por compat (si la usas)
static QueueHandle_t gDetectionQueue = nullptr;

// ============ Helpers de dibujo ============
static void drawFrame(uint8_t* buf){
    if(!buf) return;
    // Ajusta el tamaño si usas otro (tu camera_init usa FRAMESIZE_240X240 RGB565)
    tft.pushImage(0, 0, 240, 240, (uint16_t*)buf);
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
    Serial.println("ws_draw_init: inicializado");
}

void start_ws_task(QueueHandle_t queue){
    // Compat con tu setup()
    gDetectionQueue = queue;
}

void ws_draw_set_frame(uint8_t* newFrame){
    if(!newFrame) return;
    portENTER_CRITICAL(&mux);
    if(gFrame) free(gFrame);
    gFrame = newFrame;          // Propiedad pasa a ws_draw
    portEXIT_CRITICAL(&mux);
    Serial.println("ws_draw_set_frame: frame actualizado");
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

    // Intercambio atómico del frame
    uint8_t* local = nullptr;
    portENTER_CRITICAL(&mux);
    if(gFrame){ local = gFrame; gFrame = nullptr; }
    portEXIT_CRITICAL(&mux);

    if(local){
        drawFrame(local);
        drawDetections();
        free(local);
    }
}
