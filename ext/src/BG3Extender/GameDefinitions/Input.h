#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Enumerations.h>

BEGIN_NS(input)

using InputPlayerId = uint8_t;
using InputDeviceId = uint16_t;
using InputEventId = uint32_t;

static constexpr unsigned NumRawInputs = 186;
static constexpr unsigned NumPlayers = 4;

enum class EInputDeviceId : int16_t
{
	C = -2,
	Unknown = -1,
	Key = 0,
	Mouse = 1,
	Controller1 = 3,
	Controller2 = 4,
	Controller3 = 5,
	Controller4 = 6,
	Controller5 = 7,
	Controller6 = 8,
	Controller7 = 9,
	Controller8 = 10,
	Touchbar = 11
};

FixedString InputDeviceIdToString(InputDeviceId id);
InputDeviceId StringToInputDeviceId(FixedString id);

struct InputValue
{
	float Value;
	float Value2;
	InputState State;
};

struct InputDevice
{
	int64_t InputPlayerIndex;
	int32_t field_8;
	int32_t ControllerMapping;
	InputDeviceId DeviceId;
	std::array<float, 4> field_14;
	uint8_t field_24;
};


struct InputEventDesc
{
	[[bg3::readonly]] int EventID;
	uint32_t Flags;
	InputType Type;
	FixedString CategoryName;
	[[bg3::hidden]] std::array<void*, 4> EventListeners;
	[[bg3::readonly]] LSStringView EventName;
	TranslatedString EventDesc;
	int field_E8;
};

struct InputRaw
{
	InputRawType InputId;
	InputDeviceId DeviceId;

	inline FixedString LuaGetDeviceId()
	{
		return InputDeviceIdToString(DeviceId);
	}

	inline void LuaSetDeviceId(FixedString id)
	{
		DeviceId = StringToInputDeviceId(id);
	}
};

struct InputRawChange
{
	InputRaw Input;
	InputValue Value;
	bool InjectRawInput;
	bool Debug;
};

struct InjectInputData : public InputRawChange
{
};

struct [[bg3::hidden]] InjectTextData
{
	InputDeviceId DeviceId;
	char Text[64];
};

struct InjectDeviceEvent
{
	uint32_t EventId;
	InputDeviceId DeviceId;
};

struct InputEventText
{
	InputPlayerId PlayerIndex;
	char const* Text;
	uint64_t TextLength;
};

struct InputEvent
{
	int EventId;
	InputDeviceId DeviceId;
	InputPlayerId InputPlayerIndex;
	InputType Type;
	InputValue OldValue;
	InputValue NewValue;
	bool WasPreferred;

	inline FixedString LuaGetDeviceId()
	{
		return InputDeviceIdToString(DeviceId);
	}

	inline void LuaSetDeviceId(FixedString id)
	{
		DeviceId = StringToInputDeviceId(id);
	}
};

struct FireEventDesc
{
	InputEventDesc* EventDesc;
	uint8_t PlayerIndex;
	InputEvent Event;
	InputDeviceId DeviceId;
	InputRawType InputId;
	int32_t field_34;
};


struct HoldRepeatEventDesc
{
	double ReleaseTime;
	double PressTime;
	double HoldTime;
	int Acceleration;
	FireEventDesc FireEvent;
};

struct [[bg3::hidden]] InputEventListener
{
	virtual ~InputEventListener() = 0;
	virtual int GetInputListenerPriority() = 0;
	virtual uint64_t GetInputListenerFilter() = 0;
	virtual const char* GetInputListenerName() = 0;
	virtual bool IsModal() = 0;
	virtual bool InTextInput(int playerIndex) = 0;
	virtual uint16_t* OnInputEvent(uint16_t* eventRetVal, InputEvent* inputEvent) = 0;
	virtual void OnInputModifierChangedEvent(bool, bool, bool, bool) = 0;
	virtual uint16_t* OnInputEvent(uint16_t* eventRetVal, InputEventText* inputEvent) = 0;
	virtual uint16_t* OnUnlinkedInput(uint16_t* eventRetVal, InputDeviceId inputDeviceId) = 0;
};

struct [[bg3::hidden]] InputListenerGroup
{
	int RefCount;
	Array<InputEventListener*> TraversingListeners;
	Array<InputEventListener*> Listeners;
	bool IsTraversing;
};

struct InputBinding : public InputRaw
{
	InputModifier Modifiers;
	uint8_t field_9;
	uint8_t InputPlayerIndex;
};

struct InputBindingDesc
{
	int32_t InputIndex;
	InputBinding Binding;
	int32_t BindingIndex;
};

struct PendingInputRemap
{
	[[bg3::hidden]] void* InputRemapListener;
	InputBinding Binding;
};

struct RawBindingDataMapping
{
	Array<InputBindingDesc> Bindings;
	bool HasModifierKeys;
};

struct InputScheme : public ProtectedGameObject<InputScheme>
{
	std::array<LegacyRefMap<InputDeviceId, InputDeviceId>, NumPlayers> ControllerRemaps;
	std::array<RawBindingDataMapping, NumRawInputs>* RawToBinding;
	std::array<LegacyRefMap<InputEventId, LegacyArray<InputBinding>>, NumPlayers> InputBindings;
	[[bg3::hidden]] Array<void*> DeviceIdToPlayerId; // FIXME! MAP Array<InputDeviceId, int32_t>
	std::array<Array<InputDeviceId>, NumPlayers> DeviceLists;
	[[bg3::hidden]] std::array<Array<void*>, NumPlayers> field_D8; // FIXME! MAP 
	std::array<HashSet<InputEventId>, NumPlayers> DebugEvents;
};

struct InputValueSet
{
	std::array<InputValue, NumRawInputs> Inputs;
	bool Initialized;
};

struct HoldRepeatEvents
{
	Array<FireEventDesc> PressEvents;
	Array<FireEventDesc> ReleaseEvents;
	Array<FireEventDesc> ValueChangeEvents;
	Array<FireEventDesc> HoldEvents;
	Array<FireEventDesc> RepeatEvents;
	Array<HoldRepeatEventDesc> HoldRepeatEvents;
};

struct InputManager : public ProtectedGameObject<InputManager>
{
	[[bg3::hidden]] CRITICAL_SECTION CS;
	[[bg3::hidden]] CRITICAL_SECTION CS2;
	[[bg3::hidden]] UnknownSignal OnInputEvent;
	HashMap<uint32_t, HoldRepeatEvents> HoldRepeatEvents;
	HashMap<InputDeviceId, InputValueSet*> InputStates;
	LegacyRefMap<int32_t, InputEventDesc> InputDefinitions;
	InputScheme InputScheme;
	InputModifier PressedModifiers;
	double LastUpdateTime;
	bool ControllerAllowKeyboardMouseInput;
	// PORT (linux): upstream declares a second `bool AllowDeviceEvents;` here; this build has
	// no such member, and reading it is what disables proxying of the whole class.
	// Three witnesses, one of them the refusal itself:
	//   * the constructor, bin/bg3 0x3f1f060, writes exactly ONE byte between LastUpdateTime
	//     and InputListeners -- `mov byte [rbx+0x2f0], bpl` at 0x3f1f18a, then straight to
	//     `mov dword [rbx+0x2f8], ebp` (InputListeners.RefCount) at 0x3f1f191.  It is the
	//     InputManager's ctor by its own tail: 0x3f1f3df `mov qword [rbx+0x3a8], -1`
	//     (PlayerDeviceIDs, four 0xffff), 0x3f1f1f7 `mov dword [rbx+0x3b8], 0xffffffff` and
	//     0x3f1f208 `mov byte [rbx+0x3be], 0xff` (CurrentRemap).
	//   * a census of every instruction in .text with displacement 0x2f1: no function
	//     anywhere in the binary touches +0x2f1 on an object that also carries an
	//     InputManager landmark (0x3a8, 0x3b8, 0x3be, 0x620).  Calibrated on 0x2e0, which
	//     returns the ctor and 0x7717d90 as it must.
	//   * a live object (peek.py): byte 0x2f1 reads 0x75 -- stale heap
	//     text, as does the rest of the padding to 0x2f8 -- which is exactly what upstream's
	//     property validator rejects as a bool.
	// Not an ABI difference: 0x2f1..0x2f7 is padding before an 8-aligned member under MSVC
	// too, so upstream reads garbage on Windows as well.  Plain RE error, like the phantom
	// `field_b4` of the aspk key components.
	[[bg3::hidden]] InputListenerGroup InputListeners;
	[[bg3::hidden]] InputListenerGroup DeviceListeners;
	[[bg3::hidden]] InputListenerGroup NotifyUpListeners;
	Array<InputRaw> RawInputs;
	std::array<int, NumPlayers> PlayerDevices;
	std::array<InputDeviceId, NumPlayers> PlayerDeviceIDs;
	PendingInputRemap CurrentRemap;
#if 0 // Editor only
	// std::array<Array<uint32_t>, 12> RawInputTypes;
#endif
	Array<FireEventDesc> Events;
	[[bg3::hidden]] Array<InjectTextData> TextInjects;
	Array<InjectInputData> InputInjects;
	Array<InjectDeviceEvent> DeviceEventInjects;
	std::array<glm::vec4, NumPlayers> PlayerColors;
	std::array<InputDevice, 12> PerDeviceData;
	[[bg3::hidden]] void* KeyboardLayoutTranslator;
};

#if defined(BG3SE_LINUX)
// PORT (linux): the offsets the AllowDeviceEvents work established on bin/bg3 (c8da62b1).
// The whole class was re-read, not just the bool: nothing else in
// the mirror moved, which is why the fix is a deletion and not a shift.
//   ctor 0x3f1f060: 0x2e0 (byte), 0x2e8 (qword 0), 0x2f0 (byte), 0x2f8/0x328/0x358 (the three
//     listener groups' RefCount), 0x320/0x350/0x380 (their IsTraversing), 0x388 (RawInputs),
//     0x398 (PlayerDevices), 0x3a8 (PlayerDeviceIDs = -1), 0x3b0.. (CurrentRemap).
//   UnlinkDevices 0x7719720: 0xb8..0xf0 (InputStates, keys *2 and values *8 off 0xc0 buckets),
//     0xa8 with a 96-byte element (`lea r14,[rcx+rcx*2]; shl r14,5`, = sizeof(HoldRepeatEvents)),
//     0x1a0 + player*16 with size at +0xc (DeviceLists), 0x328 (DeviceListeners),
//     0x398 + player*4 set to -1 (PlayerDevices), 0x108 (InputScheme).
//   fieldcensus.py of ls__gInputManager (0x7d9d0a8): 0x190/0x198 as a bucket table,
//     0x310/0x31c/0x358, 0x620 (KeyboardLayoutTranslator).
static_assert(offsetof(InputManager, ControllerAllowKeyboardMouseInput) == 0x2F0);
static_assert(offsetof(InputManager, InputListeners) == 0x2F8);
static_assert(offsetof(InputManager, DeviceListeners) == 0x328);
static_assert(offsetof(InputManager, NotifyUpListeners) == 0x358);
static_assert(offsetof(InputManager, RawInputs) == 0x388);
static_assert(offsetof(InputManager, PlayerDevices) == 0x398);
static_assert(offsetof(InputManager, PlayerDeviceIDs) == 0x3A8);
static_assert(offsetof(InputManager, CurrentRemap) == 0x3B0);
static_assert(offsetof(InputManager, KeyboardLayoutTranslator) == 0x620);
static_assert(offsetof(InputManager, InputScheme) == 0x108);
static_assert(offsetof(InputManager, InputStates) == 0xB8);
static_assert(sizeof(HoldRepeatEvents) == 96);
static_assert(offsetof(InputScheme, DeviceIdToPlayerId) == 0x190 - 0x108);
static_assert(offsetof(InputScheme, DeviceLists) == 0x1A0 - 0x108);
#endif

END_NS()
