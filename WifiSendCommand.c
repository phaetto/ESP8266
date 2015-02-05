
#include "../Commands/commands.h"
#include "ESP8266.h"
#include "WifiSendCommand.h"

extern WifiServiceData DefaultWifiServiceData;

static byte* WifiSendImplementation(const char* args[], struct CommandEngine* commandEngine)
{
    WriteString(CMD_CRLF);

    if (args[0] == NULL)
    {
        return "Needs at least one argument" CMD_CRLF;
    }

    DefaultWifiServiceData.WifiBufferCounter = 0;

    DefaultWifiServiceData.WifiWriteString(args[0]);
    DefaultWifiServiceData.WifiWriteString(CMD_CRLF);

    return CMD_CRLF;
}

const Command WifiSendCommand = {
    "wifi-send",
    WifiSendImplementation,
    "Send data directly with the wifi module. Example: wifi-send AT+RST"
};
