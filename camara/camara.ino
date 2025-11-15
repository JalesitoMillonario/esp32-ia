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

    // OPTIMIZADO: Mostrar memoria disponible al inicio
    Serial.printf("\n=== DIAGNÓSTICO DE MEMORIA ===\n");
    Serial.printf("Heap libre: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM libre: %u bytes\n", ESP.getFreePsram());
    Serial.printf("PSRAM total: %u bytes\n", ESP.getPsramSize());

    // OPTIMIZADO: Inicializar cámara PRIMERO (necesita más memoria)
    Serial.println("\n📷 Inicializando cámara...");
    if (!camera_init()) {
        Serial.println("❌ Error al iniciar la cámara");
        Serial.printf("Heap libre después del fallo: %u bytes\n", ESP.getFreeHeap());
        Serial.printf("PSRAM libre después del fallo: %u bytes\n", ESP.getFreePsram());
        while (true) delay(1000);
    }
    Serial.println("✅ Cámara inicializada");
    Serial.printf("Heap libre: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM libre: %u bytes\n", ESP.getFreePsram());

    // Inicialización pantalla
    tft.begin();
    tft.setRotation(4);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.print("Iniciando...");

    // Conexión WiFi
    Serial.print("\n📡 Conectando WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi conectado");
    Serial.println(WiFi.localIP());

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 120);
    tft.print("WiFi OK");

    // Inicializar colas y mutex (OPTIMIZADO: reducir tamaño)
    captureQueue   = xQueueCreate(1, sizeof(uint8_t*));     // Reducido de 2 a 1
    detectionQueue = xQueueCreate(2, sizeof(Deteccion*));   // Reducido de 4 a 2
    captureMutex   = xSemaphoreCreateMutex();

    Serial.printf("Heap libre después de colas: %u bytes\n", ESP.getFreeHeap());

    // Inicializar WebSocket y capa de dibujo
    websocket_init("3b6bec75bba4.ngrok-free.app", 443, "/ws", true);
    ws_draw_init();

    // NUEVO: Iniciar tarea asíncrona de WebSocket (evita bloqueos)
    websocket_start_task();
    Serial.printf("Heap libre después de WebSocket: %u bytes\n", ESP.getFreeHeap());

    // Crear tareas asincrónicas
    create_camera_task(captureQueue, detectionQueue, captureMutex);
    start_ws_task(detectionQueue);  // compat

    Serial.printf("\n✅ Sistema inicializado - Heap final: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM final: %u bytes\n", ESP.getFreePsram());
}

void loop() {
    // Mantiene WS y dibuja el último frame + detecciones
    ws_draw_loop();

    // OPTIMIZADO: Pequeño delay para no saturar el core (mejora estabilidad)
    delay(10);  // 10ms = máx 100 FPS de dibujo (suficiente para 15 FPS de cámara)
}
