#pragma once
#include <Arduino.h>

// Estructura de detección recibida del servidor
struct Deteccion {
    String label;
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

// Compat: tu setup() llama a esto; la dejo como stub (guarda la cola si la necesitas)
void start_ws_task(QueueHandle_t queue);
