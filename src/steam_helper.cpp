#include "steam/steam_api.h"
#include <iostream>
#include "steam_helper.h"
#include "logger.h"

constexpr int AppID = 480; // test ID

// steam_appid.txt

bool InitSteam()
{
    // 1. Check if the user is running the game through Steam
    if (SteamAPI_RestartAppIfNecessary(AppID))
    {
        return false; // Exit app and let Steam restart it
    }

    // 2. Initialize the API
    if (!SteamAPI_Init())
    {
        std::cerr << "SteamAPI_Init() failed! Is Steam running?" << std::endl;
        return false;
    }

    // 3. Simple Test: Get current user's name
    const char *name = SteamFriends()->GetPersonaName();
    std::cout << "Connected to Steam as: " << name << std::endl;

    return true;
}

// Don't forget to call this when the game closes
void ShutdownSteam()
{
    SteamAPI_Shutdown();
}
