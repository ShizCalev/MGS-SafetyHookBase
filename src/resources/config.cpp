#include "common.hpp"
#include "config.hpp"

#include <inipp/inipp.h>

#include "logging.hpp"
#include "steamworks_api.hpp"
#include "version_checking.hpp"


void Config::Read()
{
    std::filesystem::path sConfigFile = sFixName + ".ini";

    std::ifstream iniFile((sExePath / sFixPath / sConfigFile).string());
    if (!iniFile)
    {
        spdlog::error("CONFIG ERROR: File not found: {}", (sExePath / sFixPath / sConfigFile).string());
        Logging::ShowConsole();
        std::cout << "" << sFixName << " v" << sFixVersion << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile << " is located in " << sExePath / sFixPath << std::endl;
        return FreeLibraryAndExitThread(baseModule, 1);
    }

    spdlog::info("Config file: {}", (sExePath / sFixPath / sConfigFile).string());

    inipp::Ini<char> ini;
    ini.parse(iniFile);
    if (!ini.errors.empty())
    {
        spdlog::error("Error parsing ini file, encountered {} errors at these lines:", ini.errors.size());
        Logging::ShowConsole();
        std::cout << "Error parsing ini file, encountered " << ini.errors.size() << " errors at these lines:" << std::endl;
        for (auto err : ini.errors)
        {
            spdlog::error(err);
            std::cout << err << std::endl;
        }
    }

    int loadedConfigVersion;
    inipp::get_value(ini.sections["Config Version"], "Version", loadedConfigVersion);
    if (loadedConfigVersion != iConfigVersion)
    {
        spdlog::error("CONFIG ERROR: Config file version mismatch! Expected version {}, but found version {}.", iConfigVersion, loadedConfigVersion);
        Logging::ShowConsole();
        std::cout << "" << sFixName << " v" << sFixVersion << " loaded." << std::endl;
        std::cout << "MGS-SafetyHookBase CONFIG ERROR: Outdated config file!" << std::endl;
        std::cout << "MGS-SafetyHookBase CONFIG ERROR: Please install -all- the files from the latest release!" << std::endl;
        return FreeLibraryAndExitThread(baseModule, 1);
    }

    // Grab desktop resolution
    //DesktopDimensions = Util::GetPhysicalDesktopDimensions();

    // Read ini file
    g_Logging.bVerboseLogging = Util::stringToBool(ini.sections["Verbose Logging"]["Enabled"]);
    bShouldCheckForUpdates = Util::stringToBool(ini.sections["Update Notifications"]["CheckForUpdates"]);
    bConsoleUpdateNotifications = Util::stringToBool(ini.sections["Update Notifications"]["ConsoleNotifications"]);

#if defined(USE_STEAMAPI)
    g_SteamAPI.bResetAchievements = Util::stringToBool(ini.sections["Reset All Achievements"]["Reset_All_Achievements"]);
#endif


    spdlog::info("Config Parse: Verbose Logging: {}", g_Logging.bVerboseLogging);
    spdlog::info("Config Parse: Check for mod updates: {}", bShouldCheckForUpdates);
    if (bShouldCheckForUpdates)
    {
        spdlog::info("Cofig Parse: Mod update console notifications: {}", bConsoleUpdateNotifications);
    }

#if defined(USE_STEAMAPI)
    spdlog::info("Config Parse: Reset Achievements: {}", g_SteamAPI.bResetAchievements);
#endif
}
