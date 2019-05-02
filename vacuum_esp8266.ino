#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>

const char *ssid = "WIFI Access Point";
const char *password = "wifi password";
const char *mqtt_server = "IP of MQTT server";
const char *device_id = "DirtDevil";

WiFiClient espClient;
PubSubClient client(espClient);

//PINS
const byte PWR_LED_PIN = D0;
const byte BUMPER_PIN = D1;
const byte BUZZER_PIN = D2;
const byte VACUUM_PIN = D5;
const byte LOW_BATT_LED_PIN = D6;
const byte LEFT_MOTOR_FORWARD_PIN = D7;
const byte LEFT_MOTOR_REVERSE_PIN = D8;
const byte RIGHT_MOTOR_FORWARD_PIN = D3;
const byte RIGHT_MOTOR_REVERSE_PIN = D4;
const byte VCC_PIN = A0;

enum Directions {FORWARD, REVERSE, LEFT, RIGHT, STOP};
enum Features {turn_on, turn_off, locate, start_pause, stop, clean_spot, return_to_base};

Features Status = turn_off;

char message_buff[100];

bool cleaning = false;
bool docked = false;
bool charging = false;
String error = "Ready";

float vout = 0.0;
float vin = 0.0;
int value = 0;
int lmotor = 1024;
int rmotor = 1024;
bool cleaningspot = false;

long lastMsg = 0;
char msg[256];

void callback(char *led_control, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(led_control);
  Serial.println("] ");

  int i;
  for (i = 0; i < length; i++)
  {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';

  String msgString = String(message_buff);
  Serial.println(msgString);
  if (strcmp(led_control, "vacuum/command") == 0)
  {
    if (msgString == "turn_off") {
      Status = turn_off;
    }

    if (msgString == "turn_on") {
      Status = turn_on;
    }

    if (msgString == "locate") {
      Status = locate;
    }

    if (msgString == "start_pause") {
      Status = start_pause;
      if (cleaning == true)
        Status = turn_off;
      else
        Status = turn_on;
    }

    if (msgString == "stop") {
      Status = stop;
    }

    if (msgString == "clean_spot") {
      Status = clean_spot;
    }

    if (msgString == "return_to_base") {
      Status = return_to_base;
    }

  }
}


void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(device_id, "user", "password"))
    {
      Serial.println("connected");
      publishData();
      digitalWrite(LOW_BATT_LED_PIN, LOW);
      digitalWrite(PWR_LED_PIN, HIGH);
      client.subscribe("vacuum/command"); // write your unique ID here
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      digitalWrite(PWR_LED_PIN, LOW);
      digitalWrite(LOW_BATT_LED_PIN, HIGH);
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  client.setServer(mqtt_server, 1883); // change port number 
  client.setCallback(callback);

  //Set Outputs
  pinMode(PWR_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VACUUM_PIN, OUTPUT);
  pinMode(LOW_BATT_LED_PIN, OUTPUT);
  pinMode(LEFT_MOTOR_FORWARD_PIN, OUTPUT);
  pinMode(LEFT_MOTOR_REVERSE_PIN, OUTPUT);
  pinMode(RIGHT_MOTOR_FORWARD_PIN, OUTPUT);
  pinMode(RIGHT_MOTOR_REVERSE_PIN, OUTPUT);

  //Set Inputs
  pinMode(VCC_PIN , INPUT);
  pinMode(BUMPER_PIN, INPUT);

  //Initialise Pins
  digitalWrite(PWR_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(VACUUM_PIN, LOW);
  digitalWrite(LOW_BATT_LED_PIN, LOW);
  digitalWrite(LEFT_MOTOR_FORWARD_PIN, LOW);
  digitalWrite(RIGHT_MOTOR_FORWARD_PIN, LOW);
  digitalWrite(RIGHT_MOTOR_REVERSE_PIN, LOW);

}

void move_robot(Directions direction)
{
  switch (direction) {
    case FORWARD:
      analogWrite(LEFT_MOTOR_FORWARD_PIN, lmotor);
      analogWrite(LEFT_MOTOR_REVERSE_PIN, LOW);
      analogWrite(RIGHT_MOTOR_FORWARD_PIN, rmotor);
      analogWrite(RIGHT_MOTOR_REVERSE_PIN, LOW);
      break;

    case REVERSE:
      digitalWrite(LEFT_MOTOR_FORWARD_PIN, LOW);
      digitalWrite(LEFT_MOTOR_REVERSE_PIN, HIGH);
      digitalWrite(RIGHT_MOTOR_FORWARD_PIN, LOW);
      digitalWrite(RIGHT_MOTOR_REVERSE_PIN, HIGH);
      break;
    case RIGHT:
      digitalWrite(LEFT_MOTOR_FORWARD_PIN, LOW);
      digitalWrite(LEFT_MOTOR_REVERSE_PIN, HIGH);
      digitalWrite(RIGHT_MOTOR_FORWARD_PIN, HIGH);
      digitalWrite(RIGHT_MOTOR_REVERSE_PIN, LOW);
      break;
    case LEFT:
      digitalWrite(LEFT_MOTOR_FORWARD_PIN, HIGH);
      digitalWrite(LEFT_MOTOR_REVERSE_PIN, LOW);
      digitalWrite(RIGHT_MOTOR_FORWARD_PIN, LOW);
      digitalWrite(RIGHT_MOTOR_REVERSE_PIN, HIGH);
      break;
    case STOP:
      digitalWrite(LEFT_MOTOR_FORWARD_PIN, LOW);
      digitalWrite(LEFT_MOTOR_REVERSE_PIN, LOW);
      digitalWrite(RIGHT_MOTOR_FORWARD_PIN, LOW);
      digitalWrite(RIGHT_MOTOR_REVERSE_PIN, LOW);
      break;
  }
}

void publishData()
{
  const size_t capacity = JSON_ARRAY_SIZE(6) + JSON_OBJECT_SIZE(7);
  DynamicJsonDocument doc(capacity);

  int value = analogRead(VCC_PIN);
  vin = map(value, 616, 942, 0, 100);

  doc["battery_level"] = vin;
  doc["docked"] = docked;
  doc["cleaning"] = cleaning;
  doc["charging"] = charging;
  doc["error"] = error;

  serializeJson(doc, msg);
  //serializeJson(doc, Serial);
  Serial.println(String(value) + ":" + String(vin));
  client.publish("vacuum/state", msg, true);
}

void Buzzer()
{
  tone(BUZZER_PIN, 1000);
  delay(1000);
  noTone(BUZZER_PIN);
  delay(1000);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  switch (Status)
  {
    case turn_on:
      cleaning = true;
      digitalWrite(PWR_LED_PIN, HIGH);
      digitalWrite(VACUUM_PIN, HIGH);

      if (digitalRead(BUMPER_PIN) == LOW)
      {
        move_robot(REVERSE);
        delay(250);
        move_robot(RIGHT);
        delay(random(500));
      }
      else
      {
        move_robot(FORWARD);
      }
      break;

    case turn_off:
      cleaning = false;
      digitalWrite(PWR_LED_PIN, LOW);
      move_robot(STOP);
      digitalWrite(VACUUM_PIN, LOW);
      break;

    case locate:
      Buzzer();
      break;

    case start_pause:
      if (cleaning) {
        cleaning = false;
        Status = turn_off;
      }
      else
      {
        cleaning = true;
        Status = turn_on;
      }
      break;

    case stop:
      cleaning = false;
      Status = turn_off;
      break;

    case clean_spot:
      cleaning = true;
      if (cleaningspot)
      {
        rmotor = 1024;
        cleaningspot=false;
        Serial.println("clean spot off");
      }
      else
      {
        rmotor = 512;
        cleaningspot=true;
        Serial.println("clean spot on");
      }
      Status = turn_on;
      break;

    case return_to_base:
      break;
  }

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    publishData();

    //if voltage reaches 14V switch off and play SOS beeps
    int value = analogRead(VCC_PIN);
    if (value < 617 && Status != turn_off)
    {
      cleaning = false;
      digitalWrite(PWR_LED_PIN, LOW);
      move_robot(STOP);
      digitalWrite(VACUUM_PIN, LOW);
      Status = turn_off;
      error = "Low Battery";

      int i;
      int pause;
      for (i = 0; i < 5; i++)
      {
        int x;
        for (x = 1; x < 10; x++)
        {
          if (x > 3 && x < 7)
            pause = 100;
          else
            pause = 50;

          tone(BUZZER_PIN, 1000);
          delay(pause);
          noTone(BUZZER_PIN);

          if (x == 3 || x == 6)
            pause = 100;

          delay(pause);
        }
        delay(2000);
      }
    }
  }
}
