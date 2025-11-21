#pragma once
#include <WiFiClient.h>
#include <vector>
#include "utils.h"

enum NETSTATUS {
    NOTHING,
    INITIAL_SENT,
    READY
};

enum RECVSTATE {
    LEN1,
    LEN2,
    LEN3,
    LEN4,
    PAYLOAD
};

enum VALUETYPE {
    INT32,
    DOUBLE,
    BOOL,
    TIME,
    COLOR,
    STRING
};

typedef struct {
    String device;
    String valueName;
    VALUETYPE valueType;

    union {
        int32_t int32Val;
        double doubleVal;
        bool boolVal;
        DanTime timeVal;
        DanColor colorVal;
    } value;

    String stringVal;

} ValueSubscription;


class Net {
    public:
        Net(String deviceName, String encroKey, String address, uint16_t port);
        ~Net();
        bool loop();

        bool sendString(String str);
        bool sendBinary(uint8_t* data, uint32_t dataLen);
        
        bool ready();
        
        void subscribeToValue(String deviceName, String valueName, VALUETYPE valueType);
        void unsubscribeToValue(String deviceName, String valueName);

        void setPacketReceivedCallback(void (*packetReceivedCallback)(uint8_t*, uint32_t));
        void setOnConnected(void (*onConnected)(void));
        void setOnDisconnected(void (*onDisconnected)(void));
        void setOnValueUpdate(void (*onValueUpdate)(ValueSubscription*));

    private:
        WiFiClient Client;
        String deviceName;
        uint8_t encroKey[32];
        String hostAddress;
        uint16_t port;

        uint32_t clientsHandshake;
        uint32_t serversHandshake;

        NETSTATUS netStatus;
        RECVSTATE recvState;
        uint32_t packetLength;
        uint8_t* packetPayload=nullptr;
        uint32_t payloadRecvdCount=0;

        const uint32_t connectAttemptInterval=2000;
        uint32_t lastConnectAttempt=0;

        bool wasConnected=false;

        std::vector<ValueSubscription> subscriptions;
        ValueSubscription* findSubscription(const String& device, const String& valueName);

        void (*packetReceived)(uint8_t* data, uint32_t dataLength)=nullptr;
        void (*onConnected)()=nullptr;
        void (*onDisconnected)()=nullptr;
        void (*onValueUpdate)(ValueSubscription*) = nullptr;

    private:
        void errorOccured(String errorText);
        void attemptToConnect();
        bool sendPacket(uint8_t* data, uint32_t dataLength);
        void byteReceived(uint8_t data);
        void packetRecieved(uint32_t recvdHandshake, uint8_t* data, uint32_t dataLength);
        void processIncoming();
        void handleIncomingSubscriptionPacket(uint8_t* data, uint32_t len);
        void valueUpdateRecieved(uint8_t* data, uint32_t dataLength);
};