#pragma once

class GameVars final
{
private:
    static void OnLevelTransition();

    int* cutsceneFlag = nullptr;
    int* padDemoFlag = nullptr;
    double* actorWaitValue = nullptr;
    const char* currentStage = nullptr;

public:
    void Initialize();
    bool InCutscene() const;
    bool InPadDemo() const;
    double ActorWaitMultiplier() const;
    const char* GetCurrentStage() const;
};

inline GameVars g_GameVars;
