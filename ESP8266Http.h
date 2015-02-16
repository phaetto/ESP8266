
#ifndef ESP8266HTTP_H
#define	ESP8266HTTP_H

#include "ESP8266.h"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef HTTP_BUFFER_SIZE
#define HTTP_BUFFER_SIZE 0xFF
#endif

////////////////////////////////////////////////////////////////////////////////
// Module Classes
////////////////////////////////////////////////////////////////////////////////

struct HttpRequest;
typedef void (*OnHttpResponseReceived)(const char* responseBuffer);

typedef struct {
    TcpRequest* Request;
    OnHttpResponseReceived Http200ResponseReceived;
    // Private fields
    unsigned int ContentLength;
    unsigned int BytesReceived;
} HttpRequest;

////////////////////////////////////////////////////////////////////////////////
// Exposed services
////////////////////////////////////////////////////////////////////////////////

void SendHttpRequest(HttpRequest * httpRequest, Service* service);

#ifdef	__cplusplus
}
#endif

#endif	/* ESP8266HTTP_H */

