
#ifndef ESP8266_H
#define	ESP8266_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * TODO:
 * Add to github (maybe add on example project?)
 * Create example project
 * Create/test the tcp server
 * Remove sprintf usage
 */

////////////////////////////////////////////////////////////////////////////////
// Software options
////////////////////////////////////////////////////////////////////////////////

#define RECEIVE_BUFFER_SIZE             512

#define MAX_REQUESTS_SUPPORTED          5

////////////////////////////////////////////////////////////////////////////////
// Imported for the default services
////////////////////////////////////////////////////////////////////////////////

extern void DefaultWifiWriteString(const char* string);

////////////////////////////////////////////////////////////////////////////////
// Module Classes
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    const char* Ssid;
    const char* Pass;
} AccessPointConnection;

typedef void (*OnResponseReceived)(const char* responseBuffer);

typedef struct {
    const char* RequestData;
    const unsigned int RequestSize;
    const char* Hostname;
    const unsigned int Port;
    OnResponseReceived OnResponseReceived;
    // Private fields
    unsigned short RequestId;
    byte IsSending : 1;
} TcpRequest;

typedef void (*WifiUARTWriteString)(const char *string);

typedef struct {
    WifiUARTWriteString WifiWriteString;
    char WifiBuffer[RECEIVE_BUFFER_SIZE];
    unsigned short WifiBufferCounter;
    AccessPointConnection* AccessPointConnection;
    TcpRequest* ActiveRequests[MAX_REQUESTS_SUPPORTED];
    TcpRequest* CurrentRequest;
} WifiServiceData;

extern byte ServiceWifiImplementation(byte state, void* data, struct CommandEngine* commandEngine);

////////////////////////////////////////////////////////////////////////////////
// Exposed services
////////////////////////////////////////////////////////////////////////////////

extern Service DefaultWifiService;

extern void SendRequest(TcpRequest * tcpRequest, Service* service);
extern void ConnectToAccessPoint(AccessPointConnection* accessPointConnection, Service* service);
extern void DisconnectAccessPoint(Service* service);
extern void ResetWifiModule(Service* service);
extern void PutcToWifiReceivedBuffer(const char rchar, Service* service);

#ifdef	__cplusplus
}
#endif

#endif	/* ESP8266_H */

