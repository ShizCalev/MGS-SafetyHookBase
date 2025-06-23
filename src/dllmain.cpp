#include "common.hpp"
#include <safetyhook.hpp>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>

///Resources
//#include "d3d11_api.hpp"
//#include "gamevars.hpp"

///Features
#include "intro_skip.hpp"

///Fixes

//#include "texture_buffer_size.hpp"

//Warnings
#include "asi_loader_checks.hpp"

///WIP

//#include "corrupt_save_message.hpp"
//#include "distance_culling.hpp"
//#include "gamma_correction.hpp"
//#include "msaa.hpp"
//#include "mute_warning.hpp"
//#include "pause_on_focus_loss.hpp"
//#include "wireframe.hpp"


std::chrono::time_point<std::chrono::high_resolution_clock> initStartTime;
std::string lastLoaded;

HWND MainHwnd = nullptr;

HMODULE baseModule = GetModuleHandle(NULL);
HMODULE EngineDLL;

// Version
#define VERSION_STRING "0.0.1a"
std::string sFixName = "MGS-SafetyHookBase";
int iConfigVersion = 1; //increment this when making config changes, along with the number at the bottom of the config file
                        //that way we can sanity check to ensure people don't have broken/disabled features due to old config files.

// Logger
std::shared_ptr<spdlog::logger> logger;
std::filesystem::path sLogFile = sFixName + ".log";
std::filesystem::path sFixPath;
std::filesystem::path sExePath;
std::string sExeName;
std::string sGameVersion;

// Ini
inipp::Ini<char> ini;
std::filesystem::path sConfigFile = "Enhanced.ini";
std::pair DesktopDimensions = { 0,0 };

// Ini Variables
bool bVerboseLogging = true;
bool bAspectFix;
bool bHUDFix;
bool bFOVFix;
bool bOutputResolution;
int iOutputResX;
int iOutputResY;
int iInternalResX;
int iInternalResY;
bool bWindowedMode;
bool bBorderlessMode;
bool bFramebufferFix;
bool bLauncherJumpStart;
int iAnisotropicFiltering;
bool bDisableTextureFiltering;
int iTextureBufferSizeMB;
bool bMouseSensitivity;
float fMouseSensitivityXMulti;
float fMouseSensitivityYMulti;
bool bDisableCursor;
bool bOutdatedReshade;

// Launcher ini variables
bool bLauncherConfigSkipLauncher = false;
int iLauncherConfigCtrlType = 5;
int iLauncherConfigRegion = 0;
int iLauncherConfigLanguage = 0;
std::string sLauncherConfigMSXGame = "mg1";
int iLauncherConfigMSXWallType = 0;
std::string sLauncherConfigMSXWallAlign = "C";

// Aspect ratio + HUD stuff
float fNativeAspect = (float)16 / 9;
float fAspectRatio;
float fAspectMultiplier;
float fHUDWidth;
float fHUDHeight;
float fDefaultHUDWidth = (float)1280;
float fDefaultHUDHeight = (float)720;
float fHUDWidthOffset;
float fHUDHeightOffset;
float fMGS2_EffectScaleX;
float fMGS2_EffectScaleY;
int iCurrentResX;
int iCurrentResY;

struct GameInfo
{
    std::string GameTitle;
    std::string ExeName;
    int SteamAppId;
};



const GameInfo* game = nullptr;
const LPCSTR sClassName = "CSD3DWND";


// CreateWindowExA Hook
SafetyHookInline CreateWindowExA_hook{};
HWND WINAPI CreateWindowExA_hooked(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    if (std::string(lpClassName) == std::string(sClassName))
    {
        if (bBorderlessMode )
        {
            auto hWnd = CreateWindowExA_hook.stdcall<HWND>(dwExStyle, lpClassName, lpWindowName, WS_POPUP, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
            SetWindowPos(hWnd, HWND_TOP, 0, 0, DesktopDimensions.first, DesktopDimensions.second, NULL);
            spdlog::info("CreateWindowExA: Borderless: ClassName = {}, WindowName = {}, dwStyle = {:x}, X = {}, Y = {}, nWidth = {}, nHeight = {}", lpClassName, lpWindowName, WS_POPUP, X, Y, nWidth, nHeight);
            spdlog::info("CreateWindowExA: Borderless: SetWindowPos to X = {}, Y = {}, cx = {}, cy = {}", 0, 0, (int)DesktopDimensions.first, (int)DesktopDimensions.second);
            MainHwnd = hWnd;
            return hWnd;
        }

        if (bWindowedMode)
        {
            auto hWnd = CreateWindowExA_hook.stdcall<HWND>(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
            SetWindowPos(hWnd, HWND_TOP, 0, 0, iOutputResX, iOutputResY, NULL);
            spdlog::info("CreateWindowExA: Windowed: ClassName = {}, WindowName = {}, dwStyle = {:x}, X = {}, Y = {}, nWidth = {}, nHeight = {}", lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight);
            spdlog::info("CreateWindowExA: Windowed: SetWindowPos to X = {}, Y = {}, cx = {}, cy = {}", 0, 0, iOutputResX, iOutputResY);
            MainHwnd = hWnd;
            return hWnd;
        }
    }

    MainHwnd = CreateWindowExA_hook.stdcall<HWND>(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    return MainHwnd;
}

// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size)
    {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open()) 
        {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (std::filesystem::exists(_filename) && std::filesystem::file_size(_filename) >= _max_size) 
        {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override
    {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file()
    {
        if (std::filesystem::exists(_filename))
        {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

void Init_CalculateScreenSize()
{
    iCurrentResX = iInternalResX;
    iCurrentResY = iInternalResY;

    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD variables
    fHUDWidth = iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2;
    fHUDHeightOffset = 0;
    if (fAspectRatio < fNativeAspect) 
    {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2;
    }


    // Log details about current resolution
    spdlog::info("Current Resolution: Resolution: {}x{}", iCurrentResX, iCurrentResY);
    spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
    spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
    spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
    spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
    spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
    spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
}

void Init_Logging()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

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
            spdlog::info("---------- Logging initialization started ----------");
            if (!logDirExists)
            {
                spdlog::info("New log subdirectory created.");
            }
            spdlog::info("{} v{} loaded.", sFixName, VERSION_STRING);
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
        catch (const spdlog::spdlog_ex& ex) 
        {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            return FreeLibraryAndExitThread(baseModule, 1);
        }
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - initStartTime).count(); \
    spdlog::info("---------- Logging loaded in: {} ms ----------", duration);
}

bool IsSteamOS()
{
    // Check for Proton/Steam Deck environment variables
    return std::getenv("STEAM_COMPAT_CLIENT_INSTALL_PATH") ||
        std::getenv("STEAM_COMPAT_DATA_PATH") ||
        std::getenv("XDG_SESSION_TYPE"); // XDG_SESSION_TYPE is often set on Linux
}

std::string GetSteamOSVersion()
{
    std::ifstream os_release("/etc/os-release");
    std::string line;
    while (std::getline(os_release, line))
    {
        if (line.find("PRETTY_NAME=") == 0)
        {
            // Remove quotes if present
            size_t first_quote = line.find('"');
            size_t last_quote = line.rfind('"');
            if (first_quote != std::string::npos && last_quote != std::string::npos && last_quote > first_quote)
            {
                return line.substr(first_quote + 1, last_quote - first_quote - 1);
            }
            return line.substr(13); // fallback
        }
    }
    return "";
}

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


    std::string os;

    if (IsSteamOS())
    {
        os = GetSteamOSVersion();
    }
    else
    {
        HKEY key;
        LSTATUS versionResult = RegOpenKeyExA(
            HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ | KEY_WOW64_64KEY, &key
        );

        if (versionResult == ERROR_SUCCESS)
        {
            char buffer[256]; DWORD size = sizeof(buffer);
            LSTATUS nameResult = RegQueryValueExA(
                key, "ProductName",
                nullptr, nullptr,
                reinterpret_cast<LPBYTE>(buffer), &size
            );
            if (nameResult == ERROR_SUCCESS)
            {
                os = buffer;
            }
        }

        RegCloseKey(key);

        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        while (ntdll)
        {
            typedef LONG(WINAPI* RtlGetVersion_t)(PRTL_OSVERSIONINFOW);
            RtlGetVersion_t RtlGetVersion =
                reinterpret_cast<RtlGetVersion_t>(GetProcAddress(ntdll, "RtlGetVersion"));
            if (!RtlGetVersion) break;

            RTL_OSVERSIONINFOW info = {};
            info.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);

            if (RtlGetVersion(&info) != 0) break;
            os += " (build " + std::to_string(info.dwBuildNumber) + ")";

            if (info.dwBuildNumber < 22000) break;
            std::size_t pos = os.find("Windows 10");

            if (pos == std::string::npos) break;
            os.replace(pos, 10, "Windows 11");
            break;
        }
    }
    if (!os.empty()) spdlog::info("System Details - OS:  {}", os);

}

void Init_ReadConfig()
{
    // Initialise config
    std::ifstream iniFile((sExePath / sConfigFile).string());
    if (!iniFile)
    {
        spdlog::error("Config file not found: {}", (sExePath / sConfigFile).string());
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName << " v" << VERSION_STRING << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile << " is located in " << sExePath << std::endl;
        return FreeLibraryAndExitThread(baseModule, 1);
    }
    else
    {
        spdlog::info("Config file: {}", (sExePath / sConfigFile).string());
        ini.parse(iniFile);
    }

    int loadedConfigVersion;
    inipp::get_value(ini.sections["Config.Internal"], "ConfigVersion", loadedConfigVersion);
    if (loadedConfigVersion != iConfigVersion)
    {
        spdlog::error("Config version mismatch: expected {}, got {}", iConfigVersion, loadedConfigVersion);

        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName << " v" << VERSION_STRING << " loaded." << std::endl;
        std::cout << "MGS-SafetyHookBase CONFIG ERROR: Outdated config file!" << std::endl;
        std::cout << "MGS-SafetyHookBase CONFIG ERROR: Please install -all- the files from the latest release!" << std::endl;
        return FreeLibraryAndExitThread(baseModule, 1);
    }

    // Grab desktop resolution
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();


    // Read ini file
    inipp::get_value(ini.sections["Config.Internal"], "VerboseLogging", bVerboseLogging);
    spdlog::info("Config Parse: VerboseLogging: {}", bVerboseLogging);

    /*
    inipp::get_value(ini.sections["Output Resolution"], "Enabled", bOutputResolution);
    inipp::get_value(ini.sections["Output Resolution"], "Width", iOutputResX);
    inipp::get_value(ini.sections["Output Resolution"], "Height", iOutputResY);
    inipp::get_value(ini.sections["Output Resolution"], "Windowed", bWindowedMode);
    inipp::get_value(ini.sections["Output Resolution"], "Borderless", bBorderlessMode);
    inipp::get_value(ini.sections["Internal Resolution"], "Width", iInternalResX);
    inipp::get_value(ini.sections["Internal Resolution"], "Height", iInternalResY);
    inipp::get_value(ini.sections["Anisotropic Filtering"], "Samples", iAnisotropicFiltering);
    inipp::get_value(ini.sections["Disable Texture Filtering"], "DisableTextureFiltering", bDisableTextureFiltering);
    inipp::get_value(ini.sections["Framebuffer Fix"], "Enabled", bFramebufferFix);
    inipp::get_value(ini.sections["Launcher Config"], "LauncherJumpStart", bLauncherJumpStart);*/
    inipp::get_value(ini.sections["MGS-SafetyHookBase.asi"], "SkipIntroVideos", g_IntroSkip.isEnabled);
    spdlog::info("Config Parse: Skip Intro Videos: {}", g_IntroSkip.isEnabled);



    /*

   
    //INITIALIZE(Init_GammaShader());
    //INITIALIZE(g_DistanceCulling.Initialize());
    //INITIALIZE(g_MultiSampleAntiAliasing.Initialize());
    //INITIALIZE(g_PauseOnFocusLoss.Initialize());
    //INITIALIZE(g_Wireframe.Initialize());

    //INITIALIZE(g_AimAfterEquipFix.Initialize());
    //INITIALIZE(g_ColorFilterFix.Initialize());*/

    //inipp::get_value(ini.sections["MG1 Custom Loading Screens"], "Enabled", g_MG1CustomLoadingScreens.isEnabled);
     /*
    inipp::get_value(ini.sections["Mouse Sensitivity"], "Enabled", bMouseSensitivity);
    inipp::get_value(ini.sections["Mouse Sensitivity"], "X Multiplier", fMouseSensitivityXMulti);
    inipp::get_value(ini.sections["Mouse Sensitivity"], "Y Multiplier", fMouseSensitivityYMulti);
    inipp::get_value(ini.sections["Disable Mouse Cursor"], "Enabled", bDisableCursor);
    inipp::get_value(ini.sections["Texture Buffer"], "SizeMB", iTextureBufferSizeMB);
    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bAspectFix);
    inipp::get_value(ini.sections["Fix HUD"], "Enabled", bHUDFix);
    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFOVFix);
    inipp::get_value(ini.sections["Launcher Config"], "SkipLauncher", bLauncherConfigSkipLauncher);



    // Log config parse
    spdlog::info("Config Parse: bOutputResolution: {}", bOutputResolution);
    if (iOutputResX == 0 || iOutputResY == 0)
    {
        iOutputResX = DesktopDimensions.first;
        iOutputResY = DesktopDimensions.second;
    }
    spdlog::info("Config Parse: iOutputResX: {}", iOutputResX);
    spdlog::info("Config Parse: iOutputResY: {}", iOutputResY);
    spdlog::info("Config Parse: bWindowedMode: {}", bWindowedMode);
    spdlog::info("Config Parse: bBorderlessMode: {}", bBorderlessMode);
    if (iInternalResX == 0 || iInternalResY == 0)
    {
        iInternalResX = iOutputResX;
        iInternalResY = iOutputResY;
    }
    spdlog::info("Config Parse: iInternalResX: {}", iInternalResX);
    spdlog::info("Config Parse: iInternalResY: {}", iInternalResY);
    spdlog::info("Config Parse: iAnisotropicFiltering: {}", iAnisotropicFiltering);
    if (iAnisotropicFiltering < 0 || iAnisotropicFiltering > 16)
    {
        iAnisotropicFiltering = std::clamp(iAnisotropicFiltering, 0, 16);
        spdlog::info("Config Parse: iAnisotropicFiltering value invalid, clamped to {}", iAnisotropicFiltering);
    }
    spdlog::info("Config Parse: bDisableTextureFiltering: {}", bDisableTextureFiltering);
    spdlog::info("Config Parse: bFramebufferFix: {}", bFramebufferFix);

    spdlog::info("Config Parse: bMouseSensitivity: {}", bMouseSensitivity);
    spdlog::info("Config Parse: fMouseSensitivityXMulti: {}", fMouseSensitivityXMulti);
    spdlog::info("Config Parse: fMouseSensitivityYMulti: {}", fMouseSensitivityYMulti);
    spdlog::info("Config Parse: bDisableCursor: {}", bDisableCursor);
    spdlog::info("Config Parse: iTextureBufferSizeMB: {}", iTextureBufferSizeMB); //g_TextureBufferSize
    */

}

// Case-insensitive string comparison helper
static bool iequals(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
        std::equal(a.begin(), a.end(), b.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
}

bool DetectGame()
{
    return iequals(sExeName, "Splintercell.exe");
}

void Init_FixDPIScaling()
{

    SetProcessDPIAware();
    spdlog::info("High-DPI scaling fixed.");

}

void Init_CustomResolution()
{
}




void Init_Miscellaneous()
{
    /*
    if (eGameType & (MG|MGS2|MGS3|LAUNCHER))
    {
        if (bDisableCursor)
        {
            // Launcher | MG/MG2 | MGS 2 | MGS 3: Disable mouse cursor
            // Thanks again emoose!
            uint8_t* MGS2_MGS3_MouseCursorScanResult = Memory::PatternScanSilent(baseModule, "BA 00 7F 00 00 33 ?? FF ?? ?? ?? ?? ?? 48 ?? ??");
            if (eGameType & LAUNCHER)
            {
                unityPlayer = GetModuleHandleA("UnityPlayer.dll");
                MGS2_MGS3_MouseCursorScanResult = Memory::PatternScanSilent(unityPlayer, "BA 00 7F 00 00 33 ?? FF ?? ?? ?? ?? ?? 48 ?? ??");
            }

            if (MGS2_MGS3_MouseCursorScanResult)
            {
                if (eGameType & LAUNCHER)
                {
                    spdlog::info("Launcher | MG/MG2 | MGS 2 | MGS 3: Mouse Cursor: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MGS2_MGS3_MouseCursorScanResult - (uintptr_t)unityPlayer);
                }
                else
                {
                    spdlog::info("Launcher | MG/MG2 | MGS 2 | MGS 3: Mouse Cursor: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MGS2_MGS3_MouseCursorScanResult - (uintptr_t)baseModule);
                }
                // The game enters 32512 in the RDX register for the function USER32.LoadCursorA to load IDC_ARROW (normal select arrow in windows)
                // Set this to 0 and no cursor icon is loaded
                Memory::PatchBytes((uintptr_t)MGS2_MGS3_MouseCursorScanResult + 0x2, "\x00", 1);
                spdlog::info("Launcher | MG/MG2 | MGS 2 | MGS 3: Mouse Cursor: Patched instruction.");
            }
            else if (!MGS2_MGS3_MouseCursorScanResult)
            {
                spdlog::error("Launcher | MG/MG2 | MGS 2 | MGS 3: Mouse Cursor: Pattern scan failed.");
            }
        }
    }

    if ((bDisableTextureFiltering || iAnisotropicFiltering > 0) && (eGameType & (MGS2|MGS3)))
    {
        uint8_t* MGS3_SetSamplerStateInsnScanResult = Memory::PatternScanSilent(baseModule, "48 8B ?? ?? ?? ?? ?? 44 39 ?? ?? 38 ?? ?? ?? 74 ?? 44 89 ?? ?? ?? ?? ?? ?? EB ?? 48 ?? ??");
        if (MGS3_SetSamplerStateInsnScanResult)
        {
            spdlog::info("MGS 2 | MGS 3: Texture Filtering: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MGS3_SetSamplerStateInsnScanResult - (uintptr_t)baseModule);

            static SafetyHookMid SetSamplerStateInsnXMidHook{};
            SetSamplerStateInsnXMidHook = safetyhook::create_mid(MGS3_SetSamplerStateInsnScanResult + 0x7,
                [](SafetyHookContext& ctx)
                {
                    // [rcx+rax+0x438] = D3D11_SAMPLER_DESC, +0x14 = MaxAnisotropy
                    *reinterpret_cast<int*>(ctx.ecx + ctx.eax + 0x438 + 0x14) = iAnisotropicFiltering;

                    // Override filter mode in r9d with aniso value and run compare from orig game code
                    // Game code will then copy in r9d & update D3D etc when r9d is different to existing value
                    //0x1 = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR (Linear mips is essentially perspective correction.) 0x55 = D3D11_FILTER_ANISOTROPIC
                    ctx.e9 = bDisableTextureFiltering ? 0x1 : 0x55;
                });

        }
        else if (!MGS3_SetSamplerStateInsnScanResult)
        {
            spdlog::error("MGS 2 | MGS 3: Texture Filtering: Pattern scan failed.");
        }
    }

    if (eGameType & MGS3 && bMouseSensitivity)
    {
        // MG 1/2 | MGS 2 | MGS 3: MouseSensitivity
        uint8_t* MGS3_MouseSensitivityScanResult = Memory::PatternScanSilent(baseModule, "F3 0F ?? ?? ?? F3 0F ?? ?? 66 0F ?? ?? ?? 0F ?? ?? 66 0F ?? ?? 8B ?? ??");
        if (MGS3_MouseSensitivityScanResult)
        {
            spdlog::info("MGS 3: Mouse Sensitivity: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MGS3_MouseSensitivityScanResult - (uintptr_t)baseModule);

            static SafetyHookMid MouseSensitivityXMidHook{};
            MouseSensitivityXMidHook = safetyhook::create_mid(MGS3_MouseSensitivityScanResult,
                [](SafetyHookContext& ctx)
                {
                    ctx.xmm0.f32[0] *= fMouseSensitivityXMulti;
                });

            static SafetyHookMid MouseSensitivityYMidHook{};
            MouseSensitivityYMidHook = safetyhook::create_mid(MGS3_MouseSensitivityScanResult + 0x2E,
                [](SafetyHookContext& ctx)
                {
                    ctx.xmm0.f32[0] *= fMouseSensitivityYMulti;
                });
        }
        else if (!MGS3_MouseSensitivityScanResult)
        {
            spdlog::error("MGS 3: Mouse Sensitivity: Pattern scan failed.");
        }
    }
    */
}




void preCreateDXGIFactory()
{

    
}

void afterCreateDXGIFactory()
{

}

void preD3D11CreateDevice()
{

}

void afterD3D11CreateDevice()
{
    //createGammaShader();

    //SetGamma(1.0);
}


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
    INITIALIZE(Init_LogSysInfo());
    INITIALIZE(Init_ASILoaderSanityChecks());
    if (DetectGame())
    {
        EngineDLL = GetModuleHandleA("Engine.dll");
            //Initialization order (these systems initialize vars used by following ones.)
        //INITIALIZE(g_GameVars.Initialize());           //1
       // INITIALIZE(Init_D3D11Hooks());                 //2 Caches the D3DDevice, DXGIFactory, and D3DContext from D3DCreateDevice/DXGICreateFactory
        INITIALIZE(Init_ReadConfig());                 //3
     //   INITIALIZE(Init_CalculateScreenSize());        //4
        INITIALIZE(Init_FixDPIScaling());              //6 Needs to be anywhere before the window is created in CustomResolution.
        //INITIALIZE(Init_CustomResolution());           //7
        //   INITIALIZE(Init_ScaleEffects());
          // INITIALIZE(Init_AspectFOVFix());
         //  INITIALIZE(Init_HUDFix());
        //INITIALIZE(Init_Miscellaneous());

        //Features
  //  INITIALIZE(g_TextureBufferSize.Initialize());

    //INITIALIZE(Init_GammaShader());
    //INITIALIZE(g_DistanceCulling.Initialize());
        INITIALIZE(g_IntroSkip.Initialize());
        //INITIALIZE(g_MG1CustomLoadingScreens.Initialize());
        //INITIALIZE(g_MultiSampleAntiAliasing.Initialize());
        //INITIALIZE(g_PauseOnFocusLoss.Initialize());
        //INITIALIZE(g_Wireframe.Initialize());

            //Fixes
      //  INITIALIZE(Init_LineScaling());
      //  INITIALIZE(g_WaterReflectionFix.Initialize());
      //  INITIALIZE(g_SkyboxFix.Initialize());
        //INITIALIZE(g_AimAfterEquipFix.Initialize());
        //INITIALIZE(g_ColorFilterFix.Initialize());
     //   INITIALIZE(g_EffectSpeedFix.Initialize());
      //  INITIALIZE(g_StereoAudioFix.Initialize());

            //Warnings
        //INITIALIZE(g_CorruptSaveWarning.Initialize());
        //INITIALIZE(g_MuteWarning.Initialize());

    }
    else
    {
        spdlog::error("Game not detected. Please ensure you are running the correct game executable.");
    }
}

std::mutex mainThreadFinishedMutex;
std::condition_variable mainThreadFinishedVar;
bool mainThreadFinished = false;

DWORD __stdcall Main(void*)
{
    initStartTime = std::chrono::high_resolution_clock::now();
    Init_Logging();
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

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        //CallbackHandler::RegisterCallback(Init);

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, CREATE_SUSPENDED, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_TIME_CRITICAL); // set our Main thread priority higher than the games thread
            ResumeThread(mainHandle);
            CloseHandle(mainHandle);
        }
        break;

    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
