#pragma once

#include <GameDefinitions/Effect.h>

BEGIN_NS(ecl)

struct GameUIDragController;
struct WorldDragController;
struct CharacterDragController;

struct DragController : public ProtectedGameObject<DragController>
{
    [[bg3::hidden]] void* VMT;
    [[bg3::readonly]] uint16_t PlayerId;
};

struct PlayerDragData : public ProtectedGameObject<PlayerDragData>
{
    Array<EntityHandle> Entities;
    glm::vec2 ScreenPosition;
    bool DraggingAction;
    bool IsDragging;
    bool Active;
    [[bg3::hidden]] DragController* ActiveController;
    [[bg3::hidden]] Array<DragController*> Controllers;

    // Lua getter helpers
    GameUIDragController* LuaGetGameUIDrag() const;
    WorldDragController* LuaGetWorldDrag() const;
    CharacterDragController* LuaGetCharacterDrag() const;

    //# P_GETTER(GameUIDrag, LuaGetGameUIDrag)
    //# P_GETTER(WorldDrag, LuaGetWorldDrag)
    //# P_GETTER(CharacterDrag, LuaGetCharacterDrag)
};


struct [[bg3::hidden]] DragDropManager : public ProtectedGameObject<DragDropManager>
{
    UnknownSignal OnStartDrag;
    UnknownSignal OnStartDragActionEntities;
    UnknownSignal OnDragUpdate;
    UnknownSignal OnStartDragActionPositional;
    UnknownSignal OnStartDragActionPassive;
    UnknownSignal OnStopDrag;
    uint8_t field_90[0x40]; // MPMCQueueBounded maybe?
    LegacyRefMap<uint16_t, PlayerDragData> PlayerData;
    ecs::EntityWorld* EntityWorld;
};

struct GameUIDragController : public DragController
{
    bool Enabled;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the whole class in one function, bg3+0x43c3c60, which builds a GameUIDragController
// for a player: `mov edi, 0x10` / `call operator new` 0x43c3c9e, then `mov word [rax+8], r14w`
// (PlayerId, its own player-id argument), `mov qword [rax], rcx` with rcx = the vtable 0x79ecf28,
// and `mov byte [rax+0xa], 0` at 0x43c3cb8 -- Enabled, its own byte store past the base's live
// PlayerId word.  The class is named without a symbol in bin/bg3: the vtable is an 8-slot run at
// 0x79ecf28 whose bodies are, slot for slot, the macOS oracle's eight ecl::GameUIDragController
// methods -- ~D1 `ret` / ~D0 `jmp operator delete` / IsValid `mov al, 1; ret` (macOS 0x103593370) /
// Update `ret` (0x103593380) / GetPriority returning 0x6e (0x103593390) / Pause `mov al, 1; ret`
// (0x1035933d0) / Resume `ret` (0x1035933e0) / ReleaseData `mov byte [rdi+0xa], 0; ret`
// (0x1035933f0).  Two of those bodies are unique enough to stand alone: `mov eax, 0x6e; ret`
// occurs exactly ONCE in this binary's .text (inside 0x654e940, the Linux GetPriority, which
// reaches it by `cmp byte [rdi+0xa], 0` / `jne` -- a third, independent read of Enabled at 0xa),
// and the ReleaseData one-liner twice (0x654e9a0 here).  The run's neighbour 0x654e9b0 is the
// matched ecl::WorldDragController::~WorldDragController, the sibling controller.
//
// Both numbers discriminate the ABI.  DragController is {void* VMT, uint16_t PlayerId}, so its
// dsize is 0xa and its sizeof 0x10: Itanium puts Enabled at 0xa inside that tail padding, MSVC
// would put it at 0x10 and make the class 0x18.
static_assert(offsetof(DragController, PlayerId) == 0x8);
static_assert(offsetof(GameUIDragController, Enabled) == 0xa);
static_assert(sizeof(GameUIDragController) == 0x10);
#endif

struct WorldDragController : public DragController
{
    [[bg3::hidden]] void* InputManagerVMT2;
    bool Enabled;
    bool Active;
    uint8_t field_1A;
    uint8_t field_1B;
    EntityHandle Outline;
    Array<EntityHandle> DraggingEntities;
    Array<EntityHandle> Visuals;
    Array<Transform> Transforms;
    [[bg3::readonly]] int PathId;
    [[bg3::readonly]] int PathId2;
    float field_60;
    float Rotation;
    float TimeSpentSq;
    float TimeSpent;
    float RotationRate;
    uint64_t ErrorFlags;
    [[bg3::readonly]] int State;
    [[bg3::hidden]] void* PathPreviewer;
};

struct CharacterDragController : public DragController
{
    [[bg3::hidden]] void* VMT2;
    bool Enabled;
    bool Active;
    uint8_t field_1A;
    bool AnimationEventTriggered;
    EntityHandle Outline1;
    EntityHandle Character;
    EntityHandle VisualEntity;
    EffectHandler Effect;
    [[bg3::hidden]] void* ObjectVisual;
    glm::vec3 WorldPosition;
    glm::vec4 WorldRotate;
    glm::vec4 WorldRotate2;
    float Rotation;
    float TimeSpentSq;
    float TimeSpent;
    float RotationRate;
    uint64_t ErrorFlags;
};

END_NS()
