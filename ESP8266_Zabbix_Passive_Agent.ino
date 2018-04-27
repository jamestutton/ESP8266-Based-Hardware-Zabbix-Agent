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




#define ONE_WIRE_BUS 2  // One wire pin with a 4.7k resistor

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


WiFiServer AgentServer(10050);


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
void parseCommand(String cmd,WiFiClient serverClient) {

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
          serverClient.println("1");
          Serial.println("1");
        // AGENT.VERSION
       } else if(cmd.equals("agent.version")) {
          serverClient.println("Sensor.IM 1.0");
          Serial.println("Sensor.IM 1.0");
      } else if(cmd.equals("agent.uptime")) {
          serverClient.println(millis() / 1000);
          Serial.println(millis() / 1000);
      } else if(cmd.equals("agent.hostname")) {
          serverClient.println(hostname);
          Serial.println(hostname);
      // TEMP READ
      //temp.sensors.read[2810b4c50600007c]
      //12345678901234567890123456789012345
      } else if(cmd.startsWith("temp.sensors.read")) {
          if (cmd.length() == 17+16+2) {
            String uid = cmd.substring(18,34);
            boolean found = 0;
            for(int i=0;i<numberOfDevices; i++) {
              if(readings[i].ref == uid) {
                serverClient.println(readings[i].temp);
                Serial.println(readings[i].temp);
                found = 1;
              }
            }
            if (!found) {
              serverClient.println("UNKNOWN SENSOR:" + uid);
              Serial.println("UNKNOWN SENSOR:" + uid);
            }
          } else {
              serverClient.println("ZBXDZBX_NOTSUPPORTED");
              Serial.println("ZBXDZBX_NOTSUPPORTED");
          }
      // SENSOR LIST
      } else if(cmd.equals("temp.sensors.count")) {
          serverClient.println(numberOfDevices);
          Serial.println(numberOfDevices);
      } else if(cmd.equals("temp.sensors.discover")) {
        DiscoverSensors();
        String sbuff ="";
        sbuff += "{";
        sbuff += "\"data\":[";
        for(int i=0;i<numberOfDevices; i++) {
          sbuff += "{";
          sbuff += "\"{#SERIAL}\":\"";
          sbuff += readings[i].ref;
          sbuff += "\"";
          sbuff += "},";
        }
        int len = sbuff.length() - 1;
        sbuff.remove(len);
        sbuff += "]}";
        serverClient.println(sbuff);
        Serial.println(sbuff);

        
      } else if(cmd.equals("temp.sensors.temps")) {
        DiscoverSensors();
        String sbuff ="";
        
        for(int i=0;i<numberOfDevices; i++) {
          sbuff += readings[i].ref;
          sbuff.concat(readings[i].temp);
          sbuff +=  "\n";
        }
        serverClient.print(sbuff);
        Serial.print(sbuff);
      // NOT SUPPORTED
      } else {
        //AgentServer.println("ZBXDZBX_NOTSUPPORTED");
        serverClient.print("UNKNOWN CMD:");
        serverClient.println(cmd);

      }
      cmd = "";


  }


}







void loop() {
  ReadSensors();


  //Must be declared Inside Loop as gets destroyed
  WiFiClient serverClient;


  // look for Client connect trial
  if (AgentServer.hasClient()) {
    if (!serverClient || !serverClient.connected()) {
      if (serverClient) {
        serverClient.stop();
        Serial.println("Client Stop");
      }
      serverClient = AgentServer.available();
      Serial.println("New client");
      //serverClient.flush();  // clear input buffer, else you get strange characters 
    }
  }

  
  if (!serverClient) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  while(!serverClient.available()){
    delay(1);
  }

  // Read the first line of the request
  String cmd = serverClient.readStringUntil('\n');
  cmd.replace("\n", "");
  cmd.replace("\r", "");
  Serial.println(cmd);
  parseCommand(cmd,serverClient);
  serverClient.flush();
  delay(10);
  Serial.println("Closing Connection");
  serverClient.stop();
  
  



}



// This function runs once only, when the Arduino is turned on or reseted.
void setup() {

  Serial.begin(115200);

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
  AgentServer.begin();
  AgentServer.setNoDelay(true);
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
