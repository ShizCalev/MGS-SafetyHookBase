#include "common.hpp"
#include "gamevars.hpp"
#include "spdlog/spdlog.h"

void GameVars::Initialize()
{
    /*
    if (eGameType & MGS2)
    {
        cutsceneFlag = reinterpret_cast<int*>(Memory::GetRelativeOffset(Memory::PatternScan(baseModule, "44 39 35 ?? ?? ?? ?? 89 15", "MGS 2: GameVars: cutsceneFlag", NULL, NULL) + 3));
        padDemoFlag = reinterpret_cast<int*>(Memory::GetRelativeOffset(Memory::PatternScan(baseModule, "8B 05 ?? ?? ?? ?? 45 33 F6 8B 0D", "MGS 2: GameVars: padDemoFlag", NULL, NULL) + 2));
        actorWaitValue = reinterpret_cast<double*>(Memory::GetRelativeOffset(Memory::PatternScan(baseModule, "66 0F 2F 05 ?? ?? ?? ?? 73 ?? 33 C0", "MGS 2: GameVars: actorWaitValue", NULL, NULL) + 4));
        currentStage = reinterpret_cast<char const*>(Memory::GetRelativeOffset(Memory::PatternScan(baseModule, "4C 8D 0D ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 4C 8D 05", "MGS 2: GameVars: currentStage", NULL, NULL) + 3));
    }*/

}

bool GameVars::InCutscene() const
{
    return cutsceneFlag == nullptr ? false : (*cutsceneFlag == 1);
}

bool GameVars::InPadDemo() const
{
    return padDemoFlag == nullptr ? false : (*padDemoFlag == 1);
}

double GameVars::ActorWaitMultiplier() const
{
    return actorWaitValue == nullptr ? 1.0 : ((1.0/60) / *actorWaitValue);
}

const char* GameVars::GetCurrentStage() const
{
    return currentStage == nullptr ? "nullptr" : currentStage;
}
