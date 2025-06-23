#include "distance_culling.hpp"
#include <spdlog/spdlog.h>
#include "common.hpp"


void DistanceCulling::Initialize() const
{
    /*
    if (!(eGameType & MGS2))
    {
        return;
    }

    if (uint8_t* grassTest1_Result = Memory::PatternScan(baseModule, "8B D0 3D ?? ?? ?? ?? 7E", "mgs2 dd", NULL, NULL))
    {
        static SafetyHookMid grassTest1MidHook {};
        grassTest1MidHook = safetyhook::create_mid(grassTest1_Result,
            [](SafetyHookContext& ctx)
            {
                ctx.rax = 300; // Set the distance culling value
                ctx.rflags |= 0x0000000000000002; // Set the ZF flag to 1 
            });
        LOG_HOOK(grassTest1MidHook, "grass scan", NULL, NULL)
    }*/


    
    if (uint8_t* grassTest1_Result = Memory::PatternScan(baseModule, "49 8B 54 2E ?? 0F 85", "grass scan", NULL, NULL))
    {
        static SafetyHookMid grassTest1MidHook {};
        grassTest1MidHook = safetyhook::create_mid(grassTest1_Result,
            [](SafetyHookContext& ctx)
            {
                ctx.xmm0.f32[0] = 400.0f; // Set the distance culling value
            });
        LOG_HOOK(grassTest1MidHook, "grass scan", NULL, NULL)
    }

}
