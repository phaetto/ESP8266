
#include "../Commands/commands.h"
#include "ESP8266.h"

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

    tcpRequest->IsSending = 0;

    for(i = 0; i < 5; ++i)
    {
        if (data->ActiveRequests[i] == NULL && data->ActiveRequests[i] != tcpRequest)
        {
            data->ActiveRequests[i] = tcpRequest;
            return;
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
    EmptyBuffer(GetWifiServiceData(service->Data));
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
            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL
                    || strstr (wifiServiceData->WifiBuffer, "busy now") != NULL)
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
            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x02;
            }

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
        // Connec [OK]
        case 0x05:
            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x04;
            }

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
            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x06;
            }

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
            //  - Checks if there is a response coming in from the module

            if (strstr (wifiServiceData->WifiBuffer, "+IPD") != NULL)
            {
                return 0x0D;
            }

            for(i = 0; i < MAX_REQUESTS_SUPPORTED; ++i)
            {
                if (wifiServiceData->ActiveRequests[i] != NULL && !wifiServiceData->ActiveRequests[i]->IsSending)
                {
                    wifiServiceData->CurrentRequest = wifiServiceData->ActiveRequests[i];
                    wifiServiceData->CurrentRequest->RequestId = i;
                    break;
                }
            }

            if (wifiServiceData->CurrentRequest == NULL)
            {
                return 0x08;
            }

            sprintf(formattedString, "AT+CIPSTART=%d,\"TCP\",\"%s\",%d" CMD_CRLF,
                wifiServiceData->CurrentRequest->RequestId,
                wifiServiceData->CurrentRequest->Hostname,
                wifiServiceData->CurrentRequest->Port);
            wifiServiceData->WifiWriteString(formattedString);
            return 0x09;
        // API Connection [OK]
        case 0x09:
            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL
                    || strstr (wifiServiceData->WifiBuffer, "DNS Fail") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x08;
            }

            if (strstr (wifiServiceData->WifiBuffer, "OK") == NULL)
            {
                return 0x09;
            }

            wifiServiceData->CurrentRequest->IsSending = 1;
            EmptyBuffer(wifiServiceData);

            return 0x0A;
        // Send http request
        case 0x0A:
            sprintf(formattedString, "AT+CIPSEND=%d,%d" CMD_CRLF,
                    wifiServiceData->CurrentRequest->RequestId,
                    wifiServiceData->CurrentRequest->RequestSize);
            wifiServiceData->WifiWriteString(formattedString);
            return 0x0B;
        // Ready to send [>]
        case 0x0B:
            if (strstr (wifiServiceData->WifiBuffer, "wdt reset") != NULL)
            {
                return Starting;
            }

            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x0A;
            }

            if (strstr (wifiServiceData->WifiBuffer, ">") == NULL)
            {
                return 0x0B;
            }

            wifiServiceData->WifiWriteString(wifiServiceData->CurrentRequest->RequestData);
            wifiServiceData->WifiWriteString(CMD_CRLF);
            EmptyBuffer(wifiServiceData);

            return 0x0C;
        // Check if send [SEND OK]
        case 0x0C:
            if (strstr (wifiServiceData->WifiBuffer, "wdt reset") != NULL)
            {
                return Starting;
            }
            
            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x0A;
            }

            // Loop to make sure the connection succeeded
            if (strstr (wifiServiceData->WifiBuffer, "SEND OK") == NULL)
            {
                return 0x0C;
            }

            wifiServiceData->CurrentRequest = NULL;

            // Return back to the main loop
            return 0x08;
        // Getting response [+IPD / ending OK]
        case 0x0D:
            if (strstr (wifiServiceData->WifiBuffer, "ERROR") != NULL)
            {
                EmptyBuffer(wifiServiceData);
                return 0x0A;
            }

            byte* responseStart = strstr (wifiServiceData->WifiBuffer, "+IPD");
            byte* responseOkAfterStart = strstr (responseStart, "OK");
            if (responseOkAfterStart == NULL)
            {
                // We need to wait until the full response arrives
                return 0x0D;
            }

            // Decode the response and call the respective connection handler
            unsigned short connectionId;
            unsigned int responseSize;
            byte responseBuffer[2048] = { '\0' };
            // Format is: +IPD,[connection #],[response size]:[data] CMD_CRLF OK
            sscanf(responseStart, "+IPD,%hu,%u:", &connectionId, &responseSize);
            if (wifiServiceData->ActiveRequests[connectionId]->OnResponseReceived != NULL)
            {
                byte* tcpResponseStart = strstr (responseStart, ":");
                ++tcpResponseStart; // Move it one position forward
                strcpy(responseBuffer, tcpResponseStart);
                responseBuffer[responseSize] = '\0';

                wifiServiceData->ActiveRequests[connectionId]->OnResponseReceived(responseBuffer);
            }

            // Request has been finalized
            wifiServiceData->ActiveRequests[connectionId]->IsSending = 0;
            wifiServiceData->ActiveRequests[connectionId] = NULL;

            EmptyBuffer(wifiServiceData);

            return 0x08;
        // Close connection [On demand]
        case 0x10:
            sprintf(formattedString, "AT+CIPCLOSE=%d" CMD_CRLF, wifiServiceData->CurrentRequest->RequestId);
            wifiServiceData->WifiWriteString(formattedString);
            return 0x11;
        // Connection closed
        case 0x11:
            if (strstr (wifiServiceData->WifiBuffer, "unlinked") == NULL
                    || strstr (wifiServiceData->WifiBuffer, "link is not") == NULL)
            {
                return 0x11;
            }

            // Request has been finalized
            wifiServiceData->ActiveRequests[wifiServiceData->CurrentRequest->RequestId] = NULL;
            wifiServiceData->CurrentRequest = NULL;

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
