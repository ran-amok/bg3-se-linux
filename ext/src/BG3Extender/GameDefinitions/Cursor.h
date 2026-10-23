#pragma once

BEGIN_NS(ecl)

struct CursorControl : ProtectedGameObject<CursorControl>
{
    [[bg3::hidden]] void* VMT;
    [[bg3::hidden]] void* VMT2;
    bool ShowCursor;
    float VisibleTimer;
    float FadeTimer;
    FixedString CursorName;
    CursorType CurrentOverlay;
    CursorType RequestedCursor;
    CursorType RequestedOverlay;
    Array<CursorType> CursorOverrides;
};

// PORT (linux): witnessed on the Linux build through the global ecl__gCursorControl
// 0x7c063a8, which every client task's SetCursor() loads.
static_assert(offsetof(CursorControl, RequestedCursor) == 0x24);      // Pickpocket::SetCursor 0x28fb0f0 stores a CursorType there
static_assert(offsetof(CursorControl, CursorOverrides) == 0x30);      // ... behind `cmp dword [obj + 0x3c], 0` = size_, so the Array starts at 0x30
static_assert(sizeof(CursorControl) == 0x40);                         // 33 of the 35 referring functions test that same +0x3c, the last member

END_NS()
