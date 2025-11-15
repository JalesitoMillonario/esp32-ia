#pragma once
#include <Arduino.h>
#include <WebSocketsClient.h>

// ¡SOLO declaración! (no definas la variable aquí)
extern WebSocketsClient webSocket;

// Estructura opcional, por compatibilidad
struct frame_item_t {
    const uint8_t* data;
    size_t len;
};

// Funciones usadas por el resto del proyecto
void websocket_init(const char* host, uint16_t port, const char* path, bool useSSL = false);
void websocket_loop();
void websocket_send_frame(const uint8_t* data, size_t len);

// NUEVO: Envío asíncrono de frames (no bloquea la tarea de cámara)
void websocket_send_frame_async(const uint8_t* data, size_t len);
void websocket_start_task();
