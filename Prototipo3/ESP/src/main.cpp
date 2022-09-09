#include <ESP8266WiFi.h>
#include <math.h>

// ------------------ ACS712 - Medidor de corrente ------------------
const unsigned int PIN_CURRENT = A0;
const unsigned int PIN_CURRENT_RMS_OUT = D1;

const unsigned int TOTAL_AMOSTRAS = 5000;
const unsigned int AMOSTRAS_OFFSET = 1024;
const double ESP_VOLTAGE = 3.234f;
const int ANALOG_BIT_RESOLUTION = 10;
const double ANALOG_BIT_RESOLUTION_VALUE = 1024.0;
const double RESOLUTION_ESP = ESP_VOLTAGE / ANALOG_BIT_RESOLUTION_VALUE;

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
    
    WiFi.mode(WIFI_OFF);
}

void loop()
{
    acumulateCurrentRms();

    sendRecurrentMessagesACS712();

    if (enviarCorrente())
    {
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);
        amostras = 0;
        accumulateADC = 0.0;
    }
}

bool enviarCorrente()
{
    return amostras >= TOTAL_AMOSTRAS;
}

void acumulateCurrentRms()
{
    int sampleI = analogRead(PIN_CURRENT);

    offsetADC += (sampleI - offsetADC) / AMOSTRAS_OFFSET;
    // DebugMessage("offsetADC: " + String(offsetADC, 2));
    accumulateADC += sq(sampleI - offsetADC);
    amostras++;
}

double getCurrent1Steps(double current){
    return current / 0.208775116;
}

double getCurrent2Steps(double current){
    if(current < 0.078688525)
        return current / 0.221387556;
    else
        return current / 0.19752228;
}

double getCurrent3Steps(double current){
    if(current < 0.052327869)
        return current / 0.229247929;
    else if(current < 0.104655738)
        return current / 0.202790489;
    else
        return current / 0.197010468; 
}

double getCurrentRms()
{
    double current = sqrt(accumulateADC / amostras) * (RESOLUTION_ESP);
    // return getCurrent1Steps(current);
    // return getCurrent2Steps(current);
    return getCurrent3Steps(current);    
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