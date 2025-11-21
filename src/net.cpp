#include <Arduino.h>
#include <Esp.h>
#include "encro.h"
#include "net.h"
#include "utils.h"
#include "parse.h"

Net::Net(String deviceName, String encroKeyString, String address, uint16_t port){
    this->deviceName=deviceName;
    this->hostAddress=address;
    this->port=port;

    buildKeyFromString(encroKeyString.c_str(), encroKey);
}

Net::~Net(){
    Client.stop();

    if (packetPayload){
        free(packetPayload);
        packetPayload=nullptr;
    }
}


void Net::errorOccured(String errorText){
    Client.stop();
    
    if (packetPayload){
        free(packetPayload);
        packetPayload=nullptr;
    }

    Serial.print("Net error occurred: ");
    Serial.println(errorText);
    if (onDisconnected) onDisconnected();
}

void Net::attemptToConnect(){
    if (!this->Client.connected()){
        if (wasConnected){
            wasConnected=false;
            if (onDisconnected) onDisconnected();
        }
        this->netStatus=NETSTATUS::NOTHING;
        this->recvState=RECVSTATE::LEN1;
        if (isTimeToExecute(this->lastConnectAttempt, connectAttemptInterval)){
            Serial.println("Attempting to connect to server...");
            if (this->Client.connect(this->hostAddress.c_str(), this->port)){
                delay(500);
                this->clientsHandshake=esp_random();

                this->Client.write((uint8_t)this->deviceName.length());
                this->Client.write(this->deviceName.c_str());
                if (sendPacket(nullptr, 0)){
                    this->netStatus=NETSTATUS::INITIAL_SENT;
                }else{
                    this->Client.stop();
                }
            }
        }
    }
}

void Net::setPacketReceivedCallback(void (*callback)(uint8_t*, uint32_t)){
    packetReceived=callback;
}

void Net::setOnConnected(void (*callback)(void)){
    onConnected=callback;
}
void Net::setOnDisconnected(void (*callback)(void)){
    onDisconnected=callback;
}

void Net::setOnValueUpdate(void (*callback)(ValueSubscription*)) {
    onValueUpdate = callback;
}

void Net::subscribeToValue(String deviceName, String valueName, VALUETYPE valueType) {
    if (findSubscription(deviceName, valueName))
        return; // Already subscribed

    ValueSubscription sub;
    sub.device = deviceName;      // store original casing
    sub.valueName = valueName;
    sub.valueType = valueType;

    sub.device.trim();
    sub.device.toLowerCase();
    sub.valueName.trim();
    sub.valueName.toLowerCase();

    subscriptions.push_back(sub);

    if (ready()){
        String msg = "subscribe:" + deviceName + ":" + valueName;
        sendString(msg);
    }
}

void Net::unsubscribeToValue(String deviceName, String valueName) {
    String fmtDeviceName = deviceName;
    String fmtValueName = valueName;
    
    fmtDeviceName.trim();
    fmtDeviceName.toLowerCase();
    fmtValueName.trim();
    fmtValueName.toLowerCase();

    for (auto it = subscriptions.begin(); it != subscriptions.end(); ++it) {
        if (it->device.equals(fmtDeviceName) && it->valueName.equals(valueName)) {
            subscriptions.erase(it);

            if (ready()){
                String msg = "unsubscribe:" + deviceName + ":" + valueName;
                sendString(msg);
            }
            return;
        }
    }
}

ValueSubscription* Net::findSubscription(const String& device, const String& valName) {
    String deviceName = device;
    String valueName = valName;
    deviceName.trim();
    deviceName.toLowerCase();
    valueName.trim();
    valueName.toLowerCase();
    for (auto &sub : subscriptions) {
        if (sub.device.equals(device) &&
            sub.valueName.equals(valueName)) {
            return &sub;
        }
    }
    return nullptr;
}

bool Net::sendString(String str){
    if (str){
        return sendPacket((uint8_t*)str.c_str(), str.length());
    }else{
        return false;
    }
}

bool Net::sendBinary(uint8_t* data, uint32_t dataLength){
    return sendPacket(data, dataLength);
}

bool Net::ready(){
    return (netStatus==NETSTATUS::READY) && Client.connected();
}

bool Net::sendPacket(uint8_t* data, uint32_t dataLength){
    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt(this->clientsHandshake, data, dataLength, encryptedLength, this->encroKey);
    if (encrypted){
        bool didntFail=true;
        this->clientsHandshake++;
        if (this->Client.write((uint8_t*)&encryptedLength, 4)!=4){
            errorOccured("sendPacket failed to send all the bytes");
            didntFail=false;
        };
        if (this->Client.write(encrypted, encryptedLength)!=encryptedLength){
            errorOccured("sendPacket failed to send all the bytes");
            didntFail=false;
        };
        free(encrypted);
        encrypted=nullptr;
        return didntFail;
    }
    return false;
}

void Net::valueUpdateRecieved(uint8_t* data, uint32_t dataLength){
    DanParser parser;
    String deviceName, valueName;

    parser.setInputText(String(data, dataLength));

    if (!parser.getName(deviceName)) return;
    if (!parser.match(':')) return;
    if (!parser.getName(valueName)) return;
    if (!parser.match('=')) return;

    deviceName.toLowerCase();
    deviceName.trim();
    valueName.toLowerCase();
    valueName.trim();

    ValueSubscription* valueSubscription = findSubscription(deviceName, valueName);
    if (valueSubscription){
        int32_t int32Val;
        double doubleVal;
        DanColor colorVal;
        DanTime timeVal;
        
        switch (valueSubscription->valueType){
            case VALUETYPE::BOOL:
                if (!parser.getInt32(int32Val)) return;
                valueSubscription->value.boolVal=(bool)int32Val;
                break;
            case VALUETYPE::COLOR:
                if (!parser.getInt32(int32Val)) return;
                if (!parser.match(',')) return;
                colorVal.red=int32Val;
                if (!parser.getInt32(int32Val)) return;
                if (!parser.match(',')) return;
                colorVal.green=int32Val;
                if (!parser.getInt32(int32Val)) return;
                colorVal.blue=int32Val;
                valueSubscription->value.colorVal=colorVal;
                break;
            case VALUETYPE::TIME:
                if (!parser.getInt32(int32Val)) return;
                if (!parser.match(':')) return;
                timeVal.hours=int32Val;
                if (!parser.getInt32(int32Val)) return;
                if (!parser.match(':')) return;
                timeVal.minutes=int32Val;
                if (!parser.getInt32(int32Val)) return;
                timeVal.seconds=int32Val;
                valueSubscription->value.timeVal=timeVal;
                break;
            case VALUETYPE::INT32:
                if (!parser.getInt32(int32Val)) return;
                valueSubscription->value.int32Val=int32Val;
                break;
            case VALUETYPE::DOUBLE:
                if (!parser.getDouble(doubleVal)) return;
                valueSubscription->value.doubleVal=doubleVal;
                break;
            case VALUETYPE::STRING:
                if (!parser.getText(valueSubscription->stringVal)) return;
                break;
            default:
                return;
        }
        if (onValueUpdate) onValueUpdate(valueSubscription);
    }
}

void Net::packetRecieved(uint32_t recvdHandshake, uint8_t* data, uint32_t dataLength){
    if (netStatus==NETSTATUS::INITIAL_SENT){
            serversHandshake=recvdHandshake+1;
            netStatus=NETSTATUS::READY;
            wasConnected=true;
            if (onConnected) onConnected();

            for (auto it = subscriptions.begin(); it != subscriptions.end(); ++it){
                String msg = "subscribe:" + it->device + ":" + it->valueName;
                sendString(msg);
            }

    }else if (netStatus==NETSTATUS::READY){
        if (recvdHandshake==serversHandshake){
            serversHandshake++;
            if (data && data[0]==0xFF){
                valueUpdateRecieved(data+1, dataLength-1);
            }else{
                if (packetReceived) packetReceived(data, dataLength);
            }
        }else{
            //throw error, wrong handshake from expected
            String errorText="Wrong handshake, expected ";
            errorText+=String(serversHandshake)+" but recvd "+String(recvdHandshake);
            errorOccured(errorText);
            return;
        }
    }else{
        errorOccured("Unknown netStatus");
    }
}

void Net::byteReceived(uint8_t data){
    switch (recvState){
        case RECVSTATE::LEN1:
            packetLength=data;
            recvState=RECVSTATE::LEN2;
            break;
        case RECVSTATE::LEN2:
            packetLength|=(data<<8);
            recvState=RECVSTATE::LEN3;
            break;
        case RECVSTATE::LEN3:
            packetLength|=(data<<16);
            recvState=RECVSTATE::LEN4;
            break;
        case RECVSTATE::LEN4:
            packetLength|=(data<<24);
            payloadRecvdCount=0;
            if (packetPayload){
                free(packetPayload);
                packetPayload=nullptr;
            }
            if (packetLength==0){
                recvState=RECVSTATE::LEN1;
            }else{
                recvState=RECVSTATE::PAYLOAD;
                packetPayload=(uint8_t*)malloc(packetLength);
                if (!packetPayload){
                    errorOccured("Failed to allocate packet payload space");
                }
            }
            break;
        case RECVSTATE::PAYLOAD:
            packetPayload[payloadRecvdCount]=data;
            payloadRecvdCount++;
            if (payloadRecvdCount>=packetLength){
                recvState=RECVSTATE::LEN1;
                uint32_t recvdHandshake=0;
                uint32_t decryptedLength=0;
                bool errorOccurred=false;
                uint8_t* plainText=decrypt(recvdHandshake, packetPayload, packetLength, decryptedLength, encroKey, errorOccurred);

                if (errorOccurred){
                    errorOccured("Error occured decrypting payload");
                }else{
                    packetRecieved(recvdHandshake, plainText, decryptedLength);
                }
                if (packetPayload){
                    free(packetPayload);
                    packetPayload=nullptr;
                }
            }
            break;
        default:
            errorOccured("Unknown recvState");
            break;

    }
}

void Net::processIncoming(){
    while (this->Client.connected() && Client.available()>0){
        this->byteReceived(this->Client.read());
    }
}

bool Net::loop(){
    this->attemptToConnect();
    this->processIncoming();
    return ready();
}
