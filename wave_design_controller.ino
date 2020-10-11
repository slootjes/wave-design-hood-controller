#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <RCSwitch.h>

const char* ssid = "";
const char* password =  "";
const char* mqttServer = "";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

const int commandHoodTimer = 233305;
const int commandHoodMinus = 233306;
const int commandHoodPlus = 233308;
const int commandHoodOff = 233309;
const int commandLightToggle = 233310;

unsigned long code = 0;
bool ignoreReceive = false;

int statusLight = 0;
int statusHood = 0;
 
WiFiClient espClient;
PubSubClient client(espClient);
RCSwitch receiver = RCSwitch();
RCSwitch transmitter = RCSwitch();
 
void setup() {
  Serial.begin(115200);

  receiver.enableReceive(4);

  transmitter.enableTransmit(5);
  transmitter.setProtocol(8);
 
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
 
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
 
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword )) {
      Serial.println("connected");  
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
 
  client.publish("wave/lwt", "online");
  client.subscribe("wave/cmd");
  Serial.println("subscribed");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Command received from MQTT: ");

  String command;
  for (int i = 0; i < length; i++) {
    command = command + (char)payload[i];
  }
  Serial.println(command);

  if (command == "light_toggle") {
    transmitLight();
    delay(1000);
  }
  else if (command == "light_on") {
      if (statusLight == 0) {
        statusLight = 1;
        transmitLight();
      }
  }
  else if (command == "light_off") {
    if (statusLight == 1) {
      statusLight = 0;
      transmitLight();
    }
  }
  else if (command == "light_on_dimmed") {
    if (statusLight == 0) {
      statusLight = 1;
      transmitter.setRepeatTransmit(1200);
      transmitLight();
      transmitter.setRepeatTransmit(10);
    }
  }
  else if (command == "hood_plus") {
    if (statusHood < 3) {
      statusHood = statusHood + 1;
      transmitHoodPlus();
    }
  }
  else if (command == "hood_on") {
    if (statusHood == 0) {
      statusHood = 1;
      transmitHoodPlus();
    }
  }
  else if (command == "hood_min") {
    if (statusHood > 0) {
      statusHood = statusHood - 1;
      transmitHoodMin();
    }
  }
  else if (command == "hood_off") {
    statusHood = 0;
    transmitHoodOff();
  }
}

void transmitLight() {
  transmit(commandLightToggle);
  broadcastLight();
}

void transmitHoodPlus() {
  transmit(commandHoodPlus);
  broadcastHood();
}

void transmitHoodMin() {
  transmit(commandHoodMinus);
  broadcastHood();
}

void transmitHoodOff() {
  transmit(commandHoodOff);
  broadcastHood();
}

void broadcastLight() {
  client.publish("wave/light/state", getLightStatus());
}

void broadcastHood() {
  client.publish("wave/hood/state", getHoodStatus());
  client.publish("wave/hood/data", getHoodData());
}

void transmit(unsigned long code) {
  transmitter.send(code, 18);
  ignoreReceive = true; // we seem to receive our own signal back (either code or unit) so make sure to ignore it
  delay(1000);
}

char* getLightStatus() {
  switch (statusLight) {
    case 1:
      return "ON";
    default:
      return "OFF";
  }
}

char* getHoodStatus() {
  switch (statusHood) {
    case 1:
    case 2:
    case 3:
      return "ON";
    default:
      return "OFF";
  }
}

char* getHoodData() {
  switch (statusHood) {
    case 1:
      return "\"{Speed\": \"1\"}";
    case 2:
      return "\"{Speed\": \"2\"}";
    case 3:
      return "\"{Speed\": \"3\"}";
    default:
      return "\"{Speed\": \"0\"}";
  }
}

void listenToRemote() {
  if (receiver.available()) {
    if (ignoreReceive == true) {
      ignoreReceive = false;
    }
    else {
      code = receiver.getReceivedValue();
      Serial.print("Command received from remote: ");
      if (code > 0) {
        switch (code) {
          case commandHoodTimer:
            // not supported right now
          break;
          case commandHoodMinus:
            Serial.println("hood_min");
            if (statusHood > 0) {
              statusHood = statusHood - 1;
              broadcastHood();
            }
          break;
          case commandHoodPlus:
            Serial.println("hood_plus");
            if (statusHood < 3) {
              statusHood = statusHood + 1;
              broadcastHood();
            }
          break;
          case commandHoodOff:
            Serial.println("hood_off");
            statusHood = 0;
            broadcastHood();
          break;
          case commandLightToggle:
            Serial.println("light_toggle");
            if (statusLight == 0) {
              statusLight = 1;
            }
            else {
              statusLight = 0;
            }
            broadcastLight();
          break;
        }
        delay(1000); // ignore subsequent received signals as these are duplicate
      }
    }
    receiver.resetAvailable();
  }
}

void loop() {
  listenToRemote();
  client.loop();
}
