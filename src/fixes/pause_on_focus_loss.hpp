#pragma once

class PauseOnFocusLoss
{
public:
    bool isEnabled;
    void Initialize() const;
};

inline PauseOnFocusLoss g_PauseOnFocusLoss;


