#include "steam/steam_api.h"
#include <iostream>
#include "steam_helper.h"
#include "logger.h"

constexpr int AppID = 1172620; // test ID

// steam_appid.txt

bool InitSteam()
{
    // 1. Check if the user is running the game through Steam
    /* if (SteamAPI_RestartAppIfNecessary(AppID))
     {
         LOGE("game already running");
         return false; // Exit app and let Steam restart it
     }
         */

    // 2. Initialize the API
    if (!SteamAPI_Init())
    {
        LOGE("SteamAPI_Init() failed! Is Steam running?");
        return false;
    }

    // 3. Simple Test: Get current user's name
    const char *name = SteamFriends()->GetPersonaName();
    LOGI("Connected to Steam as: %s", name);

    // Verify ownership
    if (!SteamApps()->BIsSubscribed())
    {
        LOGE("User does not own this game.");
        SteamAPI_Shutdown();
        return false;
    }

    return true;
}

// Don't forget to call this when the game closes
void ShutdownSteam()
{
    SteamAPI_Shutdown();
}
