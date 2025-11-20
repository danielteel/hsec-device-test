#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include <esp_wifi.h>
#include <Adafruit_BME280.h>
#include "time.h"
#include "utils.h"
#include "secrets.h"
#include "net.h"
#include <time.h> 
#include "storage.h"


const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

Net NetClient(SECRET_DEVICE_NAME, SECRET_ENCROKEY, SECRET_HOST_ADDRESS, SECRET_HOST_PORT);

const uint32_t weatherPeriod = 1000;

const uint32_t howLongBeforeRestartIfNotConnecting = 300000;//restart esp32 if havent been able to connect to server for 5 minutes


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";
const char* ntpServer3 = "time.windows.com";
const char* timeZone = "MST7MDT,M3.2.0,M11.1.0";//https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

StorageData storageData;



Adafruit_BME280 bme(39, 40, 41, 42); // use I2C interface
Adafruit_BME280 bme2(13, 10, 11, 12); // use I2C interface

void packetReceived(uint8_t* data, uint32_t dataLength){
    sensor_t * s;
    int32_t* numValue=(int32_t*)(data+1);

    DanTime* time=(DanTime*)(data+1);

    DanColor* color=(DanColor*)(data+1);

    bool* boolVal=(bool*)data+1;


    switch (data[0]){
        case 0:
            storageData.red=color->red;
            storageData.green=color->green;
            storageData.blue=color->blue;
            commitStorage(storageData);
            NetClient.sendString(String("color=")+String(storageData.red)+","+String(storageData.green)+","+String(storageData.blue));
            break;
        case 1:
            storageData.lightOn=*boolVal;
            commitStorage(storageData);
            NetClient.sendString(String("lightOn=")+String(storageData.lightOn?"1":"0"));
            break;
        case 2:
            storageData.startTime=*time;
            NetClient.sendString(String("startTime=")+String(storageData.startTime.hours)+String(":")+String(storageData.startTime.minutes)+String(":")+String(storageData.startTime.seconds));
            break;
        case 3:
            storageData.endTime=*time;
            NetClient.sendString(String("endTime=")+String(storageData.endTime.hours)+String(":")+String(storageData.endTime.minutes)+String(":")+String(storageData.endTime.seconds));
            break;
        case 0xFF:
            Serial.println(String(data+1, dataLength-1));
            break;
    }
}

void onConnected(){
    Serial.println("NetClient Connected");
    NetClient.sendString(String("color=")+String(storageData.red)+","+String(storageData.green)+","+String(storageData.blue));
    NetClient.sendString(String("lightOn=")+String(storageData.lightOn?"1":"0"));
    NetClient.sendString(String("startTime=")+String(storageData.startTime.hours)+String(":")+String(storageData.startTime.minutes)+String(":")+String(storageData.startTime.seconds));
    NetClient.sendString(String("endTime=")+String(storageData.endTime.hours)+String(":")+String(storageData.endTime.minutes)+String(":")+String(storageData.endTime.seconds));
    NetClient.sendString(String("inTimeWindow=Not sure yet"));
    NetClient.sendString(String("subscribe:Greenhouse:temperature"));
    NetClient.sendString(String("subscribe:solar:humidity"));
}

void onDisconnected(){
    Serial.println("NetClient disconnected");
}

void WiFiSetup(bool doRandomMAC){
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);

    uint8_t newMac[6]={0,0,0,0,0,0};
    String newHostName=SECRET_DEVICE_NAME;
    if (doRandomMAC){
        esp_fill_random(newMac+1, 5);
        for (int i=0;i<6;i++){
            newHostName+=String(newMac[i], 16);
        }
    }
    WiFi.setHostname(newHostName.c_str());

    WiFi.mode(WIFI_STA);
    if (doRandomMAC) esp_wifi_set_mac(WIFI_IF_STA, newMac);
    WiFi.setMinSecurity(WIFI_AUTH_OPEN);
    WiFi.setSleep(WIFI_PS_NONE);

    WiFi.begin(WiFiSSID, WiFiPass);
    Serial.println("WiFiSetup:");
    Serial.print("    Mac Address:");
    Serial.println(WiFi.macAddress());
    Serial.print("    Hostname:");
    Serial.println(newHostName);
}

void setup(){
    //Setup serial comm
    Serial.begin(115200);
    Serial.println("Initializing...");

    if (!bme.begin()){
        Serial.println(F("Could not find a valid BME280 sensor #1, check wiring!"));
    }
    if (!bme2.begin()){
        Serial.println(F("Could not find a valid BME280 sensor #2, check wiring!"));
    }
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X4, // temperature
                    Adafruit_BME280::SAMPLING_X4, // pressure
                    Adafruit_BME280::SAMPLING_X4, // humidity
                    Adafruit_BME280::FILTER_X2   );
    bme2.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X4, // temperature
                    Adafruit_BME280::SAMPLING_X4, // pressure
                    Adafruit_BME280::SAMPLING_X4, // humidity
                    Adafruit_BME280::FILTER_X2   );


    //Setup time
    configTime(0, 0, ntpServer1, ntpServer2, ntpServer3);  // 0, 0 because we will use TZ in the next line
    setenv("TZ", timeZone, 1);            // Set environment variable with your time zone
    tzset();

    //Setup non volatile storage
    StorageData defaultStorage;
    defaultStorage.red=255;
    defaultStorage.green=255;
    defaultStorage.blue=255;
    defaultStorage.lightOn=true;
    defaultStorage.startTime={20, 0, 0};
    defaultStorage.endTime={21, 0, 0};
    initStorage(&defaultStorage, storageData);

    //Setup WiFi
    WiFiSetup(false);

    //Setup NetClient
    NetClient.setPacketReceivedCallback(&packetReceived);
    NetClient.setOnConnected(&onConnected);
    NetClient.setOnDisconnected(&onDisconnected);
}

void loop(){
    static uint32_t lastConnectTime=0;
    static uint8_t failReconnects=0;

    static uint32_t lastReadyTime=0;
    static uint32_t lastWeatherSendTime=0;
    static uint32_t lastLogTime=0;


    uint32_t currentTime = millis();

    if (WiFi.status() != WL_CONNECTED){//Reconnect to WiFi
        if (isTimeToExecute(lastConnectTime, 2000)){
            Serial.println("Waiting for autoreconnect...");
            failReconnects++;
            if (failReconnects>30){
                Serial.println("Autoreconnect failed, generating new MAC and retrying...");
                failReconnects=0;
                WiFiSetup(true);
            }
        }
    }else{
        failReconnects=0;
        if (NetClient.loop()){
            lastReadyTime=currentTime;

            if (isTimeToExecute(lastWeatherSendTime, weatherPeriod)){
                bme.takeForcedMeasurement();
                float humidity = bme.readHumidity();
                float temperature = bme.readTemperature()*1.8f+32.0f;
                NetClient.sendString(String("humidity=")+String(humidity, 1));
                NetClient.sendString(String("temperature=")+String(temperature, 1));

                bme2.takeForcedMeasurement();
                float humidity2 = bme2.readHumidity();
                float temperature2 = bme2.readTemperature()*1.8f+32.0f;
                NetClient.sendString(String("humidity2=")+String(humidity2, 1));
                NetClient.sendString(String("temperature2=")+String(temperature2, 1));

                struct tm timeinfo;
                if(getLocalTime(&timeinfo, 0)){
                    NetClient.sendString(String("currentTime=")+String(timeinfo.tm_hour)+String(":")+String(timeinfo.tm_min)+":"+String(timeinfo.tm_sec));
                }

                DanTime currentTime={timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec};
                if (isInTimeWindow(&currentTime, &storageData.startTime, &storageData.endTime)){
                    NetClient.sendString(String("inTimeWindow=We are in the time window"));
                }else{
                    NetClient.sendString(String("inTimeWindow=Not in the time window"));
                }
            }
        }
    }

    if (currentTime-lastReadyTime > howLongBeforeRestartIfNotConnecting || lastReadyTime>currentTime){//Crude edge case handling, if overflow, just restart
        ESP.restart();
    }

    if (storageData.lightOn){
        neopixelWrite(RGB_BUILTIN, storageData.red, storageData.green, storageData.blue);
    }else{
        neopixelWrite(RGB_BUILTIN, 0, 0, 0);
    }
}