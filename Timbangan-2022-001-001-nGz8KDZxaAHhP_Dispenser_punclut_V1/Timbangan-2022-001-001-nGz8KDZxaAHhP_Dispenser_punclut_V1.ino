#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include "HX711.h"
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>
#include "secret.h"

#if defined(ESP32)
    #error "Software Serial is not supported on the ESP32"
#endif
 //WOI

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN D2
#define PZEM_TX_PIN D1
#endif

#define EEPROM_SIZE 12
SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzem(pzemSWSerial);

WiFiClient espClient;
PubSubClient client(espClient);

const char* mqtt_server = MQTT_SERVER ;
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS;

const char* mqttQueueTimbangan = MQTTQUEUETIMBANGAN;
const char* mqttQueuePublish = MQTTQUEUEPUBLISH;
const char* mqttSubscribe = MQTTSUBSCRIBE;

const int deviceDout= D6; // Deklarasi Pin 4 (12=6) ini = D2
const int deviceSck= D5; // Deklarasi Pin 12 (14=5) D5
HX711 scale;
float calibration_factor = 18650; // Memasukan Nilai "18650" ke Varibel calibration_factor dalam tipe data Float
const char* CL = "LSKK-DISPENSER-nGz8KDZxaAHhP"; //ini diganti
String deviceGuid = "Timbangan-2022-001-001"; //ini diganti juga
byte mac[6];
int address = 0; 
int waktuKirim = 3000;
String MACAddress;
float dataWeight;

bool shouldSaveConfig = false;

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

String mac2String(byte ar[]) {
  String s;
  for (byte i = 0; i < 6; ++i)
  {
    char buf[3];
    sprintf(buf, "%2X", ar[i]);
    s += buf;
    if (i < 5) s += ':';
  }
  return s;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String response;
  String timerValue;
  String guidAuth;
  String res;
  String resGuid;
  for (int i = 0; i < length; i++) {
    response += (char)payload[i];
    if (i < 36){
      guidAuth += (char)payload[i];
    }
    if (i > 36){
      timerValue += (char)payload[i];
    }
    if (i < 5) {
      res += (char)payload[i];
    }
    if (i > 5 and i < 42) {
      resGuid += (char)payload[i];
    }
    
  }
  Serial.println(guidAuth);
  if (guidAuth == deviceGuid){
    Serial.print("Waktu Kirim sekarang adalah : ");
    Serial.println(timerValue);
    waktuKirim = timerValue.toInt();
  }  
  if (res == "reset" and resGuid == deviceGuid){
    WiFiManager wifiManager;
    wifiManager.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    wifiManager.startConfigPortal("Timbangan Dispenser");
    Serial.println("connected!");
  }
}
void printMACAddress() {
  WiFi.macAddress(mac);
  MACAddress = mac2String(mac);
  Serial.println(MACAddress);
}
void setup_wifi () {
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  if(!wifiManager.autoConnect("Timbangan Dispenser")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.reset();
    delay(5000);
  } 
  Serial.println("connected...yeey :)");
}
float data_terakhir;
float data_muncul;
void load_data() {
  data_terakhir = EEPROM.get(address,dataWeight);
  Serial.println(data_terakhir);
}
void save_data () {
  data_muncul = EEPROM.put(address,dataWeight);
}
void mqttReconnect() {
  printMACAddress();
  const char* CL;
  CL = MACAddress.c_str();
  Serial.println(CL);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(CL, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe(mqttSubscribe);
    } else {Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      ESP.restart();
      delay(5000);
    }
  }
}
void setup() {
  Serial.begin(115200);
  setup_wifi ();
  EEPROM.begin(EEPROM_SIZE);
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
  scale.begin(deviceDout, deviceSck);  
  scale.set_scale(calibration_factor); //Adjust to this calibration factor 
//  scale.tare(); //Reset the scale to 0 
  long zero_factor = scale.read_average(); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);
}
void loop() {
  String dataSendtoMqtt;
  if (!client.connected()) {   
   mqttReconnect();
  }
  client.loop();
  Serial.print("Custom Address: "); 
  Serial.println(pzem.readAddress(), HEX);
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();
  float frequency = pzem.frequency();
  float pf = pzem.pf();

  // Check if the data is valid
  if(isnan(voltage)){
      Serial.println("Error reading VOLT");
  } else if (isnan(current)) {
      Serial.println("Error reading current");
  } else if (isnan(power)) {
      Serial.println("Error reading power");
  } else if (isnan(energy)) {
      Serial.println("Error reading energy");
  } else if (isnan(frequency)) {
      Serial.println("Error reading frequency");
  } else if (isnan(pf)) {
      Serial.println("Error reading power factor");
  } else {
    // Print the values to the Serial console
    Serial.print("Voltage: ");      Serial.print(voltage);      Serial.println("V");
    Serial.print("Current: ");      Serial.print(current);      Serial.println("A");
    Serial.print("Power: ");        Serial.print(power);        Serial.println("W");
    Serial.print("Energy: ");       Serial.print(energy,3);     Serial.println("kWh");
    Serial.print("Frequency: ");    Serial.print(frequency, 1); Serial.println("Hz");
    Serial.print("PF: ");           Serial.println(pf);
  }
   
  dataSendtoMqtt = String(deviceGuid + "#" + voltage + "#" + current + "#" + power + "#" + energy + "#" + frequency + "#" + pf);
//  client.publish(mqttQueuePublish, dataSendtoMqtt.c_str());
  Serial.println(dataSendtoMqtt); 
// 
  dataWeight = scale.get_units();
  if (dataWeight < 0){
    dataWeight = dataWeight * -1;
  }
  String convertDataWeight = String(dataWeight);
  String dataRMQ = String(convertDataWeight);
  char dataToMQTT[50];
  dataRMQ.toCharArray(dataToMQTT, sizeof(dataToMQTT)); 
  Serial.println("Ini Data untuk ke MQTT: ");
  Serial.println(dataToMQTT);
  Serial.println("data dari eeprom");
  Serial.println("-------------------------------------------------------------------");
  dataSendtoMqtt = String(deviceGuid + "#" + voltage + "#" + current + "#" + power + "#" + energy + "#" + frequency + "#" + pf + "#" + dataToMQTT);
//  client.publish(mqttQueueTimbangan, dataSendtoMqtt.c_str());
  Serial.println(dataSendtoMqtt);
  client.publish(mqttQueueTimbangan,dataSendtoMqtt.c_str());
  delay(waktuKirim); //300000
}
