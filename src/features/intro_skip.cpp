#include "common.hpp"
#include "spdlog/spdlog.h"
#include "intro_skip.hpp"

void IntroSkip::Initialize() const
{
    if (!isEnabled)
    {
        return;
    }

    if (uint8_t* IntroSkipScan2 = Memory::PatternScan(EngineDLL, "39 3D ?? ?? ?? ?? 75 ?? 39 3D", "Skip Intro Videos", NULL, NULL))
    {
        static SafetyHookMid Initialization3 {};
        Initialization3 = safetyhook::create_mid(IntroSkipScan2,
            [](SafetyHookContext& ctx)
            {
                ctx.edi = 1;
                spdlog::info("Intro movies skipped!");
            });

    }

    if (uint8_t* IntroSkipScan = Memory::PatternScan(EngineDLL, "E8 ?? ?? ?? ?? 6A ?? 6A ?? 68 ?? ?? ?? ?? 56", "Skip Intro Videos (Idle Replay)", NULL, NULL))
    {
        Memory::PatchBytes((uintptr_t)IntroSkipScan, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 20);
    }

}
