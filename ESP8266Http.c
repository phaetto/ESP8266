
#include "../Commands/commands.h"
#include "ESP8266.h"
#include "ESP8266Http.h"
#include <stddef.h>

extern void SendRequest(TcpRequest * tcpRequest, Service* service);
extern int sscanf(const char *, const char *, ...);
extern char * strstr(const char * searchee, const char * lookfor);
extern char * strcpy(char *, const char *);
extern size_t strlen(const char *);

static byte HttpBuffer[0xFFF];

void ParseTcpResponse(const char* responseBuffer, void * data)
{
    HttpRequest * httpRequest = (HttpRequest *) data;

    // The response might be coming in parts
    if (httpRequest->ContentLength == 0)
    {
        unsigned short responseCode = 0;
        sscanf(responseBuffer, "HTTP/1.1 %hu OK", &responseCode);

        if (responseCode != 200)
        {
            // No valid or unsuccessful http response
            return;
        }

        byte* contentLengthStart = strstr(responseBuffer, "Content-Length:");
        unsigned int contentLength = 0;
        sscanf(contentLengthStart, "Content-Length: %d", &contentLength);

        byte* responseAfterHttpHeaders = strstr(responseBuffer, CMD_CRLF CMD_CRLF);
        responseAfterHttpHeaders += 4; // Move after the 2xCRLFs

        httpRequest->ContentLength = contentLength;
        httpRequest->BytesReceived += strlen(responseAfterHttpHeaders);
        strcpy(HttpBuffer, responseAfterHttpHeaders);

        if (httpRequest->ContentLength == httpRequest->BytesReceived)
        {
            httpRequest->Http200ResponseReceived(HttpBuffer);
        }
    }
    else
    {
        // This is one after the first part
        strcpy(HttpBuffer + httpRequest->BytesReceived, responseBuffer);
        httpRequest->BytesReceived += strlen(responseBuffer);

        if (httpRequest->ContentLength == httpRequest->BytesReceived)
        {
            httpRequest->Http200ResponseReceived(HttpBuffer);
        }
    }
}

void SendHttpRequest(HttpRequest * httpRequest, Service* service)
{
    httpRequest->ContentLength = 0;
    httpRequest->BytesReceived = 0;
    httpRequest->Request->Data = httpRequest;
    httpRequest->Request->Connection->OnResponseReceived = ParseTcpResponse;
    SendRequest(httpRequest->Request, service);
}