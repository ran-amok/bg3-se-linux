#pragma once

BEGIN_NS(party)

struct View
{
    int UserID;
    [[bg3::legacy(field_8)]] Guid UserUuid;
    [[bg3::legacy(field_18)]] Guid ViewUuid;
    Array<EntityHandle> Characters;
};


struct ViewComponent : public BaseComponent
{
    DEFINE_COMPONENT(PartyView, "eoc::party::ViewComponent")

    [[bg3::legacy(field_0)]] Guid PartyUuid;
    Array<View> Views;
    Array<EntityHandle> Characters;
};

struct Member
{
    [[bg3::legacy(field_0)]] int32_t UserId;
    [[bg3::legacy(field_8)]] Guid UserUuid;
    [[bg3::legacy(field_18)]] Guid ViewUuid;
    Array<uint8_t> field_28;
};

struct CompositionComponent : public BaseComponent
{
    DEFINE_COMPONENT(PartyComposition, "eoc::party::CompositionComponent")

    EntityHandle Party;
    [[bg3::legacy(field_8)]] Guid PartyUuid;
    Array<Member> Members;
};


struct PortalsComponent : public BaseComponent
{
    DEFINE_COMPONENT(PartyPortals, "eoc::party::PortalsComponent")

    HashSet<EntityHandle> Portals;
};


struct Recipe
{
    FixedString Name;
    uint8_t field_4;
};


struct RecipesComponent : public BaseComponent
{
    DEFINE_COMPONENT(PartyRecipes, "eoc::party::RecipesComponent")

    Array<Recipe> Recipes;
};


struct Waypoint
{
    FixedString Name;
    Guid field_8;
    FixedString Level;

    inline bool operator == (Waypoint const& o) const
    {
        return Name == o.Name;
    }
};


struct WaypointsComponent : public BaseComponent
{
    DEFINE_COMPONENT(PartyWaypoints, "eoc::party::WaypointsComponent")

    HashSet<Waypoint> Waypoints;
};


struct MemberComponent : public BaseComponent
{
    DEFINE_COMPONENT(PartyMember, "eoc::party::MemberComponent")

    [[bg3::legacy(UserID)]] int UserId;
    [[bg3::legacy(field_8)]] Guid UserUuid;
    EntityHandle Party;
    [[bg3::legacy(field_20)]] Guid ViewUuid;
    [[bg3::legacy(field_30)]] bool IsPermanent;
};


struct FollowerComponent : public BaseComponent
{
    DEFINE_COMPONENT(PartyFollower, "eoc::party::FollowerComponent")

    EntityHandle Following;
};


DEFINE_TAG_COMPONENT(eoc::party, CurrentlyFollowingPartyComponent, CurrentlyFollowingParty)
DEFINE_TAG_COMPONENT(eoc::party, BlockFollowComponent, BlockFollow)

END_NS()


BEGIN_SE()

template <>
inline uint64_t HashMapHash<party::Waypoint>(party::Waypoint const& v)
{
    return HashMapHash(v.Name);
}

END_SE()


BEGIN_NS(recruit)

struct RecruiterComponent : public BaseComponent
{
    DEFINE_COMPONENT(Recruiter, "eoc::recruit::RecruiterComponent")

    HashSet<EntityHandle> Recruiters;
};

END_NS()


BEGIN_NS(esv::party)

struct UserSnapshotComponent : public BaseComponent
{
    DEFINE_COMPONENT(ServerUserSnapshot, "esv::party::UserSnapshotComponent")

    [[bg3::legacy(Snapshot)]] HashMap<Guid, Array<Array<EntityHandle>>> PerUserCharacters;
};

struct EntityTransferData
{
    EntityHandle Party;
    Guid Group;
    int Position{ 0 };
};

struct PartyFollowerRequestData
{
    EntityHandle Follower;
    EntityHandle Leader;
    bool ToAdd{ true };
};

struct MemberParticipationUpdateRequest
{
    bool ToAdd{ true };
    EntityHandle Member;
};

struct PartySystem : public BaseSystem
{
    DEFINE_SYSTEM(ServerParty, "esv::party::PartySystem")

    [[bg3::hidden]] void* GameEventListenerVMT;
#if !defined(BG3SE_LINUX)
    [[bg3::hidden]] void* field_18;
#endif
// PORT (linux): upstream's `[[bg3::hidden]] void* field_18;` is not in the native build -- the
// Win32 slot of ecl::TimelineSystem and ecl::CharacterManager. The macOS oracle's ctor,
// inlined into esv::RegisterSystems (0x1043cfca3,
// build af3f1c71), allocates 0x1c8 bytes, writes vtable pointers at 0, 0x10 and 0x18 only, and
// stores EocServer/PartyUnitTestHelper at 0x20/0x28; its D2 (0x1056949b0) frees every member
// below eight bytes under upstream's offset.
    [[bg3::hidden]] void* GameStateEventListenerVMT;
    [[bg3::hidden]] void* EocServer;
    [[bg3::hidden]] void* PartyUnitTestHelper;
    bool Unload;
    bool UnloadEntities;
    [[bg3::hidden]] UnknownFunction qword40;
    HashSet<EntityHandle> SummonsToRemove;
    Array<MemberParticipationUpdateRequest> MemberParticipationUpdate;
    HashMap<UserId, EntityHandle> TransferUser;
    HashSet<EntityHandle> ReevaluateParty;
    Array<Array<EntityHandle>> CreateGroupFromMembers;
    HashMap<EntityHandle, glm::vec3> PostReloadStorySetup;
    HashMap<EntityHandle, EntityTransferData> TransferMember;
    Array<PartyFollowerRequestData> FollowerRequest;
};

#if defined(BG3SE_LINUX)
// The macOS oracle's ctor and D2 (build af3f1c71); no Linux method of the class is matched.
static_assert(offsetof(PartySystem, EocServer) == 0x20,
    "macOS ctor 0x1043cfd4b stores the first argument at this+0x20");
static_assert(offsetof(PartySystem, Unload) == 0x30,
    "macOS ctor 0x1043cfd53 zeroes the two bools as a word at this+0x30");
static_assert(offsetof(PartySystem, qword40) == 0x38,
    "macOS ctor 0x1043cfd8f writes pStorage_ = this+0x40 to this+0x38");
static_assert(offsetof(PartySystem, MemberParticipationUpdate) == 0xA8,
    "macOS D2 frees the Array buffer at this+0xa8, 0x105694d34");
static_assert(offsetof(PartySystem, ReevaluateParty) == 0xF8,
    "macOS D2 ~HashTable on this+0xf8, 0x105694c7f");
static_assert(offsetof(PartySystem, FollowerRequest) == 0x1B8,
    "macOS D2 frees the Array buffer at this+0x1b8, 0x1056949da");
static_assert(sizeof(PartySystem) == 0x1C8,
    "operator new(0x1c8) in esv::RegisterSystems, macOS 0x1043cfca3");
#endif

END_NS()


BEGIN_NS(esv::escort)

struct FollowerComponent : public BaseComponent
{
    DEFINE_COMPONENT(EscortFollower, "esv::escort::FollowerComponent")

    EntityHandle Following;
};

struct LeaderComponent : public BaseComponent
{
    DEFINE_COMPONENT(EscortLeader, "esv::escort::LeaderComponent")

    FixedString Group;
};

struct LeaderPriorityComponent : public BaseComponent
{
    DEFINE_COMPONENT(EscortLeaderPriority, "esv::escort::LeaderPriorityComponent")

    HashMap<FixedString, int32_t> Priorities;
};

struct MemberComponent : public BaseComponent
{
    DEFINE_COMPONENT(EscortMember, "esv::escort::MemberComponent")

    FixedString Group;
};

struct StragglersTrackerComponent : public BaseComponent
{
    DEFINE_COMPONENT(EscortStragglersTracker, "esv::escort::StragglersTrackerComponent")

    Array<EntityHandle> Stragglers;
    glm::vec3 field_10;
};

DEFINE_TAG_COMPONENT(esv::escort, HasStragglersComponent, EscortHasStragglers)

END_NS()