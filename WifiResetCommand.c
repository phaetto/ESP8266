
#include "../Commands/commands.h"
#include "ESP8266.h"
#include "WifiGetCommand.h"

extern Service DefaultWifiService;

static byte* WifiResetCommandImplementation(const char* args[], struct CommandEngine* commandEngine)
{
    ResetWifiModule(&DefaultWifiService);

    return CMD_CRLF "Wifi is resetting.." CMD_CRLF;
}

const Command WifiResetCommand = {
    "wifi-reset",
    WifiResetCommandImplementation,
    "Resets the wifi services"
};