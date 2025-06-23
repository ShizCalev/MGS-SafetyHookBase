#pragma once
#include "helper.hpp"
#include <inipp/inipp.h>

extern std::string sExeName;


extern inipp::Ini<char> ini;
extern HMODULE baseModule;
extern HMODULE EngineDLL;
extern std::string sGameVersion;
extern std::filesystem::path sExePath;
extern std::string sFixName;


//Config Options
extern int iOutputResY;
extern int iCurrentResY;
extern float fAspectRatio;
extern bool bOutdatedReshade;
extern bool bLauncherConfigSkipLauncher;
extern int iTextureBufferSizeMB;

