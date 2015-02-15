
#ifndef ESP8266_H
#define	ESP8266_H

#ifdef	__cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Software options
////////////////////////////////////////////////////////////////////////////////

#ifndef RECEIVE_BUFFER_SIZE
#define RECEIVE_BUFFER_SIZE                     512
#endif

#ifndef MAX_REQUESTS_SUPPORTED
#define MAX_REQUESTS_SUPPORTED                  5
#endif

#ifndef TIMEOUT_SERVICE_CYCLES
#define TIMEOUT_SERVICE_CYCLES                  0xFFF
#endif

#ifndef RESPONSE_TIMEOUT_SERVICE_CYCLES
#define RESPONSE_TIMEOUT_SERVICE_CYCLES          0xFF
#endif

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

typedef void (*OnResponseReceived)(const char* responseBuffer, void * data);

typedef struct {
    const char* Hostname;
    const unsigned int Port;
    OnResponseReceived OnResponseReceived;
    // Private fields
    unsigned short Id;
    byte IsConnected : 1;
} TcpConnection;

typedef struct {
    const char* RequestData;
    unsigned int RequestSize;
    TcpConnection* Connection;
    // Private fields
    void * Data;
    byte IsSending : 1;
    byte IsSent : 1;
} TcpRequest;

typedef void (*WifiUARTWriteString)(const char *string);

typedef struct {
    WifiUARTWriteString WifiWriteString;
    byte WifiBuffer[RECEIVE_BUFFER_SIZE];
    unsigned short WifiBufferCounter;
    AccessPointConnection* AccessPointConnection;
    TcpRequest* ActiveRequest;
    TcpRequest* QueuedRequests[MAX_REQUESTS_SUPPORTED];
    unsigned short CurrentRequestId;
    unsigned int Timeout;
} WifiServiceData;

extern byte ServiceWifiImplementation(byte state, void* data, struct CommandEngine* commandEngine);

////////////////////////////////////////////////////////////////////////////////
// Exposed services
////////////////////////////////////////////////////////////////////////////////

extern Service DefaultWifiService;

void SendRequest(TcpRequest * tcpRequest, Service* service);
void ConnectToAccessPoint(AccessPointConnection* accessPointConnection, Service* service);
void DisconnectAccessPoint(Service* service);
void ResetWifiModule(Service* service);
void PutcToWifiReceivedBuffer(const char rchar, Service* service);

#ifdef	__cplusplus
}
#endif

#endif	/* ESP8266_H */

