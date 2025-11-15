#include "websocket_client.h"
#include "ws_draw.h"
#include <Arduino.h>
#include <ArduinoJson.h>

WebSocketsClient webSocket;  // Definición única

// ============ Helpers ============
static void hexdump(const uint8_t* p, size_t len) {
  const size_t maxDump = 128;
  size_t n = len > maxDump ? maxDump : len;
  for(size_t i=0;i<n;i++){
    if((i%16)==0) Serial.printf("\n  %04u: ", (unsigned)i);
    Serial.printf("%02X ", p[i]);
  }
  if(len>maxDump) Serial.printf("\n  ... (%u bytes mas)", (unsigned)(len-maxDump));
  Serial.println();
}

static bool parse_bbox(JsonObjectConst o, int &x, int &y, int &w, int &h) {
  if (o.containsKey("x") && o.containsKey("y") && (o.containsKey("w")||o.containsKey("width")) && (o.containsKey("h")||o.containsKey("height"))) {
    float xf = o["x"] | 0.0f;
    float yf = o["y"] | 0.0f;
    float wf = o.containsKey("w") ? (o["w"] | 0.0f) : (o["width"]  | 0.0f);
    float hf = o.containsKey("h") ? (o["h"] | 0.0f) : (o["height"] | 0.0f);
    x = xf; y = yf; w = wf; h = hf;
    return true;
  }
  if (o.containsKey("xmin") && o.containsKey("ymin") && o.containsKey("xmax") && o.containsKey("ymax")) {
    float xmin = o["xmin"] | 0.0f;
    float ymin = o["ymin"] | 0.0f;
    float xmax = o["xmax"] | xmin;
    float ymax = o["ymax"] | ymin;
    x = xmin; y = ymin; w = xmax - xmin; h = ymax - ymin;
    return true;
  }
  return false;
}

// Escala automática de cajas desde el espacio fuente (desconocido) a 240x240.
static void handle_detections(JsonArrayConst arr) {
  const int W = 240, H = 240;

  const auto clampi = [](int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); };

  const int n = arr.size();
  if (n <= 0) {
    ws_draw_update_detecciones(nullptr, 0);
    Serial.println("[WS] detecciones vacías: limpio overlays");
    return;
  }

  // Reserva con new[] (NO malloc) porque Deteccion tiene String
  Deteccion* out = new (std::nothrow) Deteccion[n];
  if (!out) { Serial.println("[WS] ERROR: sin memoria para detecciones"); return; }

  // --- 1) Primera pasada: decidir si vienen normalizadas y, si no, estimar tamaño fuente ---
  bool maybeNormalized = true;           // asumir 0..1 si no vemos valores > 1.2
  float maxRight = 0.0f, maxBottom = 0.0f;

  for (JsonVariantConst v : arr) {
    JsonObjectConst o = v.as<JsonObjectConst>();
    if (!o.containsKey("x") || !o.containsKey("y") || !o.containsKey("w") || !o.containsKey("h")) continue;

    // Leer como float para cubrir ambos casos (pixeles o normalizado)
    float fx = o["x"], fy = o["y"], fw = o["w"], fh = o["h"];

    if (fx > 1.2f || fy > 1.2f || fw > 1.2f || fh > 1.2f) maybeNormalized = false; // claro que son pixeles
    maxRight  = maxRight  < (fx + fw) ? (fx + fw) : maxRight;
    maxBottom = maxBottom < (fy + fh) ? (fy + fh) : maxBottom;
  }

  // Caso A: normalizadas → escala directa a 240x240
  // Caso B: píxeles (p.ej. 1280x720) → calcula factor de escala por paquete
  float sx = 1.0f, sy = 1.0f;
  if (maybeNormalized) {
    sx = W; sy = H;
  } else {
    // Evitar divisiones por cero y limitar factores razonables
    if (maxRight  < 16.0f)  maxRight  = 16.0f;
    if (maxBottom < 16.0f)  maxBottom = 16.0f;
    sx = float(W) / maxRight;
    sy = float(H) / maxBottom;
    // Limita factores por seguridad (no deberían explotar)
    if (sx < 0.05f) sx = 0.05f; if (sx > 20.0f) sx = 20.0f;
    if (sy < 0.05f) sy = 0.05f; if (sy > 20.0f) sy = 20.0f;
  }

  // --- 2) Segunda pasada: escalar, clamp y poblar salida ---
  int valid = 0;
  for (int i = 0; i < n; ++i) {
    JsonObjectConst o = arr[i];
    if (!o.containsKey("x") || !o.containsKey("y") || !o.containsKey("w") || !o.containsKey("h")) continue;

    float fx = o["x"], fy = o["y"], fw = o["w"], fh = o["h"];

    // Escala: si venían normalizadas (0..1), multiplicar por 240; si eran píxeles, multiplicar por sx/sy
    int x = int((maybeNormalized ? fx * W : fx * sx) + 0.5f);
    int y = int((maybeNormalized ? fy * H : fy * sy) + 0.5f);
    int w = int((maybeNormalized ? fw * W : fw * sx) + 0.5f);
    int h = int((maybeNormalized ? fh * H : fh * sy) + 0.5f);

    // Clamp a 240x240 (después de escalar)
    x = clampi(x, 0, W - 1);
    y = clampi(y, 0, H - 1);
    w = clampi(w, 1, W - x);
    h = clampi(h, 1, H - y);

    // Asigna en salida
    out[valid].x = x; out[valid].y = y; out[valid].w = w; out[valid].h = h;

    // Acepta "label" o "class"
    if (o.containsKey("label"))        out[valid].label = o["label"].as<const char*>();
    else if (o.containsKey("class"))   out[valid].label = o["class"].as<const char*>();
    else                               out[valid].label = "obj";

    Serial.printf("[WS] det[%d]: x=%d y=%d w=%d h=%d label=%s\n",
                  valid, x, y, w, h, out[valid].label.c_str());
    valid++;
  }

  // Publica y limpia
  ws_draw_update_detecciones(valid ? out : nullptr, valid);
  delete[] out;
  Serial.println("[WS] detecciones actualizadas OK");
}


// ============ Evento principal ============
static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] desconectado");
      break;
    case WStype_CONNECTED:
      Serial.println("[WS] conectado");
      break;
    case WStype_TEXT: {
      Serial.printf("[WS] texto (%u bytes):\n", (unsigned)length);
      Serial.write(payload, length);
      Serial.println();

      // OPTIMIZADO: Reducir buffer JSON de 4096 a 2048 bytes (suficiente para detecciones)
      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) {
        Serial.printf("[WS] JSON error: %s\n", err.c_str());
        hexdump(payload, length);
        break;
      }

      Serial.println("[WS] JSON pretty:");
      serializeJsonPretty(doc, Serial);
      Serial.println();

      if (doc.is<JsonObject>()) {
        JsonObject root = doc.as<JsonObject>();

        // caso: raíz con array de detecciones
        if (root.containsKey("faces") && root["faces"].is<JsonArray>()) {
          handle_detections(root["faces"].as<JsonArrayConst>());
          break;
        }
        if (root.containsKey("detections") && root["detections"].is<JsonArray>()) {
          handle_detections(root["detections"].as<JsonArrayConst>());
          break;
        }

        // caso: raíz es una detección única
        if (root.containsKey("x") || root.containsKey("xmin")) {
          DynamicJsonDocument tmp(256);
          JsonArray arr = tmp.createNestedArray();
          arr.add(root);
          handle_detections(arr);
          break;
        }

        // caso: objeto vacío
        Serial.println("[WS] objeto sin detecciones, limpio overlays");
        ws_draw_update_detecciones(nullptr, 0);
        break;
      }

      if (doc.is<JsonArray>()) {
        handle_detections(doc.as<JsonArrayConst>());
        break;
      }

      Serial.println("[WS] formato no reconocido, limpio overlays");
      ws_draw_update_detecciones(nullptr, 0);
      break;
    }
    default:
      break;
  }
}

// ============ Implementaciones ============
void websocket_init(const char* host, uint16_t port, const char* path, bool useSSL) {
  if (useSSL) webSocket.beginSSL(host, port, path);
  else        webSocket.begin(host, port, path);

  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(4000);
  webSocket.enableHeartbeat(15000, 3000, 2);

  Serial.printf("[WS] init host=%s port=%u path=%s ssl=%d\n",
                host, port, path, useSSL ? 1 : 0);
}

void websocket_loop() {
  webSocket.loop();
}

void websocket_send_frame(const uint8_t* data, size_t len) {
  if (!data || !len) return;
  if (!webSocket.isConnected()) return;
  webSocket.sendBIN(data, len);
}
