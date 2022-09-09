#include <Arduino.h>
#include <Adafruit_Sensor.h>

#include <Servo.h>
#include <math.h>
#include <AccelStepper.h>
#include <Ultrasonic.h>
#include <SoftwareSerial.h>

// ========================= DHT11 ====================
#include <DHT.h>
#define DHT_PIN 9
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);
void IniciaDHT();
void LerDHT(float *temp, float *umidade);
// ========================= DHT11 ====================

// ========================= LED ====================
#define LED_GREEN 8
bool ledGreenState = LOW;
void IniciaLed(int led);
void DefineLed(int led, bool state);
bool LerLed(int led);
// ========================= LED ====================

// ========================= SERVO MOTOR ====================
#define SERVO_PIN 5
Servo servo;
int pos = -1;
void IniciaServoMotor();
void DefinePosicaoServoMotor(int posicao);
// ========================= SERVO MOTOR ====================

// ========================= MOTOR DE PASSO ====================
#define STEPS_PER_REVOLUTION 4096
#define In1 10
#define In2 11
#define In3 12
#define In4 13
AccelStepper stepper(AccelStepper::HALF4WIRE, In1, In3, In2, In4);
void IniciaMotorPasso();
void DefinePosicaoMotorPasso(long posicao);
// ========================= MOTOR DE PASSO ====================

// ========================= SENSOR PROXIMIDADE ====================
#define TRIGGER_PIN 7
#define ECHO_PIN 6
Ultrasonic ultrasonic(TRIGGER_PIN, ECHO_PIN);
unsigned int LerDistancia();
// ========================= SENSOR PROXIMIDADE ====================

// ========================= ESP 8266 ====================
#define ESP_ARDUINO_RX 2
#define ESP_ARDUINO_TX 3
SoftwareSerial esp8266(ESP_ARDUINO_RX, ESP_ARDUINO_TX);
//#define esp8266 Serial
const unsigned long READ_ESP_COMMAND_TIMEOUT = 2000;
// ========================= ESP 8266 ====================

// =================== COMANDOS ===================
const char *LER_DHT = "LDHT";
const char *ALTERNAR_LED = "ALED";
const char *LER_LED = "LLED";
const char *POSICIONA_SERVO_MOTOR = "PSM";
const char *POSICIONA_MOTOR_PASSO = "PMP";
const char *LER_SENSOR_PROXIMIDADE = "LSP";
// =================== COMANDOS ===================

const char *FIM_COMANDO = "\r\n";

void InterpretaComando();

void setup()
{
  Serial.begin(9600);
  esp8266.begin(9600);

  IniciaLed(LED_GREEN);
  IniciaDHT();
  IniciaServoMotor();
  IniciaMotorPasso();
}

void loop()
{
  if (stepper.distanceToGo() != 0)
    stepper.run();

  InterpretaComando();
}

bool EhFinalComando(String comando)
{
  return comando != "" && comando.endsWith(FIM_COMANDO);
}

bool LeComando(String *comando, unsigned long timeout)
{
  bool finalComando = false;
  *comando = "";
  unsigned long millisTimeout = millis() + timeout;
  while (!finalComando && millis() < millisTimeout)
  {
    while (!finalComando && esp8266.available())
    {
      char charComando = esp8266.read();
      Serial.print(charComando);
      *comando += charComando;
      finalComando = EhFinalComando(*comando);
    }
  }

  return finalComando;
}

void EnviaComandoESP(String comando)
{
  esp8266.print(comando);
  Serial.println("[UD] - " + comando);
  if (!comando.endsWith(FIM_COMANDO))
  {
    esp8266.print(FIM_COMANDO);
    Serial.println("[UD] - FIM_COMANDO");
  }
}

void InterpretaComando()
{
  if (!esp8266.available())
    return;

  String comando = "";
  bool comandoLido = LeComando(&comando, READ_ESP_COMMAND_TIMEOUT);
  if (!comandoLido)
  {
    Serial.println("[UD] - Comando não lido: [" + comando + "]");
    return;
  }

  if (comando.startsWith(LER_DHT))
  {
    float temp, umidade;
    LerDHT(&temp, &umidade);

    String res = String(temp) + "|" + String(umidade);
    EnviaComandoESP(res);
  }
  else if (comando.startsWith(ALTERNAR_LED))
  {
    String opcao = comando.substring(strlen(ALTERNAR_LED) + 1, comando.indexOf(FIM_COMANDO));
    DefineLed(LED_GREEN, opcao == "1");
    EnviaComandoESP("");
  }
  else if (comando.startsWith(LER_LED))
  {
    bool led = LerLed(LED_GREEN);
    EnviaComandoESP(led ? "1" : "0");
  }
  else if (comando.startsWith(POSICIONA_SERVO_MOTOR))
  {
    String opcao = comando.substring(strlen(POSICIONA_SERVO_MOTOR) + 1, comando.indexOf(FIM_COMANDO));
    if (opcao != "")
    {
      int posicao = opcao.toInt();
      DefinePosicaoServoMotor(posicao);
      EnviaComandoESP("0");
    }
    else
    {
      EnviaComandoESP("1");
    }
  }
  else if (comando.startsWith(POSICIONA_MOTOR_PASSO))
  {
    String opcao = comando.substring(strlen(POSICIONA_MOTOR_PASSO) + 1, comando.indexOf(FIM_COMANDO));
    if (opcao != "")
    {
      int posicao = opcao.toInt();
      DefinePosicaoMotorPasso(posicao);
      EnviaComandoESP("0");
    }
    else
    {
      EnviaComandoESP("1");
    }
  }
  else if (comando.startsWith(LER_SENSOR_PROXIMIDADE))
  {
    unsigned int distancia = LerDistancia();
    EnviaComandoESP(String(distancia) + FIM_COMANDO);
  }
  else if (comando.startsWith("[ED] - "))
  {
    Serial.println(comando);
  }
  else
  {
    Serial.println("[UD] - Nenhum comando interpretado: [" + comando + "]");
  }
}

void IniciaDHT()
{
  dht.begin();
}

void LerDHT(float *temp, float *umidade)
{
  float h = dht.readHumidity();
  if (isnan(h))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
  }
  else
  {
    *umidade = h;
  }

  float t = dht.readTemperature();
  if (isnan(t))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
  }
  else
  {
    *temp = t;
  }
}

void IniciaLed(int led)
{
  pinMode(led, OUTPUT);
}

bool LerLed(int led)
{
  return digitalRead(led);
}

void DefineLed(int led, bool state)
{
  digitalWrite(led, state);
}

void IniciaServoMotor()
{
  servo.attach(SERVO_PIN);
}

void DefinePosicaoServoMotor(int posicao)
{
  servo.write(posicao < 10 ? 10 : posicao);
}

void IniciaMotorPasso()
{
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(200);
}

void DefinePosicaoMotorPasso(long posicao)
{
  stepper.stop();
  stepper.moveTo(posicao);
}

unsigned int LerDistancia()
{
  return ultrasonic.read(CM);
}
