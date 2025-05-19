#pragma once
#if defined(USE_STEAMAPI)
#include <optional>
#include <cstdint>
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
#pragma comment(lib,"steam_api64.lib")
#elif defined(_M_IX86) || defined(__i386__)
#pragma comment(lib,"steam_api.lib")
#endif


class SteamAPI final
{
private:
    static void OnSteamInitialized();
    static void FetchAndCacheSteamID();
    static void ResetAllAchievements();
    bool bInitialized = false;

public:
    void Setup() const;

    static void LogControllers();

    // Achievement-related wrappers
    [[nodiscard]] static bool SetAchievement(const char* achievementID);
    [[nodiscard]] static bool ClearAchievement(const char* achievementID);

    bool bIsLegitCopy = true;
    bool bIsOnline = true;
    std::optional<uint64_t> steamID;
    bool bResetAchievements = false;

};

inline SteamAPI g_SteamAPI;
#endif