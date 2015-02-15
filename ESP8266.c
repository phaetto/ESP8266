
#include "../Commands/commands.h"
#include "ESP8266.h"
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Buffers and global variables
////////////////////////////////////////////////////////////////////////////////

static char formattedString[64];

WifiServiceData DefaultWifiServiceData = {
    DefaultWifiWriteString
};

////////////////////////////////////////////////////////////////////////////////
// Wifi private api
////////////////////////////////////////////////////////////////////////////////

extern int sprintf(char *, const char *, ...);
extern int sscanf(const char *, const char *, ...);
extern char * strncpy(char *, const char *, size_t);
extern char * strcpy(char *, const char *);

static char * strstr(const char * searchee, const char * lookfor)
{
    if (*searchee == 0)
    {
        if (*lookfor)
            return (char *) NULL;
        return (char *) searchee;
    }

    while (*searchee)
    {
        int i;
        i = 0;

        while (1)
        {
            if (lookfor[i] == 0)
            {
                return (char *) searchee;
            }

            if (lookfor[i] != searchee[i])
            {
                break;
            }
            i++;
        }
        searchee++;
    }

    return (char *) NULL;
}

static WifiServiceData* GetWifiServiceData(Service* service)
{
    return (WifiServiceData*)service->Data;
}

static void EmptyBuffer(WifiServiceData* data)
{
    data->WifiBufferCounter = 0;
    data->WifiBuffer[0] = '\0';
}

static void AdvanceBuffer(WifiServiceData* data, byte* endingPattern)
{
    strcpy(data->WifiBuffer, endingPattern);
    unsigned int charactersLeft = endingPattern - data->WifiBuffer;
    data->WifiBufferCounter = data->WifiBufferCounter - charactersLeft;
    data->WifiBuffer[data->WifiBufferCounter] = '\0';
}

////////////////////////////////////////////////////////////////////////////////
// Wifi public api
////////////////////////////////////////////////////////////////////////////////

void PutcToWifiReceivedBuffer(const char rchar, Service* service)
{
    WifiServiceData* data = GetWifiServiceData(service);
    if (rchar == RETURN_ASCII && data->WifiBufferCounter < (RECEIVE_BUFFER_SIZE - 3))
    {
        data->WifiBuffer[data->WifiBufferCounter] = '\r';
        ++data->WifiBufferCounter;
        data->WifiBuffer[data->WifiBufferCounter] = '\n';
        ++data->WifiBufferCounter;
    }

    if (rchar > 31 && data->WifiBufferCounter < (RECEIVE_BUFFER_SIZE - 2) )
    {
        data->WifiBuffer[data->WifiBufferCounter] = rchar;
        ++data->WifiBufferCounter;
    }

    data->WifiBuffer[data->WifiBufferCounter] = '\0';
}

void SendRequest(TcpRequest * tcpRequest, Service* service)
{
    WifiServiceData* data = GetWifiServiceData(service);
    unsigned short i;

    for(i = 0; i < MAX_REQUESTS_SUPPORTED; ++i)
    {
        if (data->QueuedRequests[i] == NULL)
        {
            data->QueuedRequests[i] = tcpRequest;
            break;
        }
    }
}

void ConnectToAccessPoint(AccessPointConnection* accessPointConnection, Service* service)
{
    WifiServiceData* data = GetWifiServiceData(service);
    if (data->AccessPointConnection == NULL)
    {
        data->AccessPointConnection = accessPointConnection;
    }
}

void DisconnectAccessPoint(Service* service)
{
    WifiServiceData* data = GetWifiServiceData(service);
    if (data->AccessPointConnection != NULL)
    {
        data->AccessPointConnection = NULL;
        service->State = 0x04;
    }
}

void ResetWifiModule(Service* service)
{
    service->State = Starting;
    WifiServiceData* data = GetWifiServiceData(service);
    data->ActiveRequest->IsSending = 0;
    data->ActiveRequest->Connection->IsConnected = 0;
    EmptyBuffer(data);
}

////////////////////////////////////////////////////////////////////////////////
// Wifi service implementation
////////////////////////////////////////////////////////////////////////////////

byte ServiceWifiImplementation(byte state, void* data, struct CommandEngine* commandEngine)
{
    WifiServiceData* wifiServiceData = (WifiServiceData*)data;

    if (wifiServiceData->WifiWriteString == NULL)
    {
        return Stopped;
    }

    unsigned short i;

    switch(state)
    {
        case Starting:
            EmptyBuffer(wifiServiceData);
            // Turn on, make a reset
            wifiServiceData->WifiWriteString("AT+RST" CMD_CRLF);
            return 0x01;
        // Starting [ready]:
        case 0x01:
            if (strstr (wifiServiceData->WifiBuffer, "busy now") != NULL)
            {
                return Starting;
            }

            if (strstr (wifiServiceData->WifiBuffer, "ready") == NULL)
            {
                return 0x01;
            }

            EmptyBuffer(wifiServiceData);

            return 0x02;
        // Setup default configuration
        case 0x02:
            wifiServiceData->WifiWriteString("AT+CWMODE=3" CMD_CRLF);
            return 0x03;
        // Setup [OK]
        case 0x03:
            if (strstr (wifiServiceData->WifiBuffer, "OK") == NULL
                    && strstr (wifiServiceData->WifiBuffer, "no change") == NULL)
            {
                return 0x03;
            }

            EmptyBuffer(wifiServiceData);

            return 0x04;
        // Connect to wifi
        case 0x04:
            if (wifiServiceData->AccessPointConnection == NULL)
            {
                return 0x04;
            }

            wifiServiceData->WifiWriteString("AT+CWJAP=\"");
            wifiServiceData->WifiWriteString(wifiServiceData->AccessPointConnection->Ssid);
            wifiServiceData->WifiWriteString("\",\"");
            wifiServiceData->WifiWriteString(wifiServiceData->AccessPointConnection->Pass);
            wifiServiceData->WifiWriteString("\"" CMD_CRLF);
            return 0x05;
        // Connect [OK]
        case 0x05:
            if (strstr (wifiServiceData->WifiBuffer, "OK") == NULL)
            {
                return 0x05;
            }

            EmptyBuffer(wifiServiceData);

            return 0x06;
        // Multiple connections
        case 0x06:
            wifiServiceData->WifiWriteString("AT+CIPMUX=1" CMD_CRLF);
            return 0x07;
        //  Multiple connections [OK]
        case 0x07:
            if (strstr (wifiServiceData->WifiBuffer, "OK") == NULL)
            {
                return 0x07;
            }

            EmptyBuffer(wifiServiceData);

            return 0x08;
        // API connection - main service loop
        case 0x08:
            // Main loop:
            //  - Checks if there is a request and starts it if possible

            if (wifiServiceData->ActiveRequest == NULL
                    || wifiServiceData->ActiveRequest->IsSent)
            {
                for(i = 0; i < MAX_REQUESTS_SUPPORTED; ++i)
                {
                    if (wifiServiceData->QueuedRequests[i] != NULL)
                    {
                        if (wifiServiceData->ActiveRequest != NULL
                                && wifiServiceData->ActiveRequest->Connection->IsConnected
                                && wifiServiceData->ActiveRequest->Connection != wifiServiceData->QueuedRequests[i]->Connection)
                        {
                            EmptyBuffer(wifiServiceData);
                            return 0x20;
                        }

                        wifiServiceData->ActiveRequest = wifiServiceData->QueuedRequests[i];
                        wifiServiceData->QueuedRequests[i] = NULL;
                        wifiServiceData->ActiveRequest->IsSending = 0;
                        wifiServiceData->ActiveRequest->IsSent = 0;

                        if (wifiServiceData->ActiveRequest->Connection->IsConnected)
                        {
                            return 0x0A;
                        }

                        break;
                    }
                }
            }

            if (wifiServiceData->ActiveRequest == NULL
                    || wifiServiceData->ActiveRequest->IsSent)
            {
                return 0x08;
            }

            sprintf(formattedString, "AT+CIPSTART=%d,\"TCP\",\"%s\",%d" CMD_CRLF,
                wifiServiceData->ActiveRequest->Connection->Id,
                wifiServiceData->ActiveRequest->Connection->Hostname,
                wifiServiceData->ActiveRequest->Connection->Port);
            wifiServiceData->WifiWriteString(formattedString);
            return 0x09;
        // API Connection [OK]
        case 0x09:
            if (strstr (wifiServiceData->WifiBuffer, "wdt reset") != NULL)
            {
                return Starting;
            }

            if (strstr (wifiServiceData->WifiBuffer, CMD_CRLF "OK" CMD_CRLF "Unlink") != NULL)
            {
                // Connection dropped - this port might not be served
                return 0x08;
            }
            
            if (strstr (wifiServiceData->WifiBuffer, "DNS Fail") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x08;
            }

            if (strstr (wifiServiceData->WifiBuffer, CMD_CRLF "OK") == NULL
                    && strstr (wifiServiceData->WifiBuffer, CMD_CRLF "ALREAY CONNECT") == NULL)
            {
                return 0x09;
            }

            wifiServiceData->ActiveRequest->Connection->IsConnected = 1;
            //EmptyBuffer(wifiServiceData);

            return 0x0A;
        // Send http request
        case 0x0A:
            wifiServiceData->ActiveRequest->IsSending = 1;
            sprintf(formattedString, "AT+CIPSEND=%d,%d" CMD_CRLF,
                    wifiServiceData->ActiveRequest->Connection->Id,
                    wifiServiceData->ActiveRequest->RequestSize);
            wifiServiceData->WifiWriteString(formattedString);
            return 0x0B;
        // Ready to send [>]
        case 0x0B:
            if (strstr (wifiServiceData->WifiBuffer, "wdt reset") != NULL)
            {
                wifiServiceData->ActiveRequest->IsSending = 0;
                wifiServiceData->ActiveRequest->Connection->IsConnected = 0;
                EmptyBuffer(wifiServiceData);
                return Starting;
            }

            if (strstr (wifiServiceData->WifiBuffer, "link is not") != NULL)
            {
                // The connection is not up - we might need to reconnect
                wifiServiceData->ActiveRequest->IsSending = 0;
                wifiServiceData->ActiveRequest->Connection->IsConnected = 0;
                EmptyBuffer(wifiServiceData);
                return 0x08;
            }

            if (strstr (wifiServiceData->WifiBuffer, "> ") == NULL)
            {
                return 0x0B;
            }

            wifiServiceData->WifiWriteString(wifiServiceData->ActiveRequest->RequestData);
            //EmptyBuffer(wifiServiceData);

            return 0x0C;
        // Check if send [SEND OK]
        case 0x0C:
            if (strstr (wifiServiceData->WifiBuffer, "wdt reset") != NULL)
            {
                wifiServiceData->ActiveRequest->IsSending = 0;
                wifiServiceData->ActiveRequest->Connection->IsConnected = 0;
                EmptyBuffer(wifiServiceData);
                return Starting;
            }
            
            if (strstr (wifiServiceData->WifiBuffer, "Unlink") != NULL)
            {
                wifiServiceData->ActiveRequest->IsSending = 0;
                wifiServiceData->ActiveRequest->Connection->IsConnected = 0;
                EmptyBuffer(wifiServiceData);
                return 0x08;
            }

            // Loop to make sure the connection succeeded
            if (strstr (wifiServiceData->WifiBuffer, CMD_CRLF "SEND OK") == NULL)
            {
                return 0x0C;
            }

            wifiServiceData->Timeout = 0xFFFF;

            return 0x0D;
        // Getting response [+IPD / ending OK]
        case 0x0D:
        {
            byte* responseStart = strstr (wifiServiceData->WifiBuffer, "+IPD");
            if (responseStart == NULL)
            {
                --wifiServiceData->Timeout;

                if (wifiServiceData->Timeout == 0)
                {
                    // Resend response
                    return 0x0A;
                }

                // We need to wait until the response arrives
                return 0x0D;
            }

            byte* tcpResponseStart = strstr (responseStart, ":");
            if (tcpResponseStart == NULL)
            {
                // We need to wait until the full header arrives
                return 0x0D;
            }

            ++tcpResponseStart; // Move it one position forward

            // Decode the response and call the respective connection handler
            unsigned short connectionId;
            unsigned int responseSize;
            byte responseBuffer[RECEIVE_BUFFER_SIZE];
            // Format is: +IPD,[connection #],[response size]:[data] CMD_CRLF OK
            sscanf(responseStart, "+IPD,%hu,%u:", &connectionId, &responseSize);

            unsigned int bytesFromStart = (tcpResponseStart - wifiServiceData->WifiBuffer);
            if (wifiServiceData->WifiBufferCounter < (responseSize + bytesFromStart))
            {
                // Response is not yet full on buffer
                return 0x0D;
            }

            if (wifiServiceData->ActiveRequest->Connection->OnResponseReceived != NULL)
            {
                strncpy(responseBuffer, tcpResponseStart, responseSize);
                responseBuffer[responseSize] = '\0';

                wifiServiceData->ActiveRequest->Connection->OnResponseReceived(responseBuffer);
            }

            tcpResponseStart += (responseSize + 4); // Response + CRLF + "OK"
            AdvanceBuffer(wifiServiceData, tcpResponseStart);

            if (strstr (wifiServiceData->WifiBuffer, "+IPD") != NULL)
            {
                return 0x0D;
            }

            // Request has been finalized
            wifiServiceData->ActiveRequest->IsSending = 0;
            wifiServiceData->ActiveRequest->IsSent = 1;

            return 0x08;
        }
        // Close connection
        case 0x20:
            sprintf(formattedString, "AT+CIPCLOSE=%d" CMD_CRLF, wifiServiceData->CurrentRequestId);
            wifiServiceData->WifiWriteString(formattedString);
            return 0x21;
        // Connection closed
        case 0x21:
            if (strstr (wifiServiceData->WifiBuffer, CMD_CRLF "OK" CMD_CRLF "Unlink") == NULL
                    && strstr (wifiServiceData->WifiBuffer, CMD_CRLF "link is not") == NULL)
            {
                return 0x21;
            }

            // Request has been finalized
            wifiServiceData->ActiveRequest->Connection->IsConnected = 0;

            EmptyBuffer(wifiServiceData);

            return 0x08;
        // Stop
        default:
            break;
    }

    return Stopped;
}

Service DefaultWifiService = {
    "Wifi service (default)",
    "Default service for background wifi orchestration",
    ServiceWifiImplementation,
    Starting,
    (void*)&DefaultWifiServiceData
};
