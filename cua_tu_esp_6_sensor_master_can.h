#include "AppDebug.h"
#include <Arduino.h>
#include "WiFi.h"
#include "HTTPClient.h"
#include "WebServer.h"
#include "ESPmDNS.h"
#include "ArduinoJson.h"
#include "Ticker.h"
#include "EEPROM.h"
#include <PubSubClient.h>

#include <ESP32CAN.h>
#include <CAN_config.h>

#include "soc/soc.h"  //Brownout detector was triggered
#include "soc/rtc_cntl_reg.h"

#define LED_TEST_AP  32 // D4 onchip GPIO2
#define PIN_CONFIG 0       // D3 flash GPIO0


#define RESPONSE_LENGTH 2048     //do dai data nhan ve tu tablet
#define EEPROM_WIFI_SSID_START 0
#define EEPROM_WIFI_SSID_END 32
#define EEPROM_WIFI_PASS_START 33
#define EEPROM_WIFI_PASS_END 64
#define EEPROM_WIFI_DEVICE_ID_START 65
#define EEPROM_WIFI_DEVICE_ID_END 66
#define EEPROM_WIFI_SERVER_START 67
#define EEPROM_WIFI_SERVER_END 128
#define EEPROM_WIFI_MAX_CLEAR 512


#define SSID_PRE_AP_MODE "AvyInterior-"
#define PASSWORD_AP_MODE "123456789"

#define m_Getstatus "/getstatus"
#define m_Control "/control"
#define m_Controlhand "/controlhand"
#define m_Resetdistant "/resetdistant"
#define m_Ledrgb "/ledrgb"
#define m_Setmoderun "/setmoderun"
#define m_Settimereturn "/settimereturn"
#define m_Setlowspeed "/setlowspeed"
#define m_Typedevice  "motor"

#define MSG_MASTER_ID         0
#define MSG_GET_STATUS        1
#define MSG_CONTROL_OPEN      2
#define MSG_CONTROL_CLOSE     3
#define MSG_CONTROL_STOP      4
#define MSG_CONTROL_LED_VOICE 5
#define MSG_CONTROL_LED_HAND  6
#define MSG_RESET_DISTANT     7
#define MSG_TIME_RETURN       8
#define MSG_MODE_RUN          9
#define MSG_PERCENT_LOW       10
#define MSG_DELAY_ANALOG      11
#define MSG_ERROR_ANALOG      12
#define MSG_AUTO_CLOSE        13
#define MSG_MIN_STOP_SPEED    14

#define MSG_SET_ID       100


#define HTTP_PORT 80
#define MQTT_PORT 1883
#define WL_MAC_ADDR_LENGTH 6

#define CONFIG_HOLD_TIME 5000

// const char* mqtt_server = "test.mosquitto.org";
//const char* mqtt_server = "iot.eclipse.org";
//const char* mqtt_server = "broker.mqtt-dashboard.com";
const char* mqtt_server = "3.3.0.120";

const char* topicsendStatus = "CabinetAvy/HPT/deviceStatus";
const char* m_userNameServer = "avyinterial";
const char* m_passSever = "avylady123";
String m_Pretopic = "CabinetAvy/HPT";
WiFiClient espClient;
PubSubClient client(espClient);

WebServer server(HTTP_PORT);

//normal mode
void getStatus(int deviceId, bool isopen, int red, int green, int blue);
void clearEeprom();
void SetupNetwork();
void tickerupdate();
void ConnecttoMqttServer();
void callbackMqttBroker(char* topic, byte* payload, unsigned int length);
void reconnect();
void receiveDataCan();


//Config Mode
void checkButtonConfigClick();      //kiem tra button
void SetupConfigMode();             //phat wifi
void StartConfigServer();           //thiet lap sever
void ConfigMode();                  //nhan data tu app
void setLedApMode();                //hieu ung led
String GetFullSSID();
bool connectToWifi(String nssid, String npass);
bool testWifi(String esid, String epass);
unsigned long ConfigTimeout;

String esid, epass, serverMqtt;
int device_id_start, device_id_end;
uint32_t countDisconnectToServer = 0;
unsigned long count_time_disconnect_to_sever = 0;
bool flag_disconnect_to_sever = false;
unsigned long sum_time_disconnect_to_sever = 0;
unsigned long lastReconnectAttempt = 0;

bool Flag_Normal_Mode = true;

bool forward = true;
bool statusStop = true;
bool clickbutton = false;
bool statusLed = false;

// unsigned long Pul_Motor;
// unsigned long test_time, time_start_speed;
CAN_device_t CAN_cfg;               // CAN Config
const int rx_queue_size = 10;       // Receive Queue size

Ticker tickerSetApMode(setLedApMode, 200, 0);   //every 200ms
