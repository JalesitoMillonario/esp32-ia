#pragma once
#include <Arduino.h>

// Estructura de detección recibida del servidor
// OPTIMIZADO: char[] fijo en lugar de String para evitar corrupción de heap
struct Deteccion {
    char label[32];  // Buffer fijo (antes String dinámico)
    int x;
    int y;
    int w;
    int h;
};

// Inicialización de la capa de dibujo
void ws_draw_init();

// Loop principal (llámalo en loop())
void ws_draw_loop();

// Actualizar detecciones (ws_draw copia internamente)
void ws_draw_update_detecciones(Deteccion* detecciones, int count);

// Entregar un frame a ws_draw (ws_draw toma propiedad y lo libera tras dibujar)
void ws_draw_set_frame(uint8_t* cameraBuf);

// OPTIMIZADO: Usar frame directamente sin copia (más eficiente, usa el buffer de la cámara)
void ws_draw_set_frame_direct(const uint8_t* cameraBuf, size_t len);

// NUEVO: Recibir JPEG y decodificar a RGB565 para TFT
void ws_draw_set_frame_jpeg(const uint8_t* jpegBuf, size_t jpegLen);

// Compat: tu setup() llama a esto; la dejo como stub (guarda la cola si la necesitas)
void start_ws_task(QueueHandle_t queue);
