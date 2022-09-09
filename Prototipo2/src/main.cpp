#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WebServerConfig.h>
#include <math.h>

// =================== ESP WIFI Server - Persistent Variables ===================
const char *AP_SSID = "Termofluidos_ESP_Config";

void setupServer();

AsyncWebServer espServer(80);
// =================== ESP WIFI Server - Persistent Variables ===================

// =================== ESP WIFI Client ===================
const unsigned int MQTT_CLIENT_ID_START = 0;
const unsigned int MQTT_CLIENT_ID_MAX_SIZE = 50;

const unsigned int MQTT_TOKEN_START = MQTT_CLIENT_ID_START + MQTT_CLIENT_ID_MAX_SIZE;
const unsigned int MQTT_TOKEN_MAX_SIZE = 50;

const unsigned int WIFI_SSID_START = MQTT_TOKEN_START + MQTT_TOKEN_MAX_SIZE;
const unsigned int WIFI_SSID_MAX_SIZE = 50;

const unsigned int WIFI_PASS_START = WIFI_SSID_START + WIFI_SSID_MAX_SIZE;
const unsigned int WIFI_PASS_MAX_SIZE = 50;

const unsigned int MAX_SIZE = WIFI_PASS_START + WIFI_PASS_MAX_SIZE;

String mqtt_client_id = "";
String mqtt_token = ""; // "7e2e7399-ccae-470a-9c2a-0de40bc3f208";

String wifi_ssid = "";
String wifi_pass = "";

void setupWifi();
void carregaDados();

WiFiClient espClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
// =================== ESP WIFI Client ===================

// ------------------- MQTT -------------------
const int MQTT_PORT = 1883;
const char *MQTT_SERVER = "mqtt.tago.io";
const char *MQTT_USER = "Token";

const char *TOPICO_VARIAVEIS = "variaveis";

void setupMqtt();
void sendRecurrentMessages();

PubSubClient client(espClient);
// ------------------- MQTT -------------------

// ------------------ ACS712 - Medidor de corrente ------------------
const unsigned int GRID_VOLTAGE = 220;

const unsigned int PIN_CURRENT = A0;
const unsigned int PIN_CURRENT_RMS_OUT = D1;

const unsigned long TEMPO_AMOSTRAS = 5000;
const unsigned int TOTAL_AMOSTRAS = 5000;
const unsigned int AMOSTRAS_OFFSET = 1024;
const double ESP_VOLTAGE = 3.234f;
const int ANALOG_BIT_RESOLUTION = 10;
const double ANALOG_BIT_RESOLUTION_VALUE = 1024.0;
const double RESOLUTION_ESP = ESP_VOLTAGE / ANALOG_BIT_RESOLUTION_VALUE;
const double MODULE_SENSIBILITY = 0.197847784f;
const double RESOLUTION_MODULE = RESOLUTION_ESP / MODULE_SENSIBILITY;

unsigned long lastSendMillis = 0;
unsigned int amostras = 0;
double offsetADC = (2.5f / ESP_VOLTAGE) * 1024.0;
double accumulateADC = 0.0;
double currentRms = 0.0;

void acumulateCurrentRms();
double getResolutionModule();
double getCurrentRms();
void sendRecurrentMessagesACS712();
bool enviarCorrente();
bool enviarCorrenteMqtt();
// ------------------ ACS712 - Medidor de corrente ------------------

void DebugMessage(String message);
void DebugMessage(String message, bool skipLine);

void setup()
{
    Serial.begin(115200);

    analogWriteResolution(ANALOG_BIT_RESOLUTION); // 10 bits
    analogWriteFreq(10000);                       // 100 a 40000Hz
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIN_CURRENT, INPUT);
    pinMode(PIN_CURRENT_RMS_OUT, OUTPUT);

    EEPROM.begin(MAX_SIZE);

    WiFi.mode(WIFI_AP_STA);
    carregaDados();
    setupServer();
}

void loop()
{
    setupWifi();
    setupMqtt();

    acumulateCurrentRms();

    if (WiFi.isConnected() && client.connected())
    {
        client.loop();
        sendRecurrentMessages();
    }

    sendRecurrentMessagesACS712();

    if (enviarCorrente())
    {
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);
        amostras = 0;
        accumulateADC = 0.0;
    }

    if (enviarCorrenteMqtt())
    {
        lastSendMillis = millis();
    }
}

bool enviarCorrente()
{
    return amostras >= TOTAL_AMOSTRAS;
}

bool enviarCorrenteMqtt()
{
    return lastSendMillis + TEMPO_AMOSTRAS < millis();
}

unsigned long getTime()
{
    timeClient.update();
    return timeClient.getEpochTime();
}

String escreveObjetoJson(String Key, String Value, String Unit, String variavelExistente)
{
    String retorno = "{\"variable\":\"" + Key + "\",\"value\":\"" + Value + "\",\"serial\":" + String(getTime()) + ",\"unit\":\"" + Unit + "\"}";
    if (!variavelExistente.endsWith("["))
        retorno = "," + retorno;

    return retorno;
}

void acumulateCurrentRms()
{
    int sampleI = analogRead(PIN_CURRENT);

    offsetADC += (sampleI - offsetADC) / AMOSTRAS_OFFSET;
    // DebugMessage("offsetADC: " + String(offsetADC, 2));
    accumulateADC += sq(sampleI - offsetADC);
    amostras++;
}

double getCurrentRms()
{
    return (sqrt(accumulateADC / amostras) * RESOLUTION_MODULE);
}

void sendRecurrentMessagesACS712()
{
    if (enviarCorrente())
    {
        currentRms = getCurrentRms();
        // Corrente RMS para abranger mais da
        // faixa de saída já que a resistência chega
        // apenas a 1A
        double correnteRmsMultiplicado = currentRms;
        int correnteRmsPwmInt = int((correnteRmsMultiplicado * ANALOG_BIT_RESOLUTION_VALUE));

        analogWrite(PIN_CURRENT_RMS_OUT, correnteRmsPwmInt);
        DebugMessage("Corrente: " + String(currentRms, 3) + " - Offset: " + String(offsetADC, 1) + " - CorrenteRmsMultiplicado: " + String(correnteRmsPwmInt, 3));
    }
}

void sendRecurrentMessages()
{
    String variaveis = "[";

    if (enviarCorrenteMqtt())
    {
        unsigned long durationAmostras = millis() - lastSendMillis;

        variaveis += escreveObjetoJson("current_rms", String(currentRms, 6), "A", variaveis);
        variaveis += escreveObjetoJson("duration", String(durationAmostras), "ms", variaveis);
        variaveis += escreveObjetoJson("voltage_rms", String(GRID_VOLTAGE), "V", variaveis);
    }

    variaveis += "]";

    if (variaveis != "[]")
    {
        client.publish(TOPICO_VARIAVEIS, variaveis.c_str());
        DebugMessage("Enviando mensagem MQTT");
    }
}

void armazenaString(unsigned int offset, String valor, unsigned int tamanhoTotal)
{
    unsigned int tamanhoValorComCaractereFinal = valor.length() + 1;
    for (unsigned int i = 0; i < tamanhoValorComCaractereFinal && i < tamanhoTotal; i++)
        EEPROM.write(offset + i, valor[i]);
}

String obtemString(unsigned int offset, unsigned int tamanhoTotal)
{
    String s = "";
    for (unsigned int i = 0; i < tamanhoTotal; i++)
    {
        char c = EEPROM.read(offset + i);
        if (c == '\0')
            break;
        s += c;
    }

    return s;
}

void salvarDados()
{
    armazenaString(MQTT_CLIENT_ID_START, mqtt_client_id, MQTT_CLIENT_ID_MAX_SIZE);
    armazenaString(MQTT_TOKEN_START, mqtt_token, MQTT_TOKEN_MAX_SIZE);
    armazenaString(WIFI_SSID_START, wifi_ssid, WIFI_SSID_MAX_SIZE);
    armazenaString(WIFI_PASS_START, wifi_pass, WIFI_PASS_MAX_SIZE);

    EEPROM.commit();
}

void carregaDados()
{
    mqtt_client_id = obtemString(MQTT_CLIENT_ID_START, MQTT_CLIENT_ID_MAX_SIZE);
    mqtt_token = obtemString(MQTT_TOKEN_START, MQTT_TOKEN_MAX_SIZE);
    wifi_ssid = obtemString(WIFI_SSID_START, WIFI_SSID_MAX_SIZE);
    wifi_pass = obtemString(WIFI_PASS_START, WIFI_PASS_MAX_SIZE);
}

void handleIndexPost(AsyncWebServerRequest *request)
{
    mqtt_client_id = request->getParam("mqttClientId", true, false)->value();
    mqtt_token = request->getParam("mqttToken", true, false)->value();
    wifi_ssid = request->getParam("wifiSsid", true, false)->value();
    wifi_pass = request->getParam("wifiPass", true, false)->value();

    salvarDados();
    WiFi.disconnect();

    DebugMessage("Salvando dados: " + mqtt_client_id + " / " + mqtt_token + " / " + wifi_ssid + " / " + wifi_pass);

    request->redirect("/");
}

String htmlProcessor(const String &var)
{
    if (var == "mqttClientId")
    {
        return mqtt_client_id;
    }
    else if (var == "mqttToken")
    {
        return mqtt_token;
    }
    else if (var == "wifiSsid")
    {
        return wifi_ssid;
    }
    else if (var == "wifiPass")
    {
        return wifi_pass;
    }
    return String();
}

void setupServer()
{
    IPAddress Ip(192, 168, 1, 1);
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(Ip, Ip, NMask);
    WiFi.softAP(AP_SSID);

    IPAddress IP = WiFi.softAPIP();
    DebugMessage("AP IP address: " + IP.toString());

    espServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                 { request->send_P(200, "text/html", index_html, htmlProcessor); });
    espServer.on("/", HTTP_POST, handleIndexPost);

    espServer.begin();
}

void setupWifi()
{
    if (WiFi.isConnected())
        return;

    if (!wifi_ssid || !wifi_pass)
        return;

    WiFi.begin(wifi_ssid, wifi_pass);

    unsigned int tentativas = 0;
    const unsigned int maxTentativas = 20;
    while (WiFi.status() != WL_CONNECTED && tentativas < maxTentativas)
    {
        delay(500);
        DebugMessage(".", false);
        tentativas++;
    }

    if (WiFi.isConnected())
    {
        timeClient.begin();
        DebugMessage("IP address: " + WiFi.localIP().toString());
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
    }
}

void setupMqtt()
{
    if (client.connected())
        return;

    if (!mqtt_client_id || !mqtt_token)
        return;

    if (WiFi.status() != WL_CONNECTED)
        return;

    client.setServer(MQTT_SERVER, MQTT_PORT);

    if (!client.connected())
    {
        bool connected = client.connect(mqtt_client_id.c_str(), MQTT_USER, mqtt_token.c_str());
        if (!connected)
        {
            char msg[50];
            sprintf(msg, "MQTT - failed, rc= %d. Try again in 5 seconds", client.state());
            DebugMessage(msg);
            delay(5000);
        }
    }
}
void DebugMessage(String message)
{
    DebugMessage(message, true);
}
void DebugMessage(String message, bool skipLine)
{
    if (skipLine)
    {
        Serial.println(message);
    }
    else
    {
        Serial.print(message);
    }
}