#include "stdafx.h"
#include "helper.hpp"
#include <string>
#include <string_view>

#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <safetyhook.hpp>


//Only utilize for primary game fix mods, don't bother logging all this stuff if we're just a standard mod since it just bloats the log file.
#ifndef PRIMARY_FIX_MOD
//#define PRIMARY_FIX_MOD
#endif


HMODULE baseModule = GetModuleHandle(NULL);

// Version
std::string sFixName = "MGS-SafetyHookBase";
std::string sFixVer = "1.0.0";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::filesystem::path sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;
std::filesystem::path sFixPath;


// Ini
inipp::Ini<char> ini;
std::filesystem::path sConfigFile = sFixName + ".ini";


// Ini Variables
bool bAspectFix;

struct GameInfo
{
    std::string GameTitle;
    std::string ExeName;
    int SteamAppId;
};

enum MgsGame : std::uint8_t
{
    NONE = 0,
    MGS2 = 1 << 0,
    MGS3 = 1 << 1,
    MG = 1 << 2,
    LAUNCHER = 1 << 3,
    UNKNOWN = 1 << 4
};

const std::map<MgsGame, GameInfo> kGames = {
    {MGS2, {"Metal Gear Solid 2 HD", "METAL GEAR SOLID2.exe", 2131640}},
    {MGS3, {"Metal Gear Solid 3 HD", "METAL GEAR SOLID3.exe", 2131650}},
    {MG, {"Metal Gear / Metal Gear 2 (MSX)", "METAL GEAR.exe", 2131680}},
};


const GameInfo* game = nullptr;
MgsGame eGameType = UNKNOWN;

#pragma region Logging

#pragma region Spdlog sink
// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size) {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open()) {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (std::filesystem::exists(_filename) && std::filesystem::file_size(_filename) >= _max_size) {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file() {
        if (std::filesystem::exists(_filename)) {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

#pragma endregion Spdlog sink

void Logging()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    std::array<std::string, 4> paths = { "", "plugins", "scripts", "update" };
    for (const auto& path : paths)
    {
        if (std::filesystem::exists(sExePath / path / (sFixName + ".asi")))
        {
            if (!sFixPath.empty()) { //multiple versions found
                AllocConsole();
                FILE* dummy;
                freopen_s(&dummy, "CONOUT$", "w", stdout);
                std::cout << "\n" << sFixName + " ERROR: Duplicate .asi installations found! Please make sure to delete any old versions!" << std::endl;
                FreeLibraryAndExitThread(baseModule, 1);
            }
            sFixPath = path;
        }
    }
    // spdlog initialisation
    {
        try {
            bool logDirExists = std::filesystem::is_directory(sExePath / "logs");
            if (!logDirExists)
            {
                std::filesystem::create_directory(sExePath / "logs"); //create a "logs" subdirectory in the game folder to keep the main directory tidy.
            }
            // Create 10MB truncated logger
            logger = std::make_shared<spdlog::logger>(sLogFile.string(), std::make_shared<size_limited_sink<std::mutex>>((sExePath / "logs" / sLogFile).string(), 10 * 1024 * 1024));
            spdlog::set_default_logger(logger);

            spdlog::flush_on(spdlog::level::debug);
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            if (!logDirExists)
            {
                spdlog::info("----------");
                spdlog::info("New log subdirectory created.");
            }
            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", sFixName, sFixVer); 
            spdlog::info("ASI plugin location: {}", (sExePath / sFixPath / (sFixName + ".asi")).string());
            spdlog::info("----------");
            spdlog::info("Log file: {}", (sExePath / "logs" / sLogFile).string());
            spdlog::info("----------");

            // Log module details
            spdlog::info("Module Name: {0:s}", sExeName.c_str());
            spdlog::info("Module Path: {0:s}", sExePath.string());
            spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
            spdlog::info("Module Version: {}", Memory::GetModuleVersion(baseModule));
        }
        catch (const spdlog::spdlog_ex& ex) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(baseModule, 1);
        }
    }
}
#pragma endregion Logging

#ifdef PRIMARY_FIX_MOD
///Prints CPU, GPU, and RAM info to the log to expedite common troubleshooting.
void Init_LogSysInfo()
{
#ifndef _WIN32
    spdlog::info("System Details - Steam Deck/Linux");
    return;
#endif


    std::array<int, 4> integerBuffer = {};
    constexpr size_t sizeofIntegerBuffer = sizeof(int) * integerBuffer.size();
    std::array<char, 64> charBuffer = {};
    std::array<std::uint32_t, 3> functionIds = {
        0x8000'0002, // Manufacturer  
        0x8000'0003, // Model 
        0x8000'0004  // Clock-speed
    };

    std::string cpu;
    for (int id : functionIds)
    {
        __cpuid(integerBuffer.data(), id);
        std::memcpy(charBuffer.data(), integerBuffer.data(), sizeofIntegerBuffer);
        cpu += std::string(charBuffer.data());
    }

    spdlog::info("System Details - CPU: {}", cpu);

    std::string deviceString;
    for (int i = 0; ; i++)
    {
        DISPLAY_DEVICE dd = { sizeof(dd), 0 };
        BOOL f = EnumDisplayDevices(NULL, i, &dd, EDD_GET_DEVICE_INTERFACE_NAME);
        if (!f)
        {
            break; //that's all, folks.
        }
        char deviceStringBuffer[128];
        WideCharToMultiByte(CP_UTF8, 0, dd.DeviceString, -1, deviceStringBuffer, sizeof(deviceStringBuffer), NULL, NULL);
        if (deviceString == deviceStringBuffer) //each monitor reports what gpu is driving it, lets just double check in case we're looking at a laptop with mixed usage.
        {
            continue;
        }
        deviceString = deviceStringBuffer;
        spdlog::info("System Details - GPU: {}", deviceString);
    }

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    double totalMemory = status.ullTotalPhys / 1024 / 1024;    ///Total physical RAM in MB.
    spdlog::info("System Details - RAM: {} GB ({} MB)", ceil((totalMemory / 1024) * 100) / 100, totalMemory);
}

void Init_ASILoaderSanityChecks()
{
    //Don't simplify by removing filesystem::exists() from this check. While GetFileDescription does handle non-existent files own its own, checking filesystem::exists() first saves 400+ ms of initialization time
    if (std::filesystem::exists(sExePath / "d3d11.dll") && (Util::GetFileDescription((sExePath / "d3d11.dll").string()) == Util::GetFileDescription((sExePath / "winhttp.dll").string())))
    {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "DUPLICATE MOD LOADER ERROR: Multiple ASI Loader .dll's detected! This can cause inconsistent bugs and crashes.\n";
        spdlog::error("DUPLICATE MOD LOADER ERROR: Multiple ASI Loader .dll installations detected! This can cause inconsistent bugs and crashes.");
        std::cout << "DUPLICATE MOD LOADER ERROR: Please delete d3d11.dll, it has been replaced by winhttp.dll & wininit.dll.\n";
        spdlog::error("DUPLICATE MOD LOADER ERROR: Please delete d3d11.dll, it has been replaced by winhttp.dll & wininit.dll.");
#ifndef _WIN32
        std::cout << "DUPLICATE MOD LOADER ERROR: Steam Deck / Linux users must also replace their Steam game launch paramaters with the following command:\n";
        spdlog::error("DUPLICATE MOD LOADER ERROR: Steam Deck / Linux users must also replace their Steam game launch paramaters with the following command:");
        std::cout << "`WINEDLLOVERRIDES=\"wininet,winhttp=n,b\" % command % `\n";
        spdlog::error("`WINEDLLOVERRIDES=\"wininet,winhttp=n,b\" % command % `");
#endif
        spdlog::info("----------");
    }
    Util::CheckForASIFiles(sFixName, true, true); //Exit thread & warn the user if multiple copies of MGSHDFix are trying to initialize.
}
#endif

void Init_ReadConfig()
{
    // Initialise config
    std::ifstream iniFile((sExePath / sFixPath / sConfigFile));
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName << " v" << sFixVer << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile << " is located in " << sExePath / sFixPath << std::endl;
        FreeLibraryAndExitThread(baseModule, 1);
    }
    else {
        spdlog::info("Config file: {}", (sExePath / sFixPath / sConfigFile).string());
        ini.parse(iniFile);
    }


    // Read ini file
    //inipp::get_value(ini.sections["Output Resolution"], "Enabled", bOutputResolution);
    //inipp::get_value(ini.sections["Launcher Config"], "SkipLauncher", bLauncherConfigSkipLauncher);

}

bool DetectGame()
{
    eGameType = UNKNOWN;
    // Special handling for launcher.exe
    if (sExeName == "launcher.exe")
    {
        for (const auto& [type, info] : kGames)
        {
            auto gamePath = sExePath.parent_path() / info.ExeName;
            if (std::filesystem::exists(gamePath))
            {
                spdlog::info("Detected launcher for game: {} (app {})", info.GameTitle.c_str(), info.SteamAppId);
                eGameType = LAUNCHER;
                game = &info;
                return false;
            }
        }
        spdlog::error("Failed to detect supported game, unknown launcher");
        return false;
    }

    for (const auto& [type, info] : kGames)
    {
        if (info.ExeName == sExeName)
        {
            spdlog::info("Detected game: {} (app {})", info.GameTitle.c_str(), info.SteamAppId);
            eGameType = type;
            game = &info;
            return true;
        }
    }

    spdlog::error("Failed to detect supported game, {} isn't supported by MGS-SafetyHookBase", sExeName.c_str());
    return false;
}


void Init_Main()
{ 
    /*if ((eGameType & (MGS2 | MGS3 | MG))
    {
        
    } 
    */
    if (1 == 1) {
        std::string pattern = "E8 ?? ?? ?? ?? 48 8B D8 48 85 C0 0F 84 ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? 48 8B C8 48 8D 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? 0F 57 C0";
        std::string log_name = "put new flag start";

        uint8_t* patternLocation = Memory::PatternScan(baseModule, pattern.c_str());
        if (!patternLocation) {
            spdlog::info("{} : Failed to find pattern - \"{}\"", log_name.c_str(), pattern.c_str());
            return;
        }
        spdlog::info("{} : Hooked! Address is {:s}+{:x}", log_name.c_str(), sExeName.c_str(), (uintptr_t)patternLocation - (uintptr_t)baseModule);
        static SafetyHookMid Initialization{};
        Initialization = safetyhook::create_mid(patternLocation,
            [](SafetyHookContext& ctx)
            {
                spdlog::info("put new flag start - triggered!");
            });
    }
    if (1 == 1) {
        std::string pattern = "0F 84 ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? 48 8B C8 48 8D 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? 0F 57 C0";
        std::string log_name = "newflag return 1";

        uint8_t* patternLocation = Memory::PatternScan(baseModule, pattern.c_str());
        if (!patternLocation) {
            spdlog::info("{} : Failed to find pattern - \"{}\"", log_name.c_str(), pattern.c_str());
            return;
        }
        spdlog::info("{} : Hooked! Address is {:s}+{:x}", log_name.c_str(), sExeName.c_str(), (uintptr_t)patternLocation - (uintptr_t)baseModule);
        static SafetyHookMid Initialization2{};
        Initialization2 = safetyhook::create_mid(patternLocation,
            [](SafetyHookContext& ctx)
            {
                spdlog::info("newflag return 1 - triggered!");
            });
    }
    if (1 == 1) {
        std::string pattern = "33 C0 E9 ?? ?? ?? ?? 8B CD";
        std::string log_name = "newflag return 2";

        uint8_t* patternLocation = Memory::PatternScan(baseModule, pattern.c_str());
        if (!patternLocation) {
            spdlog::info("{} : Failed to find pattern - \"{}\"", log_name.c_str(), pattern.c_str());
            return;
        }
        spdlog::info("{} : Hooked! Address is {:s}+{:x}", log_name.c_str(), sExeName.c_str(), (uintptr_t)patternLocation - (uintptr_t)baseModule);
        static SafetyHookMid Initialization3{};
        Initialization3 = safetyhook::create_mid(patternLocation,
            [](SafetyHookContext& ctx)
            {
                spdlog::info("newflag return 2 - triggered!");
            });
    }
}

std::string lastLoaded;
std::chrono::time_point<std::chrono::high_resolution_clock> initStartTime;

#define INITIALIZE(func) \
    do { \
        std::chrono::time_point<std::chrono::high_resolution_clock> currentInitPhaseStartTime;\
        if(strcmp(#func,"InitializeSubsystems()") == 0) \
        {\
            spdlog::info("---------- Subsystem initialization started ----------", #func); \
            currentInitPhaseStartTime = initStartTime;\
        }\
        else if(!lastLoaded.empty())\
        {\
            spdlog::info("---------- {}\tNow loading: {} ----------", lastLoaded, #func); \
            currentInitPhaseStartTime = std::chrono::high_resolution_clock::now();\
        }\
        else\
        {\
            spdlog::info("---------- Loading: {} ----------", #func); \
            currentInitPhaseStartTime = std::chrono::high_resolution_clock::now();\
        }\
        (func); \
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - currentInitPhaseStartTime).count();\
        if(strcmp(#func,"InitializeSubsystems()") == 0) \
        {\
            if(!lastLoaded.empty())\
            {\
                spdlog::info("---------- {} ----------", lastLoaded); \
            }\
            spdlog::info("---------- All systems completed loading in: {} ms. ----------", duration); \
        }\
        else\
        {\
            lastLoaded = std::string(#func) + " loaded in: " + std::to_string(duration) + " ms."; \
        }\
    } while (0)

void InitializeSubsystems()
{
#ifdef PRIMARY_FIX_MOD
    INITIALIZE(Init_LogSysInfo());
    INITIALIZE(Init_ASILoaderSanityChecks());
#endif
    if (DetectGame())
    {
        INITIALIZE(Init_ReadConfig());
#ifdef PRIMARY_FIX_MOD
        INITIALIZE(Init_FixDPIScaling()); //Needs to be anywhere before the window is created.
#endif
        INITIALIZE(Init_Main());
    }
}

std::mutex mainThreadFinishedMutex;
std::condition_variable mainThreadFinishedVar;
bool mainThreadFinished = false;

DWORD __stdcall Main(void*)
{
    initStartTime = std::chrono::high_resolution_clock::now();
    Logging();
    INITIALIZE(InitializeSubsystems());
    // Signal any threads which might be waiting for us before continuing
    {
        std::lock_guard lock(mainThreadFinishedMutex);
        mainThreadFinished = true;
        mainThreadFinishedVar.notify_all();
    }

    return true;
}

std::mutex memsetHookMutex;
bool memsetHookCalled = false;
void* (__cdecl* memset_Fn)(void* Dst, int Val, size_t Size);
void* __cdecl memset_Hook(void* Dst, int Val, size_t Size)
{
    // memset is one of the first imports called by game (not the very first though, since ASI loader still has those hooked during our DllMain...)
    std::lock_guard lock(memsetHookMutex);
    if (!memsetHookCalled)
    {
        memsetHookCalled = true;

        // First we'll unhook the IAT for this function as early as we can
        Memory::HookIAT(baseModule, "VCRUNTIME140.dll", memset_Hook, memset_Fn);

        // Wait for our main thread to finish before we return to the game
        if (!mainThreadFinished)
        {
            std::unique_lock finishedLock(mainThreadFinishedMutex);
            mainThreadFinishedVar.wait(finishedLock, [] { return mainThreadFinished; });
        }
    }

    return memset_Fn(Dst, Val, Size);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // Try hooking IAT of one of the imports game calls early on, so we can make it wait for our Main thread to complete before returning back to game
        // This will only hook the main game modules usage of memset, other modules calling it won't be affected
        HMODULE vcruntime140 = GetModuleHandleA("VCRUNTIME140.dll");
        if (vcruntime140)
        {
            memset_Fn = decltype(memset_Fn)(GetProcAddress(vcruntime140, "memset"));
            Memory::HookIAT(baseModule, "VCRUNTIME140.dll", memset_Fn, memset_Hook);
        }

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST); // set our Main thread priority higher than the games thread
            CloseHandle(mainHandle);
        }
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

