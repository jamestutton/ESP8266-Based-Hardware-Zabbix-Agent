//******************************************************************************
//* Purpose : Zabbix Sensor Agent - Environmental Monitoring Solution *
//* Git : https://github.com/interlegis/arduino-zabbix-agent
//* Author :  Gabriel Ferreira and Marco Rougeth *
//* https://github.com/gabrielrf and
//* https://github.com/rougeth
//* Adapted from : Evgeny Levkov and Schotte Vincent *
//* Credits: *

#include <SPI.h>
#include <string.h>
#include <ESP8266WiFi.h>

#include <OneWire.h>
#include <DallasTemperature.h>

void ReadSensors();
void DiscoverSensors();

int pirPin = 4;    //the digital pin connected to the PIR sensor's output
int relayPin = 5;

int Light = 0;
boolean activeMotion = 0;
unsigned long lastMotion = 0;
boolean newMotion = 0;
int MotionTimeout = 15000;


#define ONE_WIRE_BUS 2  // One wire pin with a 4.7k resistor

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


WiFiServer server(10050);
WiFiClient client;
const char* ssid = "SSID";
const char* password = "APPASSWORD";
const String hostname = "HOSTNAME";

typedef struct {
   float temp;
   String ref;
   char* desc;
   byte addr[8];
   boolean crc = 0;
} TempSensor;



const int MAX_DEVICES = 10;
int numberOfDevices;
int waitTime = 15000;           // Default waiting time before reading sensor again.
unsigned long oneWireLastCheck = 0;

TempSensor readings[MAX_DEVICES];

void DiscoverSensors(){

    numberOfDevices = sensors.getDeviceCount();


    Serial.print("Found ");
    Serial.print(numberOfDevices);
    Serial.println(" sensors: ");

    byte ia;
    for(int i=0;i<numberOfDevices; i++) {
      readings[i].ref = "";
      if (sensors.getAddress(readings[i].addr, i)) {
        for( ia = 0; ia < 8; ia++) {
          if (readings[i].addr[ia] < 16) {
            readings[i].ref += '0';
          }
          readings[i].ref += String(readings[i].addr[ia], HEX);
        }
        // a check to make sure that what we read is correct.
        if ( OneWire::crc8( readings[i].addr, 7) != readings[i].addr[7]) {
          readings[i].crc = 0;
        }
      }
      Serial.println("temp.read["+ readings[i].ref + "]");
    }
    oneWireLastCheck = 0;
    ReadSensors();

}

void checkMotion() {
    if(digitalRead(pirPin) == HIGH){
      Serial.println("Motion");
      activeMotion = 1;
      newMotion = 1;
      lastMotion = millis();
    }else if(digitalRead(pirPin) == LOW){
      activeMotion = 0;
    }

}



void ReadSensors() {
  if (millis() - oneWireLastCheck > waitTime) {

    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    sensors.requestTemperatures(); // Send the command to get temperatures

    int p = 0;
    for(int i=0;i<numberOfDevices; i++) {
        float t = sensors.getTempCByIndex(i);
        if (t >= -55 && t <= 125) {
            readings[i].temp = t;
        }
    }
    oneWireLastCheck = millis();

  }
}



//Commands received by agent on port 10050 parsing
void parseCommand(String cmd) {

  if (cmd.equals("")) {

  }else {

      Serial.print(" Time: ");
      Serial.print(millis() / 1000);
      Serial.print("\t");
      Serial.print("Cmd: ");
      Serial.print(cmd);
      Serial.print("\t");
      Serial.print("Response: ");
      // AGENT.PING
      if(cmd.equals("agent.ping")) {
          client.println("1");
          Serial.println("1");
        // AGENT.VERSION
       } else if(cmd.equals("agent.version")) {
          client.println("Sensor.IM 1.0");
          Serial.println("Sensor.IM 1.0");
      } else if(cmd.equals("agent.uptime")) {
          client.println(millis() / 1000);
          Serial.println(millis() / 1000);
      } else if(cmd.equals("agent.hostname")) {
          client.println(hostname);
          Serial.println(hostname);
      // TEMP READ
      //temp.read[2810b4c50600007c]
      //123456789012345678901234567
      } else if(cmd.startsWith("temp.read")) {
          if (cmd.length() == 27) {
            String uid = cmd.substring(10,26);
            boolean found = 0;
            for(int i=0;i<numberOfDevices; i++) {
              if(readings[i].ref == uid) {
                client.println(readings[i].temp);
                Serial.println(readings[i].temp);
                found = 1;
              }
            }
            if (!found) {
              client.println("UNKNOWN SENSOR:" + uid);
              Serial.println("UNKNOWN SENSOR:" + uid);
            }
          } else {
              client.println("ZBXDZBX_NOTSUPPORTED");
              Serial.println("ZBXDZBX_NOTSUPPORTED");
          }
      // SENSOR LIST
      } else if(cmd.equals("sensors.count")) {
          client.println(numberOfDevices);
          Serial.println(numberOfDevices);
      } else if(cmd.equals("sensors.serials")) {
        DiscoverSensors();
        for(int i=0;i<numberOfDevices; i++) {
          client.println(readings[i].ref);
          Serial.println(readings[i].ref);
        }
      } else if(cmd.equals("sensors.temps")) {
        DiscoverSensors();
        for(int i=0;i<numberOfDevices; i++) {
          client.println(readings[i].temp);
          Serial.println(readings[i].temp);
        }
      }else if(cmd.equals("light.on")) {
        Light = 2;
      }else if(cmd.equals("light.off")) {
        Light = -1;
      }else if(cmd.equals("light.auto")) {
        Light = 0;
      }else if(cmd.equals("light.state")) {
        client.println(Light);
        Serial.println(Light);
      }else if(cmd.equals("motion.now")) {
        client.println(activeMotion);
        Serial.println(activeMotion);
      }else if(cmd.equals("motion.new")) {
          client.println(newMotion);
          Serial.println(newMotion);
          newMotion = 0;
      }else if(cmd.equals("motion.age")) {
          int diff = (millis() - lastMotion) / 1000;
          client.println(diff);
          Serial.println(diff);
          newMotion = 0;
      // NOT SUPPORTED
      } else {
        //server.println("ZBXDZBX_NOTSUPPORTED");
        client.print("UNKNOWN CMD:");
        client.println(cmd);

      }
      cmd = "";


  }
  client.flush();
  client.stop();

}







void loop() {
  ReadSensors();
  checkMotion();


  //-1 Hard Off
  //+2 Hard On
  if (Light<=1 && Light >=0) {
      if (millis() - lastMotion < MotionTimeout) {
        Light=1;
      } else {
        Light=0;
      }
  }
  if (Light>0) {
      digitalWrite(relayPin, LOW);
  }else{
    digitalWrite(relayPin, HIGH);
  }

  // Check if a client has connected
  client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available()){
    delay(1);
  }

  // Read the first line of the request
  String cmd = client.readStringUntil('\r');
  cmd.replace("\n", "");
  cmd.replace("\r", "");
  Serial.println(cmd);
  parseCommand(cmd);
  client.flush();




}



// This function runs once only, when the Arduino is turned on or reseted.
void setup() {

  Serial.begin(115200);

  pinMode(pirPin, INPUT);
  pinMode(relayPin, OUTPUT);

  digitalWrite(pirPin, LOW);
  digitalWrite(relayPin, HIGH);


  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  server.setNoDelay(true);
  Serial.println("Looking for sensors...");
   // Start up the library
   sensors.begin();
   delay(6000);

  DiscoverSensors();

  Serial.println("Starting Zabbix Agent...");


  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 10050' to connect");


}
