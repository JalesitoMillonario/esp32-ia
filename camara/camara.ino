#include <Arduino.h>
#include <WiFi.h>
#include "display.h"
#include "camera.h"
#include "camera_ui.h"
#include "websocket_client.h"
#include "ws_draw.h"

#include <freertos/queue.h>
#include <freertos/semphr.h>

// --- Configuración WiFi ---
const char* ssid     = "DIGIFIBRA-2hK2";
const char* password = "fEdfD236996S";

// --- Colas y mutex ---
QueueHandle_t captureQueue;      // (opcional) si la usa tu cámara
QueueHandle_t detectionQueue;    // detecciones del servidor -> ws_draw
SemaphoreHandle_t captureMutex;

void setup() {
    Serial.begin(115200);

    // Inicialización pantalla (como lo tenías)
    tft.begin();
    tft.setRotation(4);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.print("Iniciando...");

    // Conexión WiFi
    Serial.print("Conectando WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi conectado");
    Serial.println(WiFi.localIP());

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 120);
    tft.print("✅ WiFi conectado");

    // Inicializar colas y mutex
    captureQueue   = xQueueCreate(2, sizeof(uint8_t*));     // si no la usas, puedes omitirla
    detectionQueue = xQueueCreate(4, sizeof(Deteccion*));   // *** importante: detecciones ***
    captureMutex   = xSemaphoreCreateMutex();

    // Inicializar cámara (tu implementación)
    if (!camera_init()) {
        Serial.println("❌ Error al iniciar la cámara");
        tft.fillScreen(TFT_RED);
        tft.setCursor(20, 120);
        tft.print("Camara fallida!");
        while (true) delay(1000);
    }

    // Inicializar WebSocket y capa de dibujo
    websocket_init("3b6bec75bba4.ngrok-free.app", 443, "/ws", true);
    ws_draw_init();

    // Crear tareas asincrónicas (tus mismas llamadas)
    create_camera_task(captureQueue, detectionQueue, captureMutex);
    start_ws_task(detectionQueue);  // compat: la función existe (stub) y guarda la cola si la quieres usar luego
}

void loop() {
    // Mantiene WS y dibuja el último frame + detecciones
    ws_draw_loop();
}
