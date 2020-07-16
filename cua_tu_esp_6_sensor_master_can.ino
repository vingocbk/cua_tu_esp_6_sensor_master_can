#include "cua_tu_esp_6_sensor_master_can.h"


void getStatus(int deviceId,bool isopen, int red, int green, int blue){
    String datasend = "{\"deviceid\" : \"";
    datasend += String(deviceId);
    datasend += "\", \"devicetype\" : \"motor\", \"typecontrol\" : \"getstatus\",  \"number_disconnect\" : \"";
    datasend += String(countDisconnectToServer - 1);
    datasend += "\", \"all_time\" : \"";
    datasend += String(sum_time_disconnect_to_sever);
    datasend += "\", \"red\" : \"";
    datasend += String(red);
    datasend += "\", \"green\" : \"";
    datasend += String(green);
    datasend += "\", \"blue\" : \"";
    datasend += String(blue);
    datasend +=  "\", \"status\" : \"";
    if(isopen == true){
        datasend += "open\"}";
        client.publish(topicsendStatus, datasend.c_str());
    }else{
        datasend += "close\"}";
        client.publish(topicsendStatus, datasend.c_str());
    }
    ECHOLN("getstatus");
}


void clearEeprom(){
    ECHOLN("clearing eeprom");
    for (int i = 0; i < EEPROM_WIFI_MAX_CLEAR; ++i){
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    ECHOLN("Done writing!");
}


void ConfigMode(){
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    ECHOLN(server.arg("plain"));
    JsonObject& rootData = jsonBuffer.parseObject(server.arg("plain"));
    ECHOLN("--------------");
    tickerSetApMode.stop();
    digitalWrite(LED_TEST_AP, LOW);
    if (rootData.success()) {
        server.sendHeader("Access-Control-Allow-Headers", "*");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json; charset=utf-8", "{\"status\":\"success\"}");
        //server.stop();
        String nssid = rootData["ssid"];
        String npass = rootData["password"];
        String nid_start = rootData["id_start"];
        String nid_end = rootData["id_end"];
        String nserver = rootData["server"];

        esid = nssid;
        epass = npass;
        device_id_start = nid_start.toInt();
        device_id_end = nid_end.toInt();
        serverMqtt = nserver;

        ECHOLN("clearing eeprom");
        for (int i = 0; i <= EEPROM_WIFI_SERVER_END; i++){ 
            EEPROM.write(i, 0); 
        }
        ECHOLN("writing eeprom ssid:");
        ECHO("Wrote: ");
        for (int i = 0; i < nssid.length(); ++i){
            EEPROM.write(i+EEPROM_WIFI_SSID_START, nssid[i]);             
            ECHO(nssid[i]);
        }
        ECHOLN("");
        ECHOLN("writing eeprom pass:"); 
        ECHO("Wrote: ");
        for (int i = 0; i < npass.length(); ++i){
            EEPROM.write(i+EEPROM_WIFI_PASS_START, npass[i]);
            ECHO(npass[i]);
        }
        ECHOLN("");
        ECHOLN("writing eeprom device id start:"); 
        ECHO("Wrote: ");
        EEPROM.write(EEPROM_WIFI_DEVICE_ID_START, device_id_start);
        ECHOLN(device_id_start);

        ECHOLN("writing eeprom device id end:");
        ECHO("Wrote: ");
        EEPROM.write(EEPROM_WIFI_DEVICE_ID_END, device_id_end);
        ECHOLN(device_id_end);

        ECHOLN("writing eeprom server:"); 
        ECHO("Wrote: ");
        for (int i = 0; i < nserver.length(); ++i){
            EEPROM.write(i+EEPROM_WIFI_SERVER_START, nserver[i]);
            ECHO(nserver[i]);
        }
        ECHOLN("");

        EEPROM.commit();
        ECHOLN("Done writing!");

        if (testWifi(nssid, npass)) {
            // ConnecttoMqttServer();
            Flag_Normal_Mode = true;
            return;
        }
        tickerSetApMode.start();
        ECHOLN("Wrong wifi!!!");
        SetupConfigMode();
        StartConfigServer();
        return;
    }
    ECHOLN("Wrong data!!!");
}

bool connectToWifi(String nssid, String npass) {
    ECHOLN("Open STA....");
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    delay(100);
    // setupIP();
    //WiFi.begin(nssid.c_str(), npass.c_str());

    if (testWifi(nssid, npass)) {
        return true;
    }

    return false;
}

bool testWifi(String esid, String epass) {
    ECHO("Connecting to: ");
    ECHOLN(esid);
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    server.close();

    WiFi.mode(WIFI_STA);        //bat che do station
    WiFi.begin(esid.c_str(), epass.c_str());
    int c = 0;
    ECHOLN("Waiting for Wifi to connect");
    while (c < 20) {
        if (WiFi.status() == WL_CONNECTED) {
            ECHOLN("\rWifi connected!");
            ECHO("Local IP: ");
            ECHOLN(WiFi.localIP());
            // digitalWrite(LED_TEST_AP, HIGH);
            ConnecttoMqttServer();
            return true;
        }
        delay(500);
        ECHO(".");
        c++;
        if(digitalRead(PIN_CONFIG) == LOW){
            break;
        }
    }
    ECHOLN("");
    ECHOLN("Connect timed out");
    return false;
}

void ConnecttoMqttServer(){
    client.setServer(serverMqtt.c_str(), MQTT_PORT);
    client.setCallback(callbackMqttBroker);
    reconnect();
}



void callbackMqttBroker(char* topic, byte* payload, unsigned int length){
    
    String Topic = String(topic);
    ECHO("TOPIC: ");
    ECHOLN(Topic);

    String data;
    for (int i = 0; i < length; i++) {
        data += char(payload[i]);
    }
    ECHO("DATA: ");
    ECHOLN(data);
    StaticJsonBuffer<RESPONSE_LENGTH> jsonBuffer;
    JsonObject& rootData = jsonBuffer.parseObject(data);

    if(Topic.indexOf(m_Controlhand) > 0){
        if (rootData.success()){
            if(rootData["typedevice"] == m_Typedevice){
                int arraySize = rootData["deviceid"].size();   //get size of JSON Array
                int device_id[arraySize];
                bool isTrueControl = false;
                // ECHO("size: ");
                // ECHOLN(arraySize);
                for (int i = 0; i < arraySize; i++) { //Iterate through results
                    device_id[i] = rootData["deviceid"][i];  //Implicit cast
                    if(device_id[i] >= device_id_start && device_id[i] <= device_id_end){
                        String dataType = rootData["typecontrol"];
                        if(dataType == "controlled"){
                            int controlled[3];
                            for (int i = 0; i < 3; i++) { //Iterate through results
                                controlled[i] = rootData["data"][i];  //Implicit cast
                            }
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 5;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_CONTROL_LED_HAND;
                            tx_frame.data.u8[2] = controlled[0];
                            tx_frame.data.u8[3] = controlled[1];
                            tx_frame.data.u8[4] = controlled[2];
                            ESP32Can.CANWriteFrame(&tx_frame);

                        }

                    }
                }
            }
        }
    }

    else if(Topic.indexOf(m_Control) > 0){
        if (rootData.success()){
            //--------------getstatus-------------
            if(rootData["typedevice"] == m_Typedevice){
                int arraySize = rootData["deviceid"].size();   //get size of JSON Array
                int device_id[arraySize];
                String dataType = rootData["typecontrol"];
                
                for (int i = 0; i < arraySize; i++) { //Iterate through results
                    device_id[i] = rootData["deviceid"][i];  //Implicit cast
                    // ECHOLN(device_id[i]);
                    if(device_id[i] >= device_id_start && device_id[i] <= device_id_end){
                        //set id
                        if(dataType == "set_id"){
                            ECHOLN("set id");
                            String Strsetid = rootData["data"];
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 2;
                            tx_frame.data.u8[0] = MSG_SET_ID;
                            tx_frame.data.u8[1] = Strsetid.toInt();;
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }
                        //---------getstatus------------------
                        if(dataType == "getstatus"){
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 2;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_GET_STATUS;
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }

                        //---------control------------------
                        else if(dataType == "control"){
                            String dataControl = rootData["data"];
                            if(dataControl == "open"){
                                CAN_frame_t tx_frame;
                                tx_frame.FIR.B.FF = CAN_frame_std;
                                tx_frame.MsgID = MSG_MASTER_ID;
                                tx_frame.FIR.B.DLC = 2;
                                tx_frame.data.u8[0] = device_id[i];
                                tx_frame.data.u8[1] = MSG_CONTROL_OPEN;
                                ESP32Can.CANWriteFrame(&tx_frame);
                            }
                            else if(dataControl == "close"){
                                CAN_frame_t tx_frame;
                                tx_frame.FIR.B.FF = CAN_frame_std;
                                tx_frame.MsgID = MSG_MASTER_ID;
                                tx_frame.FIR.B.DLC = 2;
                                tx_frame.data.u8[0] = device_id[i];
                                tx_frame.data.u8[1] = MSG_CONTROL_CLOSE;
                                ESP32Can.CANWriteFrame(&tx_frame);
                            }
                            else if(dataControl == "stop"){
                                CAN_frame_t tx_frame;
                                tx_frame.FIR.B.FF = CAN_frame_std;
                                tx_frame.MsgID = MSG_MASTER_ID;
                                tx_frame.FIR.B.DLC = 2;
                                tx_frame.data.u8[0] = device_id[i];
                                tx_frame.data.u8[1] = MSG_CONTROL_STOP;
                                ESP32Can.CANWriteFrame(&tx_frame);
                            }
                        }
                        //---------controlled------------------
                        else if(dataType == "controlled"){
                            int controlled[3];
                            for (int i = 0; i < 3; i++) { //Iterate through results
                                controlled[i] = rootData["data"][i];  //Implicit cast
                            }
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 5;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_CONTROL_LED_VOICE;
                            tx_frame.data.u8[2] = controlled[0];
                            tx_frame.data.u8[3] = controlled[1];
                            tx_frame.data.u8[4] = controlled[2];
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }
                        //---------resetdistant------------------
                        else if(dataType == "resetdistant"){
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 2;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_RESET_DISTANT;
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }
                        //---------settimereturn------------------
                        else if(dataType == "settimereturn"){
                            String dataTimereturn = rootData["data"];
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 3;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_TIME_RETURN;
                            tx_frame.data.u8[2] = dataTimereturn.toInt();
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }
                        //---------setmoderun------------------
                        else if(dataType == "setmoderun"){
                            String setmoderunstr = rootData["data"];
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 3;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_MODE_RUN;
                            tx_frame.data.u8[2] = setmoderunstr.toInt();
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }
                        //---------percent in out------------------
                        else if(dataType == "setlowspeed"){
                            int percentSlow[2];
                            for (int i = 0; i < 2; i++) { //Iterate through results
                                percentSlow[i] = rootData["data"][i];  //Implicit cast
                                ECHOLN(percentSlow[i]);
                            }
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 4;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_PERCENT_LOW;
                            tx_frame.data.u8[2] = percentSlow[0];
                            tx_frame.data.u8[3] = percentSlow[0];
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }
                        else if(dataType == "changedelayanalog"){
                            String delay_analog_Str = rootData["data"];
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 3;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_DELAY_ANALOG;
                            tx_frame.data.u8[2] = delay_analog_Str.toInt();;
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }
                        else if(dataType == "changeerroranalog"){
                            String error_analog_Str = rootData["data"];
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 3;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_ERROR_ANALOG;
                            tx_frame.data.u8[2] = error_analog_Str.toInt();;
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }

                        else if(dataType == "timeautoclose"){
                            String Strtimeautoclose = rootData["data"];
                            CAN_frame_t tx_frame;
                            tx_frame.FIR.B.FF = CAN_frame_std;
                            tx_frame.MsgID = MSG_MASTER_ID;
                            tx_frame.FIR.B.DLC = 3;
                            tx_frame.data.u8[0] = device_id[i];
                            tx_frame.data.u8[1] = MSG_AUTO_CLOSE;
                            tx_frame.data.u8[2] = Strtimeautoclose.toInt();;
                            ESP32Can.CANWriteFrame(&tx_frame);
                        }

                        

                    }
                }
            }
        }
    }
}

void reconnect() {
    // Loop until we're reconnected
    ECHO("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Avy-";
    // clientId += GetFullSSID;
    clientId += String(random(0xffffff), HEX);
    // const char* willTopic = "CabinetAvy/HPT/LWT";
    // uint8_t willQos = 0;
    // boolean willRetain = false;
    // String willMessage = "{\"devicetype\" : \"";
    // willMessage += m_Typedevice;
    // willMessage += "\", \"deviceid\" : \"";
    // willMessage += String(deviceId);
    // willMessage += "\", \"status\" : \"error\"}";
    // Attempt to connect
    // if (client.connect(clientId.c_str(), willTopic, willQos, willRetain, willMessage.c_str())) {
    if(client.connect(clientId.c_str())){
        ECHO("connected with id: ");
        ECHOLN(clientId);
        String topicControl = m_Pretopic + m_Control;
        String topicControlhand = m_Pretopic + m_Controlhand;

        
        // client.subscribe(topicGetstatus.c_str());
        client.subscribe(topicControl.c_str());
        client.subscribe(topicControlhand.c_str());
        ECHO("Done Subscribe Channel: ");
        // ECHO(topicGetstatus);
        // ECHO("  +  ");
        ECHO(topicControl);
        ECHO(", ");
        ECHOLN(topicControlhand);
        countDisconnectToServer++;
        if(flag_disconnect_to_sever == true){
            sum_time_disconnect_to_sever += millis() - count_time_disconnect_to_sever;
            flag_disconnect_to_sever = false;
        }
        digitalWrite(LED_TEST_AP, HIGH);

        for(int i = device_id_start; i <= device_id_end; i++){
            CAN_frame_t tx_frame;
            tx_frame.FIR.B.FF = CAN_frame_std;
            tx_frame.MsgID = MSG_MASTER_ID;
            tx_frame.FIR.B.DLC = 2;
            tx_frame.data.u8[0] = i;
            tx_frame.data.u8[1] = 1;
            ESP32Can.CANWriteFrame(&tx_frame);
        }
    } else {
        ECHO("failed, rc=");
        ECHO(client.state());
        ECHOLN(" try again in 2 seconds");
        // Wait 2 seconds before retrying
        delay(2000);
    }
}

void setLedApMode() {
    digitalWrite(LED_TEST_AP, !digitalRead(LED_TEST_AP));
}



String GetFullSSID() {
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    String macID;
    WiFi.mode(WIFI_AP);
    WiFi.softAPmacAddress(mac);
    macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    ECHO("[Helper][getIdentify] Identify: ");
    ECHO(SSID_PRE_AP_MODE);
    ECHOLN(macID);
    return macID;
}

void checkButtonConfigClick(){
    if (digitalRead(PIN_CONFIG) == LOW && (ConfigTimeout + CONFIG_HOLD_TIME) <= millis()) { // Khi an nut
        ConfigTimeout = millis();
        //tickerSetMotor.attach(0.2, setLedApMode);  //every 0.2s
        Flag_Normal_Mode = false;
        tickerSetApMode.start();
        SetupConfigMode();
        StartConfigServer();
    } else if(digitalRead(PIN_CONFIG) == HIGH) {
        ConfigTimeout = millis();
    }
}


void SetupConfigMode(){
    ECHOLN("[WifiService][setupAP] Open AP....");
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    server.close();
    delay(1000);
    WiFi.mode(WIFI_AP_STA);
    String SSID_AP_MODE = SSID_PRE_AP_MODE + GetFullSSID();
    WiFi.softAP(SSID_AP_MODE.c_str(), PASSWORD_AP_MODE);
    IPAddress APIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.softAPConfig(APIP, gateway, subnet);
    ECHOLN(SSID_AP_MODE);

    ECHOLN("[WifiService][setupAP] Softap is running!");
    IPAddress myIP = WiFi.softAPIP();
    ECHO("[WifiService][setupAP] IP address: ");
    ECHOLN(myIP);
}

void StartConfigServer(){    
    ECHOLN("[HttpServerH][startConfigServer] Begin create new server...");
//    server = new ESP8266WebServer(HTTP_PORT);
    server.on("/config", HTTP_POST, ConfigMode);
    server.begin();
    ECHOLN("[HttpServerH][startConfigServer] HTTP server started");
}


void SetupNetwork() {
    ECHOLN("Reading EEPROM ssid");
    esid = "";
    for (int i = EEPROM_WIFI_SSID_START; i < EEPROM_WIFI_SSID_END; ++i){
        esid += char(EEPROM.read(i));
    }
    ECHO("SSID: ");
    ECHOLN(esid);
    ECHOLN("Reading EEPROM pass");
    epass = "";
    for (int i = EEPROM_WIFI_PASS_START; i < EEPROM_WIFI_PASS_END; ++i){
        epass += char(EEPROM.read(i));
    }
    ECHO("PASS: ");
    ECHOLN(epass);

    ECHOLN("Reading EEPROM Device ID START");
    device_id_start = EEPROM.read(EEPROM_WIFI_DEVICE_ID_START);
    ECHO("ID: ");
    ECHOLN(device_id_start);

    ECHOLN("Reading EEPROM Device ID END");
    device_id_end = EEPROM.read(EEPROM_WIFI_DEVICE_ID_END);
    ECHO("ID: ");
    ECHOLN(device_id_end);

    ECHOLN("Reading EEPROM server");
    serverMqtt = "";
    for (int i = EEPROM_WIFI_SERVER_START; i < EEPROM_WIFI_SERVER_END; ++i){
        serverMqtt += char(EEPROM.read(i));
    }
    ECHO("SERVER: ");
    ECHOLN(serverMqtt);
    
    testWifi(esid, epass);
}


void tickerupdate(){
    tickerSetApMode.update();
}

void receiveDataCan(){
    CAN_frame_t rx_frame;
    if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE) {

        if (rx_frame.FIR.B.FF == CAN_frame_std) {
        printf("New standard frame");
        }
        else {
        printf("New extended frame");
        }

        if (rx_frame.FIR.B.RTR == CAN_RTR) {
        printf(" RTR from %d, DLC %d\r\n", rx_frame.MsgID,  rx_frame.FIR.B.DLC);
        }
        else {
        printf(" from %d, DLC %d, Data ", rx_frame.MsgID,  rx_frame.FIR.B.DLC);
        for (int i = 0; i < rx_frame.FIR.B.DLC; i++) {
            printf("%d ", rx_frame.data.u8[i]);
        }
        printf("\n");
        }

        if(rx_frame.data.u8[0] == 1){
            getStatus(rx_frame.MsgID, true, rx_frame.data.u8[1], rx_frame.data.u8[2], rx_frame.data.u8[3]);
        }else{
            getStatus(rx_frame.MsgID, false, rx_frame.data.u8[1], rx_frame.data.u8[2], rx_frame.data.u8[3]);
        }


    }
}


void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

    pinMode(LED_TEST_AP, OUTPUT);
    pinMode(PIN_CONFIG, INPUT);
    digitalWrite(LED_TEST_AP, LOW);

    Serial.begin(115200);
    EEPROM.begin(512);

    

    CAN_cfg.speed = CAN_SPEED_125KBPS;
    // CAN_cfg.tx_pin_id = GPIO_NUM_25; //with magic arm board
    // CAN_cfg.rx_pin_id = GPIO_NUM_33;
    CAN_cfg.tx_pin_id = GPIO_NUM_19;    //with rgb board
    CAN_cfg.rx_pin_id = GPIO_NUM_21;
    CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
    // Init CAN Module
    ESP32Can.CANInit();

    SetupNetwork();     //khi hoat dong binh thuong


}


void loop() {
    if (Flag_Normal_Mode == true && WiFi.status() != WL_CONNECTED){
        // digitalWrite(LED_TEST_AP, LOW);
        if(flag_disconnect_to_sever == false){
            count_time_disconnect_to_sever = millis();
            flag_disconnect_to_sever = true;
        }
        testWifi(esid, epass); 
    } 

    if(WiFi.status() == WL_CONNECTED){
        if (!client.connected()) {
            digitalWrite(LED_TEST_AP, LOW);
            if(flag_disconnect_to_sever == false){
                count_time_disconnect_to_sever = millis();
                lastReconnectAttempt = millis();
                flag_disconnect_to_sever = true;
            }
            unsigned long nowReconnectAttempt = millis();

            if (abs(nowReconnectAttempt - lastReconnectAttempt) > 10000) {
                lastReconnectAttempt = nowReconnectAttempt;
                reconnect();
            }
        }else{
            client.loop();
        }
        
    }




    checkButtonConfigClick();
    tickerupdate();
    receiveDataCan();
    server.handleClient();
}
