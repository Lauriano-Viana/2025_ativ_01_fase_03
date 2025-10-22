/*
 * Sistema de Monitoramento Cardíaco Vestível - ESP32 (Versão Simples)
 * Fase 3 - Atividade 1: Edge Computing + Cloud Computing
 * 
 * Sensores:
 * - DHT22: Temperatura e Umidade
 * - Botão: Simulação de batimentos cardíacos (pressionar para simular batida)
 * 
 * Funcionalidades:
 * - Armazenamento local em SPIFFS
 * - Resiliência offline
 * - Transmissão via Serial (simulando MQTT)
 * - Sincronização de dados pendentes
 */

#include <WiFi.h>
#include <DHT.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <time.h>

// Configurações dos sensores
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define HEART_RATE_BUTTON 2

// Configurações dos LEDs
#define WIFI_LED_PIN 5      // LED Azul - Status WiFi
#define MQTT_LED_PIN 18     // LED Verde - Status MQTT
#define ALERT_LED_PIN 19    // LED Vermelho - Alertas

// Configurações WiFi (Wokwi simula automaticamente)
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Objetos
DHT dht(DHT_PIN, DHT_TYPE);

// Variáveis de controle
bool wifiConnected = false;
unsigned long lastSensorRead = 0;
unsigned long lastHeartBeat = 0;
int heartRate = 0;
int heartRateCount = 0;
unsigned long heartRateStartTime = 0;
const unsigned long HEART_RATE_WINDOW = 60000; // 1 minuto

// Configurações de armazenamento
const int MAX_STORED_READINGS = 1000; // Limite de amostras offline
const unsigned long SENSOR_INTERVAL = 5000; // 5 segundos entre leituras

// Estrutura para dados dos sensores
struct SensorData {
  float temperature;
  float humidity;
  int heartRate;
  unsigned long timestamp;
  bool sent;
};

// Buffer para dados offline
SensorData offlineBuffer[MAX_STORED_READINGS];
int bufferIndex = 0;
int totalStored = 0;

void setup() {
  Serial.begin(115200);
  
  // Inicializar sensores
  dht.begin();
  pinMode(HEART_RATE_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HEART_RATE_BUTTON), onHeartBeat, FALLING);
  
  // Configurar LEDs
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(MQTT_LED_PIN, OUTPUT);
  pinMode(ALERT_LED_PIN, OUTPUT);
  
  // Inicializar LEDs (todos apagados)
  digitalWrite(WIFI_LED_PIN, LOW);
  digitalWrite(MQTT_LED_PIN, LOW);
  digitalWrite(ALERT_LED_PIN, LOW);
  
  // Inicializar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS");
    return;
  }
  
  // Carregar dados offline salvos
  loadOfflineData();
  
  // Configurar WiFi
  setupWiFi();
  
  Serial.println("Sistema de Monitoramento Cardíaco Iniciado");
  Serial.println("Pressione o botão para simular batimentos cardíacos");
  Serial.println("=== DADOS SERÃO EXIBIDOS VIA SERIAL (SIMULANDO MQTT) ===");
  
  // Teste inicial dos LEDs
  testLEDs();
}

void loop() {
  // Verificar conectividade WiFi
  checkWiFiConnection();
  
  // Ler sensores periodicamente
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    readSensors();
    lastSensorRead = millis();
  }
  
  // Calcular frequência cardíaca
  calculateHeartRate();
  
  // Tentar sincronizar dados offline
  if (wifiConnected && totalStored > 0) {
    syncOfflineData();
  }
  
  delay(100);
}

void setupWiFi() {
  // No Wokwi, simular WiFi sempre conectado para demonstração
  wifiConnected = true;
  digitalWrite(WIFI_LED_PIN, HIGH);  // Acender LED azul (WiFi)
  Serial.println("WiFi simulado conectado (Wokwi)!");
  Serial.println("IP: 192.168.1.100 (simulado)");
  Serial.println("=== MODO DEMONSTRAÇÃO ATIVADO ===");
  Serial.println("🔵 LED Azul: WiFi conectado");
}

void checkWiFiConnection() {
  // No Wokwi, manter sempre conectado para demonstração
  wifiConnected = true;
}

void readSensors() {
  // Ler DHT22
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Verificar se as leituras são válidas
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Erro na leitura do DHT22");
    return;
  }
  
  // Criar estrutura de dados
  SensorData data;
  data.temperature = temperature;
  data.humidity = humidity;
  data.heartRate = heartRate;
  data.timestamp = millis();
  data.sent = false;
  
  // Armazenar dados
  storeData(data);
  
  // Exibir dados no Serial
  Serial.println("=== Dados dos Sensores ===");
  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Umidade: ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.print("Frequência Cardíaca: ");
  Serial.print(heartRate);
  Serial.println(" bpm");
  Serial.print("Conectado: ");
  Serial.println(wifiConnected ? "Sim" : "Não");
  Serial.println("=========================");
  
  // Enviar via Serial (simulando MQTT)
  if (wifiConnected) {
    sendDataToCloud(data);
  }
}

void onHeartBeat() {
  static unsigned long lastBeat = 0;
  unsigned long now = millis();
  
  // Debounce - evitar múltiplas detecções
  if (now - lastBeat > 200) {
    heartRateCount++;
    lastBeat = now;
    
    if (heartRateStartTime == 0) {
      heartRateStartTime = now;
    }
  }
}

void calculateHeartRate() {
  unsigned long now = millis();
  
  if (heartRateStartTime > 0 && now - heartRateStartTime >= HEART_RATE_WINDOW) {
    heartRate = (heartRateCount * 60000) / (now - heartRateStartTime);
    
    // Reset para próximo cálculo
    heartRateCount = 0;
    heartRateStartTime = 0;
    
    Serial.print("Frequência cardíaca calculada: ");
    Serial.print(heartRate);
    Serial.println(" bpm");
  }
}

void storeData(SensorData data) {
  // Armazenar no buffer
  if (bufferIndex < MAX_STORED_READINGS) {
    offlineBuffer[bufferIndex] = data;
    bufferIndex++;
    totalStored++;
  } else {
    // Buffer cheio - sobrescrever dados mais antigos
    bufferIndex = 0;
    offlineBuffer[bufferIndex] = data;
    totalStored = MAX_STORED_READINGS;
  }
  
  // Salvar no SPIFFS
  saveToSPIFFS(data);
  
  Serial.print("Dados armazenados. Total offline: ");
  Serial.println(totalStored);
}

void saveToSPIFFS(SensorData data) {
  File file = SPIFFS.open("/sensor_data.json", "a");
  if (file) {
    DynamicJsonDocument doc(1024);
    doc["temperature"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["heartRate"] = data.heartRate;
    doc["timestamp"] = data.timestamp;
    doc["sent"] = data.sent;
    
    serializeJson(doc, file);
    file.println();
    file.close();
  }
}

void loadOfflineData() {
  File file = SPIFFS.open("/sensor_data.json", "r");
  if (file) {
    totalStored = 0;
    bufferIndex = 0;
    
    while (file.available() && totalStored < MAX_STORED_READINGS) {
      String line = file.readStringUntil('\n');
      
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, line);
      
      if (!doc["sent"].as<bool>()) {
        offlineBuffer[totalStored].temperature = doc["temperature"];
        offlineBuffer[totalStored].humidity = doc["humidity"];
        offlineBuffer[totalStored].heartRate = doc["heartRate"];
        offlineBuffer[totalStored].timestamp = doc["timestamp"];
        offlineBuffer[totalStored].sent = false;
        
        totalStored++;
      }
    }
    
    file.close();
    bufferIndex = totalStored;
    
    Serial.print("Carregados ");
    Serial.print(totalStored);
    Serial.println(" registros offline");
  }
}

void syncOfflineData() {
  static int syncIndex = 0;
  static unsigned long lastSync = 0;
  
  if (millis() - lastSync >= 2000 && syncIndex < totalStored) { // Sincronizar a cada 2 segundos
    SensorData data = offlineBuffer[syncIndex];
    
    if (sendDataToCloud(data)) {
      // Marcar como enviado
      offlineBuffer[syncIndex].sent = true;
      
      // Atualizar no SPIFFS
      updateSPIFFSRecord(syncIndex);
      
      syncIndex++;
      Serial.print("Sincronizado registro ");
      Serial.print(syncIndex);
      Serial.print(" de ");
      Serial.println(totalStored);
    }
    
    lastSync = millis();
  }
  
  // Se todos os dados foram sincronizados, limpar buffer
  if (syncIndex >= totalStored && totalStored > 0) {
    clearOfflineData();
    syncIndex = 0;
    totalStored = 0;
    bufferIndex = 0;
    Serial.println("Todos os dados offline foram sincronizados!");
  }
}

bool sendDataToCloud(SensorData data) {
  if (!wifiConnected) {
    return false;
  }
  
  // Piscar LED verde (MQTT) durante transmissão
  blinkMQTTLED();
  
  // Simular envio MQTT via Serial
  Serial.println("📡 === DADOS ENVIADOS PARA A NUVEM (SIMULANDO MQTT) ===");
  Serial.println("🟢 LED Verde: MQTT ativo");
  
  // Dados individuais
  Serial.print("📤 Tópico: medical/sensors/temperature | Valor: ");
  Serial.println(data.temperature);
  
  Serial.print("📤 Tópico: medical/sensors/humidity | Valor: ");
  Serial.println(data.humidity);
  
  Serial.print("📤 Tópico: medical/sensors/heartrate | Valor: ");
  Serial.println(data.heartRate);
  
  // Dados completos em JSON
  DynamicJsonDocument doc(1024);
  doc["device_id"] = "ESP32_Medical_001";
  doc["temperature"] = data.temperature;
  doc["humidity"] = data.humidity;
  doc["heart_rate"] = data.heartRate;
  doc["timestamp"] = data.timestamp;
  doc["battery_level"] = 85; // Simulado
  doc["signal_strength"] = WiFi.RSSI();
  
  String payload;
  serializeJson(doc, payload);
  
  Serial.print("📤 Tópico: medical/sensors/alldata | Payload: ");
  Serial.println(payload);
  
  // Verificar alertas
  checkAlerts(data);
  
  Serial.println("📡 === FIM DA TRANSMISSÃO ===");
  
  return true;
}

void checkAlerts(SensorData data) {
  Serial.println("🔔 === VERIFICAÇÃO DE ALERTAS ===");
  
  bool hasAlert = false;
  
  // Verificar temperatura
  if (data.temperature > 38) {
    Serial.println("🚨 ALERTA ALTO: Temperatura alta (" + String(data.temperature) + "°C)");
    hasAlert = true;
  } else if (data.temperature > 35) {
    Serial.println("⚠️  ATENÇÃO: Temperatura elevada (" + String(data.temperature) + "°C)");
    hasAlert = true;
  }
  
  // Verificar frequência cardíaca
  if (data.heartRate > 120) {
    Serial.println("🚨 ALERTA ALTO: Frequência cardíaca alta (" + String(data.heartRate) + " bpm)");
    hasAlert = true;
  } else if (data.heartRate > 100) {
    Serial.println("⚠️  ATENÇÃO: Frequência cardíaca elevada (" + String(data.heartRate) + " bpm)");
    hasAlert = true;
  }
  
  // Controlar LED vermelho (alertas)
  if (hasAlert) {
    digitalWrite(ALERT_LED_PIN, HIGH);  // Acender LED vermelho
    Serial.println("🔴 LED Vermelho: ALERTA ATIVO");
  } else {
    digitalWrite(ALERT_LED_PIN, LOW);   // Apagar LED vermelho
    Serial.println("✅ Todos os parâmetros dentro da normalidade");
  }
  
  Serial.println("🔔 === FIM DA VERIFICAÇÃO ===");
}

void updateSPIFFSRecord(int index) {
  // Implementação simplificada - em produção, seria necessário reescrever o arquivo
  File file = SPIFFS.open("/sensor_data.json", "r");
  if (file) {
    String content = file.readString();
    file.close();
    
    // Marcar como enviado (implementação simplificada)
    content.replace("\"sent\":false", "\"sent\":true");
    
    file = SPIFFS.open("/sensor_data.json", "w");
    if (file) {
      file.print(content);
      file.close();
    }
  }
}

void clearOfflineData() {
  SPIFFS.remove("/sensor_data.json");
  Serial.println("Dados offline limpos do SPIFFS");
}

void testLEDs() {
  Serial.println("🔧 === TESTE DOS LEDs ===");
  
  // Teste LED Azul (WiFi)
  Serial.println("🔵 Testando LED Azul (WiFi)...");
  digitalWrite(WIFI_LED_PIN, HIGH);
  delay(500);
  digitalWrite(WIFI_LED_PIN, LOW);
  delay(500);
  
  // Teste LED Verde (MQTT)
  Serial.println("🟢 Testando LED Verde (MQTT)...");
  digitalWrite(MQTT_LED_PIN, HIGH);
  delay(500);
  digitalWrite(MQTT_LED_PIN, LOW);
  delay(500);
  
  // Teste LED Vermelho (Alertas)
  Serial.println("🔴 Testando LED Vermelho (Alertas)...");
  digitalWrite(ALERT_LED_PIN, HIGH);
  delay(500);
  digitalWrite(ALERT_LED_PIN, LOW);
  delay(500);
  
  Serial.println("✅ Teste dos LEDs concluído!");
  Serial.println("🔧 === FIM DO TESTE ===");
}

void blinkMQTTLED() {
  // Piscar LED verde 3 vezes para indicar transmissão MQTT
  for (int i = 0; i < 3; i++) {
    digitalWrite(MQTT_LED_PIN, HIGH);
    delay(100);
    digitalWrite(MQTT_LED_PIN, LOW);
    delay(100);
  }
}

