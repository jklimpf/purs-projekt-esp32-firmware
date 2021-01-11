#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "DHT.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>

#define DHTPIN 4
#define DHTPIN2 17
#define DHTTYPE DHT11
#define BUZZER_PIN 32
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define GAS_ANALOG 33
#define GAS_ANALOG2 35
#define BUTTON_PIN 25

int channel = 0, resolution = 8, freq = 100;

int buttonState;
int lastButtonState = LOW;
int mode = 1;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

float t, t2;
int gassensorAnalog, gassensorAnalog2;
float gassensorAnalogArit = 0, gassensorAnalogArit2 = 0;
int i = 0, j = 0;

DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

char ssid[] = ""; // Change this to your network SSID (name).
char pass[] = ""; // Change this to your network password.
const char *server = "mqtt.thingspeak.com";
char mqttUserName[] = ""; // Use any name.
char mqttPass[] = "";     // Change to your MQTT API key from Account > MyProfile.
long writeChannelID = 0;
char writeAPIKey[] = "";

WiFiClient client;               // Initialize the Wi-Fi client library.
PubSubClient mqttClient(client); // Initialize the PuBSubClient library.

int fieldsToPublish[4] = {1, 1, 1, 1}; // Change to allow multiple fields.
float dataToPublish[4];                // Holds your field data.

unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 16000;

void spremanjeMjerenja();

// Generate a unique client ID and connect to MQTT broker.
void mqttConnect();

// Publish messages to a channel feed.
void mqttPublish(long pubChannelID, char *pubWriteAPIKey, float dataArray[], int fieldArray[]);

// Connect to a given Wi-Fi SSID
void connectWifi();

// Build a random client ID for MQTT connection.
void getID(char clientID[], int idLength);

void buzzerConditions();

void oled();

void oled2();

void setup()
{

    Serial.begin(9600);
    Serial.println("Start");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }
    delay(2000);
    display.clearDisplay();
    display.setTextColor(WHITE);
    pinMode(BUTTON_PIN, INPUT);

    dht.begin();
    dht2.begin();
    connectWifi();                      // Connect to Wi-Fi network.
    mqttClient.setServer(server, 1883); // Set the MQTT broker details.
}

void loop()
{

    if (WiFi.status() != WL_CONNECTED)
    {
        connectWifi();
    }

    if (!mqttClient.connected())
    {
        mqttConnect(); // Connect if MQTT client is not connected.
    }

    mqttClient.loop(); // Call the loop to maintain connection to the server.

    int reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState)
    {
        // reset the debouncing timer
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay)
    {

        if (reading != buttonState)
        {
            buttonState = reading;

            if (buttonState == HIGH)
            { //Change room on oled
                mode = ++mode % 2;
            }
        }
    }
    switch (mode + 1)
    {
    case 1:
        oled();
        break;

    case 2:
        oled2();
        break;
    }

    lastButtonState = reading;
    if (millis() - lastConnectionTime > postingInterval)
    {
        //dataToPublish[0]=random(100);

        spremanjeMjerenja();
        delay(1100); // Wait for ThingSpeak to publish.
        mqttPublish(writeChannelID, writeAPIKey, dataToPublish, fieldsToPublish);
    }
}

/** 
 * Publish to a channel 
 *   pubChannelID - Channel to publish to. 
 *   pubWriteAPIKey - Write API key for the channel to publish to. 
 *   dataArray - Binary array indicating which fields to publish to, starting with field 1. 
 *   fieldArray - Array of values to publish, starting with field 1. 
 */
void mqttPublish(long pubChannelID, char *pubWriteAPIKey, float dataArray[], int fieldArray[])
{
    int index = 0;
    String dataString = "";
    while (index < 4)
    {
        if (fieldArray[index] > 0)
        {
            dataString += "&field" + String(index + 1) + "=" + String(dataArray[index]);
        }
        index++;
    }

    Serial.println(dataString);

    String topicString = "channels/" + String(pubChannelID) + "/publish/" + String(pubWriteAPIKey);
    mqttClient.publish(topicString.c_str(), dataString.c_str());
    Serial.println("Published to channel " + String(pubChannelID));
    lastConnectionTime = millis();
}

void connectWifi()
{
    while (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin(ssid, pass);
        delay(2500);
        Serial.println("Connecting to Wi-Fi");
    }
    Serial.println("Connected");
}

void mqttConnect()
{
    char clientID[9];

    // Loop until connected.
    while (!mqttClient.connected())
    {

        getID(clientID, 8);

        // Connect to the MQTT broker.
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(clientID, mqttUserName, mqttPass))
        {
            Serial.println("Connected with Client ID:  " + String(clientID) + " User " + String(mqttUserName) + " Pwd " + String(mqttPass));
        }
        else
        {
            Serial.print("failed, rc = ");
            // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation. ;
            Serial.print(mqttClient.state());
            Serial.println(" Will try again in 5 seconds");
            delay(5000);
        }
    }
}

/** 
 * Build a random client ID. 
 *   clientID - Character array for output 
 *   idLength - Length of clientID (actual length is one character longer for NULL) 
 */
void getID(char clientID[], int idLength)
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz"; // For random generation of the client ID.

    // Generate client ID.
    for (int i = 0; i < idLength; i++)
    {
        clientID[i] = alphanum[random(51)];
    }
    clientID[idLength] = '\0';
}

void spremanjeMjerenja()
{
    t = dht.readTemperature();
    t2 = dht2.readTemperature();
    gassensorAnalog = analogRead(GAS_ANALOG);
    gassensorAnalog2 = analogRead(GAS_ANALOG2);
    gassensorAnalogArit = 0;
    gassensorAnalogArit2 = 0;
    i = 0;
    j = 0;
    gassensorAnalog = 0;
    gassensorAnalog2 = 0;

    for (i = 0; i < 4000; i++) // Dobivanje aritmeticke sredine 4000 uzoraka za sto tocnije mjerenje koncentracije plina
    {
        gassensorAnalog += analogRead(GAS_ANALOG);
    };

    gassensorAnalogArit = gassensorAnalog / 4000;

    for (j = 0; j < 4000; j++)
    {
        gassensorAnalog2 += analogRead(GAS_ANALOG2);
    };

    gassensorAnalogArit2 = gassensorAnalog2 / 4000;

    dataToPublish[0] = t;
    dataToPublish[1] = t2;
    dataToPublish[2] = gassensorAnalogArit;
    dataToPublish[3] = gassensorAnalogArit2;

    buzzerConditions();

    Serial.print(F("  Temperature dht: "));
    Serial.print(t);
    Serial.print(F("  Temperature dht2: "));
    Serial.print(t2);
    Serial.print(F("   Gas Sensor: "));
    Serial.print(gassensorAnalogArit);
    Serial.print("     ");
    Serial.print(F("   Gas Sensor2: "));
    Serial.print(gassensorAnalogArit2);
    Serial.print("     ");
}

void buzzerConditions()
{
    if (t > 35 || gassensorAnalogArit > 400 || t2 > 35 || gassensorAnalogArit2 > 400)
    {
        ledcSetup(channel, freq, resolution);
        ledcAttachPin(BUZZER_PIN, channel);
        ledcWriteTone(channel, 50);
    }

    else
        ledcDetachPin(BUZZER_PIN);
}

void oled()
{
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Soba 1:");
    display.setTextSize(1);
    display.setCursor(0, 25);
    display.print("Temperatura: ");
    display.setCursor(75, 25);
    display.print(t);
    display.setCursor(111, 25);
    display.cp437(true);
    display.write(167);
    display.print("C");
    display.setCursor(0, 40);
    display.print("Plin: ");
    display.setCursor(30, 40);
    display.print(gassensorAnalogArit);
    display.display();
}

void oled2()
{
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("Soba 2:");
    display.setTextSize(1);
    display.setCursor(0, 25);
    display.print("Temperatura: ");
    display.setCursor(75, 25);
    display.print(t2);
    display.setCursor(111, 25);
    display.cp437(true);
    display.write(167);
    display.print("C");
    display.setCursor(0, 40);
    display.print("Plin: ");
    display.setCursor(30, 40);
    display.print(gassensorAnalogArit2);
    display.display();
}