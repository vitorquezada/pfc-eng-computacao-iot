#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <WebServeConfig.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// =================== CONSTANTES ===================
const char *AP_SSID = "Arduino_Config";
const int AP_PORT = 80;

const int MQTT_PORT = 1883;
const char *MQTT_SERVER = "mqtt.tago.io";
const char *MQTT_USER = "Token";

const char *TOPICO_ENTRADA = "inTopic";
const char *TOPICO_VARIAVEIS = "variaveis";

const unsigned int MQTT_CLIENT_ID_START = 0;
const unsigned int MQTT_CLIENT_ID_MAX_SIZE = 50;

const unsigned int MQTT_TOKEN_START = MQTT_CLIENT_ID_START + MQTT_CLIENT_ID_MAX_SIZE;
const unsigned int MQTT_TOKEN_MAX_SIZE = 50;

const unsigned int WIFI_SSID_START = MQTT_TOKEN_START + MQTT_TOKEN_MAX_SIZE;
const unsigned int WIFI_SSID_MAX_SIZE = 50;

const unsigned int WIFI_PASS_START = WIFI_SSID_START + WIFI_SSID_MAX_SIZE;
const unsigned int WIFI_PASS_MAX_SIZE = 50;

const unsigned int MAX_SIZE = WIFI_PASS_START + WIFI_PASS_MAX_SIZE;

const unsigned long INTERVALO_COMANDO_DHT = 30000;
const unsigned long INTERVALO_SENSOR_PROXIMIDADE = 500;
const unsigned int DISTANCIA_PROXIMO = 10;

const unsigned long INTERVALO_ONLINE = 60000;

const unsigned long TIMEOUT_COMANDO_ARDUINO = 2000;

const char *FIM_COMANDO = "\r\n";
// =================== CONSTANTES ===================

// =================== VARIÁVEIS ===================
String mqtt_client_id = "";
String mqtt_token = ""; // "1efc7a66-a1a5-431b-9998-636812b5f10a";

String wifi_ssid = ""; //"1602_2G";
String wifi_pass = ""; // "Wifi16021810";

unsigned long ultimo_envio_dht = 0;
unsigned long ultimo_envio_proximidade = 0;
unsigned long ultimo_envio_online = 0;
bool objeto_proximo = false;
// =================== VARIÁVEIS ===================

// =================== COMANDOS ARDUINO ===================
const char *LER_DHT = "LDHT";
const char *ALTERNAR_LED = "ALED";
const char *LER_LED = "LLED";
const char *POSICIONA_SERVO_MOTOR = "PSM";
const char *POSICIONA_MOTOR_PASSO = "PMP";
const char *LER_SENSOR_PROXIMIDADE = "LSP";
const char *CONECTOR_PARAMETROS = "|";
// =================== COMANDOS ARDUINO ===================

// =================== COMANDOS MQTT ===================
const char *MQTT_ALTERNAR_LED = "ALTERNAR_LED";
const char *MQTT_LER_LED = "LER_LED";
const char *MQTT_POSICIONA_SERVO_MOTOR = "POSICIONA_SERVO_MOTOR";
const char *MQTT_POSICIONA_MOTOR_PASSO = "POSICIONA_MOTOR_PASSO";
// =================== COMANDOS MQTT ===================

// ============== Métodos ===================
void setupServer();
void setupWifi();
void setupMqtt();
void sendRecurrentMessages();
void carregaDados();
void DebugMessage(String message);
// ============== Métodos ===================

WiFiClient espClient;
AsyncWebServer espServer(80);
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setup()
{
    EEPROM.begin(MAX_SIZE);
    Serial.begin(9600);
    WiFi.mode(WIFI_AP_STA);

    DebugMessage("Configurando server");
    setupServer();
    DebugMessage("Carregando dados");
    carregaDados();
    DebugMessage("Finalizando setup");
}

void loop()
{
    setupWifi();
    setupMqtt();
    if (WiFi.isConnected() && client.connected())
    {
        client.loop();
        sendRecurrentMessages();
    }
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

String escreveObjetoJson(String Key, bool Value, String variavelExistente)
{
    String valueStr = (Value ? "true" : "false");
    String retorno = "{\"variable\":\"" + Key + "\",\"value\":" + valueStr + ",\"serial\":" + String(getTime()) + "}";
    if (!variavelExistente.endsWith("["))
        retorno = "," + retorno;

    return retorno;
}

String obtemComando(String comando, String variaveis)
{
    if (variaveis != "")
        return comando + CONECTOR_PARAMETROS + variaveis + FIM_COMANDO;
    else
        return comando + FIM_COMANDO;
}

bool EhFinalComando(String comando)
{
    return comando != "" && comando.endsWith(FIM_COMANDO);
}

bool enviaComandoArduino(String comando, String variaveis, String *resposta)
{
    String comandoArduino = obtemComando(comando, variaveis);
    Serial.print(comandoArduino);

    bool finalComandoRecuo = false;
    bool finalComando = false;
    (*resposta) = "";
    unsigned long timeout = millis() + TIMEOUT_COMANDO_ARDUINO;
    while (!finalComando && millis() < timeout)
    {
        while (!finalComando && Serial.available() > 0)
        {
            char charResposta = Serial.read();
            *resposta += charResposta;
            if (finalComandoRecuo)
            {
                if (charResposta == '\n')
                    finalComando = true;
                else
                    finalComandoRecuo = false;
            }

            if (charResposta == '\r')
            {
                finalComandoRecuo = true;
            }
        }
    }

    return finalComando;
}

void sendRecurrentMessages()
{
    unsigned long agora = millis();

    String variaveis = "[";

    if (ultimo_envio_dht + INTERVALO_COMANDO_DHT < agora)
    {
        ultimo_envio_dht = agora;
        String resposta;
        bool sucesso = enviaComandoArduino(LER_DHT, emptyString, &resposta);
        if (sucesso)
        {
            unsigned int endFirstParam = resposta.indexOf(CONECTOR_PARAMETROS);
            String temp = resposta.substring(0, endFirstParam);
            variaveis += escreveObjetoJson("temperatura", temp, "ºC", variaveis);
            String umidade = resposta.substring(endFirstParam + 1, resposta.indexOf(FIM_COMANDO));
            variaveis += escreveObjetoJson("umidade", umidade, "%", variaveis);
        }
    }

    if (ultimo_envio_proximidade + INTERVALO_SENSOR_PROXIMIDADE < agora)
    {
        ultimo_envio_proximidade = agora;
        String resposta;
        bool sucesso = enviaComandoArduino(LER_SENSOR_PROXIMIDADE, emptyString, &resposta);
        if (sucesso)
        {
            String distancia = resposta.substring(0, resposta.indexOf(FIM_COMANDO));
            unsigned int distanciaInt = atoi(distancia.c_str());
            bool estaProximoAgora = distanciaInt <= DISTANCIA_PROXIMO;
            if (objeto_proximo && !estaProximoAgora)
            {
                variaveis += escreveObjetoJson("objetoProximo", false, variaveis);
            }
            else if (!objeto_proximo && estaProximoAgora)
            {
                variaveis += escreveObjetoJson("objetoProximo", true, variaveis);
            }
            objeto_proximo = estaProximoAgora;
        }
    }

    if (ultimo_envio_online + INTERVALO_ONLINE < agora)
    {
        ultimo_envio_online = agora;
        variaveis += escreveObjetoJson("online", true, variaveis);
    }

    variaveis += "]";

    if (variaveis != "[]")
    {
        client.publish(TOPICO_VARIAVEIS, variaveis.c_str());
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    String payloadStr = "";
    for (unsigned int i = 0; i < length; i++)
        payloadStr += (char)payload[i];

    if (strcmp(topic, TOPICO_ENTRADA) != 0)
        return;

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payloadStr);
    if (error.code() != DeserializationError::Ok)
    {
        DebugMessage(error.code() + FIM_COMANDO);
        return;
    }

    String comando = doc["comando"];
    if (comando == MQTT_ALTERNAR_LED)
    {
        bool valor = doc["valor"];
        String resposta;
        String comandoArduino = "";
        if (valor)
            comandoArduino += String(ALTERNAR_LED) + CONECTOR_PARAMETROS + "1";
        else
            comandoArduino += String(ALTERNAR_LED) + CONECTOR_PARAMETROS + "0";
        enviaComandoArduino(comandoArduino, "", &resposta);
    }
    else if (comando == MQTT_LER_LED)
    {
        String resposta;
        bool sucesso = enviaComandoArduino(LER_LED, emptyString, &resposta);
        if (sucesso)
        {
            String comando = "[";
            comando += escreveObjetoJson("led", resposta[0] == '1', comando);
            comando += "]";
            client.publish(TOPICO_VARIAVEIS, comando.c_str());
        }
    }
    else if (comando == MQTT_POSICIONA_SERVO_MOTOR)
    {
        int valor = doc["valor"];

        char valorStr[10];
        sprintf(valorStr, "%d", valor);
        String resposta;
        enviaComandoArduino(POSICIONA_SERVO_MOTOR, valorStr, &resposta);
    }
    else if (comando == MQTT_POSICIONA_MOTOR_PASSO)
    {
        long valor = doc["valor"];

        char valorStr[20];
        sprintf(valorStr, "%d", valor);
        String resposta;
        enviaComandoArduino(POSICIONA_MOTOR_PASSO, valorStr, &resposta);
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
        DebugMessage(".");
        tentativas++;
    }

    if (WiFi.isConnected())
    {
        timeClient.begin();
        DebugMessage("IP address: " + WiFi.localIP().toString());
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
    client.setCallback(callback);

    if (!client.connected())
    {
        DebugMessage("Attempting MQTT connection...");
        if (client.connect(mqtt_client_id.c_str(), MQTT_USER, mqtt_token.c_str()))
        {
            client.subscribe(TOPICO_ENTRADA);
        }
        else
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
    Serial.print("[ED] - " + message + FIM_COMANDO);
}
