#include <ESP8266WiFi.h>

#define TotalSamples 800
float calibration();
int runcount = 0;
unsigned long sampleTime[TotalSamples] = {};
int sensorValue[TotalSamples]={};

const int pin = A0;

float zero = 0;

void setup() {
  
  Serial.begin(38400);
  
  zero = calibration();

  analogWriteResolution(10); //10 bits
  analogWriteFreq(10000); // 100 a 40000Hz
  pinMode(LED_BUILTIN, OUTPUT);
}

float calibration() 
{
  Serial.println("---------------");
  Serial.print("Calibrando...\nOffset:");
  float acc = 0;
  for (int i = 0; i < 1000; i++) {
    acc += analogRead(pin);
  }
  float zero = acc / 1000;
  zero = zero * ( 3.3 / 1024.0);
  Serial.print(zero);
  Serial.println("V");
  Serial.println("Pronto!");
  Serial.println("---------------");
  return zero;
  
}

float getcurrent()
{
  float sum = 0;
  float voltage = 0;
  float current = 0;
  
  for (int i = 0; i < TotalSamples; i++)
    {
      voltage = sensorValue[i] * (3.3 / 1024.0) - zero;
      current = voltage / 0.185;
      sum += sq(current);
    }
  current = sqrt(sum / TotalSamples);
  return (current);
}

void loop() {
 
  while (runcount < TotalSamples)
  {
    sensorValue[runcount]=analogRead(A0);
    runcount++;
  }
    
    float currentRMS = getcurrent();

    digitalWrite(LED_BUILTIN,LOW);
    analogWrite(D1,(currentRMS * 3) * 1024 / 3.3); // Corrente RMS x3 para abranger mais da
                                                  //  faixa de saída já que a resistência chega
                                                  // apenas a 1A
    
    delay(500);
    digitalWrite(LED_BUILTIN,HIGH);
    runcount = 0;
    zero = calibration();
}
