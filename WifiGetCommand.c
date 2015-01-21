
#include "../Commands/commands.h"
#include "ESP8266.h"
#include "WifiGetCommand.h"

extern char wifiBuffer[RECEIVE_BUFFER_SIZE];
extern unsigned short wifiBufferCounter;
extern WifiServiceData DefaultWifiServiceData;

static byte* WifiGetImplementation(const char* args[], struct CommandEngine* commandEngine)
{
    WriteString(CMD_CRLF);

    if (DefaultWifiServiceData.WifiBufferCounter != 0)
    {
        unsigned short i = 0;
        while(DefaultWifiServiceData.WifiBuffer[i] != '\0' && i < RECEIVE_BUFFER_SIZE)
        {
            PutCharacter(DefaultWifiServiceData.WifiBuffer[i]);
            ++i;
        }

        if (args[0] != NULL && strcmp(args[0], "--clear") == 0)
        {
            DefaultWifiServiceData.WifiBufferCounter = 0;
            DefaultWifiServiceData.WifiBuffer[0] = '\0';
        }
    }

    return CMD_CRLF;
}

const Command WifiGetCommand = {
    "wifi-get",
    WifiGetImplementation,
    "Get data from buffer and optionally clear it. Usage: wifi-get [--clear]"
};