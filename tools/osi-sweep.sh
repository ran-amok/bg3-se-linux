#!/bin/bash
# A scripted probe of the Osiris surface over the live console (instead of
# ad-hoc pokes): one Lua line per probe, each wrapped in pcall and
# labelled, so extender.log says which one fell over if the game dies partway. Run with the
# game up, in a loaded save (server Lua exists); reads the answers back from extender.log.
#
#   tools/osi-sweep.sh            run every probe, then print the SWEEP lines from the log
#   tools/osi-sweep.sh --list     print the probes without sending anything
#   tools/osi-sweep.sh --check    parse every probe with the host's Lua, no game needed
#   tools/osi-sweep.sh --net-pair [HOST_STATE [JOIN_STATE]]
#                                 the remote net probes across the two instances of the LAN
#                                 rig, with a verdict per direction
#   tools/osi-sweep.sh --net-half host|join [TOK [PEERUSER]]
#                                 one side of that round against the local console, for a
#                                 round across two boxes
#   tools/osi-sweep.sh --net-selftest   calibrate the net-pair reading on crafted logs
#
# What each expects: the Osiris globals are located, so Osi.* calls and queries resolve and
# run; listeners fire through the hooked Event entry; Ext.Template getters raise, not crash.
# The insert path (story PROCs, DB inserts/deletes, events) goes through the right vtable
# slot and a real AddStr, so proc-timer produces
# TimerFinished("Debug_LaunchWithInterval_bg3se_sweep") in osiris.log and a SWEEP-TIMER line
# here within ~2 s, proc-status leaves status-check at 1 (BLESS for 30 s), and
# db-insert/db-delete come back "inserted"/"deleted".
# DB_*:Get reads facts (CTuple's layout is measured): db-get is a count >= 1 in a loaded save,
# db-get-row the host's GUID, and db-count-inserted/-deleted read 1 then 0.
# mod-name/mod-dir/mod-base exercise STDString properties through the object proxy (nil
# there means the validator does not know this build's 16-byte string), and
# template-count/template-one the bank slot. game-version is the
# manifest's .rodata version literal packed as Windows bg3se would report it: v4.73.98.727
# on this build.
# loca-set/loca-get round-trip a new translated string through the repository's write path
# (Lock + Array push_back + HashMap set), which kills the game unless the spin lock's mirror
# has the game's 0x18-byte layout; the main
# menu's version line ("Script Extender v33 loaded") is the same path's visible witness.
# ecs-* cover the bind of the entity world: ecs-types walks ComponentRegistry_.Bitmask (the
# logical bit count is the +8 word here, not the +12 one) and Types at stride 0x40, and its
# count must agree with a popcount of the live bitmask and with the registry's own count field
# (1854 in a server world, 1651 in the client one). "No entity world is bound on this build"
# means ValidateEntityWorld() refused and extender.log says which identity failed.
# staticdata-* are the witness for the eoc__gGuidResourceManager row: without it every
# Ext.StaticData call fails with "Resource definition manager not available yet!". The
# manager is the game's ls::ImmutableDataHeadmaster, and the index side is bound separately
# (114/114 resource managers in the UpdateComponentMappings line), so what these test is the
# pointer: staticdata-feat should be a count in
# the hundreds, staticdata-banks a non-zero count per bank, and the two -one probes a named resource with
# its GUID. staticdata-sources and staticdata-bymod walk the bank's ResourceGuidsByMod instead of
# Definitions, so they fail separately if the bank mirror rather than the manager is wrong.
# ecs-types-one and ecs-types-mapped witness the symtab TypeId table, which is what populates
# IndexSymbolToNameMaps on Linux: with no table bound no component index has a name and
# GetRegisteredComponentTypes pushes either nothing or 1854 empty strings (the unbound-index
# sentinel's Name is a pointer to ""). Expected: mapped in the hundreds, one a real name such
# as ecs::IsReplicationOwnedComponent.
# ecs-uuid, ecs-host, ecs-host-comps and ecs-host-uuid go one step further, through the
# singleton query cache, which only PostUpdate fills -- so they read nil unless the
# ecs::EntityWorld::Update hook is in. They are the witness for it: ecs-uuid a handle rather
# than nil, ecs-host an entity,
# ecs-host-comps a nonzero count of mapped components on that entity, and ecs-host-uuid the
# host's own Guid read back out of ls::uuid::Component -- which is a mapped component read,
# so it is also the first thing ValidateMappedComponentSizes() unmaps if the mirror's size is
# wrong (extender.log carries that verdict, one ERR line per mismatch).
# ecs-fixed-mapped is the regression check on eight mirror sizes measured offline (the list is
# in the probe): each is a registered component, so the ECS registry
# supplies the expected size and ValidateMappedComponentSizes() is the instrument. It reads
# "n/8"; anything short names the ones that do not map, and extender.log's
# "[ECS INTEGRITY CHECK]" lines say whether the cause is a force unmap (local < ECS) or an OOB
# mismatch (local > ECS, mapping kept).
#
# The expected reading is "7/8 ecl::TimelineSceneTrigger", not 8/8, and that is by design rather
# than a regression: the sweep runs in the *server* context, whose mapped-component list holds
# no ecl:: components at all. The calibration is ecl::SoundVolumeTrigger, a mirror upstream has
# right, which reads false in the same query (ecs-never-broken below). The client console
# cannot answer it -- "Cannot queue client commands in game state" -- so the client-side witness
# is structural.
#
# level-info/level-state/level-tracked are the witness for LevelData, a mirror no offline
# instrument can check: on this build the HashMap's value is an Array of templates per key
# rather than one template pointer, and with upstream's shape the property validator rejects
# 'TrackedTemplates' and disables Lua proxying of the whole class.
# The three carry their own calibration: if the class is refused,
# level-info reads "ok nil" and the other two read ERR ("attempt to index a nil value"),
# which is a different signature from the game simply not being in a level. Expected on the
# TUT_Avernus_C save: a LevelData address, State = 4 (ls::LevelData::ResolveDependencies' own
# terminal value, macOS 0x106e7cb50), and 16 keys -- the template count varies with the save
# and the moment, so read its shape not its value.
#
# vt-count/vt-one/vt-all-gtex are the witness for resource::VirtualTextureResource past its SRW
# lock (the lock is a bare 56-byte pthread_rwlock_t, 48 bytes more than upstream's SRWLOCK,
# so every member behind it moves).  They read the struct on both sides of the lock at once --
# RootPath and GTSIndex in front of it, GTexFileName / TileSetFileName / IsValid behind -- and
# the calibration is inside the probe: with an 8-byte lock GTexFileName is read out of
# the middle of the pthread_rwlock_t, so vt-all-gtex counts few or none of the bank's
# textures as named, and a FixedString built from those bytes is as likely to be refused by
# upstream's validator (an ERR line here) as to be empty.  A real GTS filename on every row is
# what the right lock size predicts.  This path matters beyond the probe: BindSEVirtualTextures()
# reads tex->GTexFileName on every launch to remap mod virtual textures.
#   Without the ls__gGlobalResourceManager manifest row they read "0", an ERR on indexing nil
#   and "0/0": StaticSymbols::GetCurrentResourceBank() is null and every Ext.Resource call
#   logs "Resource manager not available".  0/0 out of
#   vt-all-gtex reads like a pass and is not one, so read **res-banks and vt-count first**: a
#   zero there still means "no bank", and nothing behind it says anything about a mirror.
#
# res-banks is the witness for ls::ResourceManager itself, whose Linux object begins with a
# bare 56-byte pthread_rwlock_t where upstream's Windows mirror has `void* VMT` -- with
# upstream's layout ResourceBanks sits at 0x20, inside the lock, and GetCurrentResourceBank()
# dereferences a garbage pointer.  It asks six different bank types for their resource lists, so it
# reads ResourceBanks[sel] at 0x50, the bank's Packages count (which is what picks bank 0 over
# bank 1) and ResourceContainer::Banks indexed by six different ResourceBankType values.  Its
# calibration is that a wrong ResourceBanks offset cannot produce six plausible non-zero counts
# -- it produces a null bank (every count 0, or "Resource manager not available") or a crash.
# The members past this struct's CRITICAL_SECTION at 0xe0 have no Lua path in the server
# context; witness those by reading the live object instead (Sources.HashSize == 401 and
# GenomeAnimationManagers.HashSize == 3 are the ctor's own constants).
# res-anim-*/res-visual-*/res-blendspace-one are the witness for resource::TwoStepLoadableResource,
# the base of twelve resource classes, whose SRW lock is the game's bare 56-byte
# pthread_rwlock_t -- so the base is 0x70 and not upstream's 0x40, and every member of
# all twelve moves by 48.  Three derived classes are read because the offline witnesses are
# three different functions: AnimationResource off its own Visit, VisualResource off
# its DoUnload, BlendSpaceResource off its destructor's `add rdi, 0x70`.
#   The calibration is in the shapes, not the values.  With a 0x40 base every one of these
#   members is read out of the middle of a pthread_rwlock_t: Duration and TimeStep come out as
#   whatever the lock's words decode to as floats (the idle lock is nearly all zero, so
#   res-anim-durations would read near 0/n rather than near n/n), #Objects comes off a garbage
#   CompactSet so res-visual-objects reads 0/n or faults, and XAxisName is an STDString built on
#   a pointer out of the lock -- as likely to be refused by upstream's property validator (an ERR
#   line here) as to print gibberish.  A high ratio on both bulk probes is what the right base
#   predicts.  Read res-banks first, as with the vt-* probes: no bank means these say nothing.
#   resource::TextureManager has no Lua path at all, so it stays unwitnessed in game -- the
#   live object at ResourceManager+0xa8 is its instrument.
# cache-templates/cache-create are the witness for ls::CacheTemplateManagerBase past its lock,
# where the game's ls::SRWLock is 0x48 (a bare pthread_rwlock_t, a pthread_t owner and a u32)
# rather than MSVC's 0x10, 0x38 more -- so
# Lock 0xd0, AddedTemplates 0x118, RemovedTemplates 0x128, EnableTemplateSync 0x138, sizeof
# 0x140.  cache-create calls esv::Character::CreateCacheTemplate on the host, which is the one
# Lua path that runs upstream's RegisterTemplate/CacheTemplate against the *game's* manager:
# it takes the write side of that lock, sets all three HashMaps, reads the EnableTemplateSync
# byte, push_backs into AddedTemplates, and then takes the read side again in IncTemplateRef.
# With MSVC's lock size the shim's own SRWLOCK handle is written over the first 8 bytes of the
# game's pthread_rwlock_t, so this probe is also the check that nothing is being corrupted.
# The calibration is inside the probe: it reports the Templates count before and after, and
# `before -> before+1` is the only reading that means RegisterTemplate ran to completion --
# a hang means the lock is wrong, and an unchanged count means the write never landed.
# net-* are the witness for Ext.Net's in-process path and for the client tick that delivers
# its replies. net-version reads 2 on both
# sides, net-ishost true, and net-host-userid the host's esv::Character::UserID -- 65537,
# which is the UserId(1, 1) ClientNet.inl assumes for the local user on this port; any other
# reading means that assumption is wrong for this save. net-channel registers the server
# half of the SampleMod channel (the client half is tools/test-mod-overlay's
# BootstrapClient.lua, so the test mod must be installed with it) through the builtin
# NetworkManager's AddChannel -- Ext.Net.CreateChannel exists only in a mod's own Ext table
# netid-host / netid-roundtrip read the server's ecs::EntityReplicationAuthority through the
# net::GameServer mirror (Replication at +0x438; EntityToNetId 0x90 and
# NetIdToEntity 0xd0 inside it): the host's NetId is a small number and Ext.Entity.Get(netid)
# comes back to the same entity ("N -> true"). nil or a wrong entity means the mirror is off.
# vars-* and pvars-* are the witness for esv__OsirisVariableHelper__SavegameVisit 0x4198970,
# the hook most SE mods lean on.
# They are the *in-memory* half of that path: UserVariableManager and ModVariableManager are what
# SavegameSerializer::Serialize() hands the visitor, so a mirror or proxy fault here would fault at
# save time too. vars-mod-write should echo the token it just stored and vars-mod-read the same
# token back after a Flush; vars-user-roundtrip the string read off the host entity's Vars proxy and
# vars-user-entities 1. pvars-write is `n=1` on a save that has never seen the sweep and one higher
# each time it runs in the same session.
#   What these cannot witness on their own is the *savegame* leg, and it matters which half:
#   - The write side is witnessed off the resulting .lsv, offline: SavegameSerializer::SavegameVisit
#     opens EnterRegion(GFS.strScriptExtenderSave) and writes an ExtenderVersion attribute into it,
#     so a save taken with the hook in must carry a ScriptExtenderSave region in its globals.lsf
#     (tools/lspk.py reads the .lsv; it is an LSPK). That region is also the calibration for the
#     Itanium destructor-pair slot in ObjectVisitor: one slot off and every
#     call lands on the game's preceding virtual, so EnterRegion becomes TryEnterRegion and nothing
#     is written -- no region, rather than a wrong one.
#   - The read side needs the same registration to exist after the reload, and a console-registered
#     prototype does not survive the Lua reset. So vars-mod-read/vars-user-roundtrip come back nil
#     after a load and that is *expected*, not a failure. pvars-read is the one that should carry
#     across: PersistentVars is restored by ModTable name with no prototype -- but only for mods
#     GetPersistentVarMods() returns, which is `MinimumVersion >= 4 && ModTable non-empty`, and
#     upstream's SampleMod declares RequiredVersion 1. So with SampleMod alone pvars-read reads
#     the token back within a session and nil after a reload, and witnessing the restore needs a mod
#     with RequiredVersion >= 4 enabled.
# ecb-* are the witness for ecs__EntityWorld__FlushECBs 0x2486dd0, the hook
# Ext.Entity.OnCreate/OnDestroy ride.
# The hook's pre-hook is upstream's OnECSFlushECBs -> ThrowECBFlushEvents(), which walks
# world->CommandBuffers[i].Data.EntityChanges (CommandBuffers is the +0x170 assert in
# EntitySystem.h) and calls the Lua Create/Destroy subscriptions -- so nothing at all fires
# without the hook, and a wrong CommandBuffers offset faults inside the pre-hook rather than
# reading zero.
#   The probes make their own traffic instead of waiting for the world to produce some:
#   ecb-subscribe registers OnCreate/OnDestroy for StatusID (eoc::status::IDComponent) and keeps
#   the counters in _G, then ecb-apply puts BLESS on the host and ecb-remove takes it off, which
#   creates and destroys a status entity carrying that component. ecb-counts runs last, after
#   notify, and should read create >= 1, destroy >= 1 and a plausible entity handle in last=.
#   Four calibrations, because "0" is exactly what a missing hook and a missing status both
#   produce: ecb-type-mapped must say `mapped` (an unmapped component type is never delivered,
#   and the sentinel's name is a pointer to "", so this is the ecs-types-mapped question asked of
#   one type); ecb-subscribe must return two subscription ids, which says the subscription exists
#   whether or not it ever fires; and ecb-active then ecb-gone must read 1 then 0, which says the
#   *game* did the apply and the remove. All four passing with ecb-counts at 0 is the hook not
#   firing, and nothing else.
#   Ordering: a Create arrives within one probe interval of the ApplyStatus, but the matching
#   Destroy can take longer than the 0.7 s after RemoveStatus (it is there on a later read, with
#   HasActiveStatus going 1 -> 0 in step). So the counts probe wants slack, which
#   is why it sits after notify; a destroy short by one with ecb-gone reading 0 is that lag and
#   not the hook.
# (ModLoader.lua), not in the console's -- and the CoreLib table is the live one because
# FileLoader caches by name. net-request sends one server->client request and net-send one
# message, net-legacy one module-less broadcast (upstream's wire path; in-process here). The
# reading is in the SWEEP-NET lines that follow in extender.log, six of them, in this order:
# 'client request ping=41 from 65537' (server->client, delivered by the client tick),
# 'server request ping=141' (the reverse request the client handler fires), 'server got
# reply pong=42' (the reply, by the server tick), 'client got reply pong=142', then 'client
# message text=hello' and 'client legacy channel=bg3se_sweep_legacy'.
# No client lines at all means the client tick is not
# running; server lines only means the reply queue is not drained. Rerunnable: the server
# channel is kept in _G across sweeps. The overlay's BootstrapClient.lua also prints every
# client GameStateChanged it sees ('SWEEP-NET client GameStateChanged A -> B'), the witness
# for the ecl::GameState enum against this build.
# dmg-* are the witness for stats__DealDamageFunctor__ApplyDamage 0x3c4ae20 and
# esv__StatsSystem__ThrowDamageEvent 0x330fe20, the pair behind
# Ext.Events.DealDamage / DealtDamage / BeforeDealDamage.
# All three events exist only inside those two hooks: OnDealDamage
# wraps ApplyDamage and throws DealDamage before the call and DealtDamage after it,
# OnEntityDamageEvent wraps ThrowDamageEvent and throws BeforeDealDamage. So a count of zero
# with the rest of the block passing is the hook not firing, and nothing else.
#   The trigger is Osi.ApplyDamage, chosen because the path is established offline: the game
#   registers ApplyDamage with four parameters -- Object, Damage, DamageType, Source, read off
#   the registration at 0x3a863af in 0x3a73c30 -- and its handler 0x7190c40 calls 0x3c4ae20
#   *directly*, one edge in the harvest callgraph. dmg-subscribe registers the three
#   subscriptions and keeps the counters in _G, dmg-apply damages the host for 1 Slashing.
#   Three calibrations, because "0" is what a missing hook and a damage the game refused both
#   produce: dmg-osi must say `userdata`, which says the Osiris call is bound at all (the
#   binding is callable userdata on this build, not a plain function, and the probes after it
#   prove it is callable);
#   dmg-subscribe must return three subscription ids, which says the subscriptions exist
#   whether or not they fire; and dmg-hp-before then dmg-hp-after must differ by 1, which says
#   the *game* applied the damage. dmg-apply refuses when the host is at 10 hp or less, so a
#   sweep can never kill the host -- and then hp-before == hp-after and the counts mean
#   nothing, which the `skipped:` in its result says out loud.
#   Two count reads, for the ecb-* block's ordering reason: dmg-counts
#   right after the trigger and dmg-counts-late after notify. deal/dealt are synchronous
#   inside the hooked function, so they should already be up at dmg-counts; before= may lag,
#   since ThrowDamageEvent is only one branch of ApplyDamage. before=0 with deal and dealt
#   up is the second hook not reached on this path, not the second hook missing -- read
#   whether ApplyDamage took the branch before blaming the row.
# aigrid-* are the witness for eoc__AiGrid__FindPath 0x2663ae0.
# The hook is a *pre*-hook, upstream's ScriptExtender::OnFindPath -> Lua
# State::OnFindPath, and that handler dereferences the mirror before Lua sees anything: it walks
# AiGrid::PathMap (+0xf0) for the pathId and reads AiPath::SearchStarted (+0x270). So a path
# arriving non-nil in the event is the *live* witness for both offsets, where Ai.h's eleven asserts
# are only the offline half. Ext.Events.FindPath fires nowhere else.
#   The trigger is deliberately the async API rather than a character order:
#   aigrid-begin calls Ext.Level.BeginPathfinding on the host entity towards a point 4 m away,
#   which pushes a path into AiGrid::Paths for the game's own pathfinding pass to service --
#   esv::AiHelpers::UpdateRequest 0x4047670 is one of FindPath's 13 call sites and is the
#   candidate for that pass. If the request is created but nothing fires, that reading is "the
#   game does not service a path queued from outside", not "the hook is missing", and aigrid-move
#   is the fallback trigger: it issues Osi.CharacterMoveToPosition, which reaches
#   esv::MSMoveTo::FindPath 0x2815e40 (another call site) the way the game itself does.
#   **The story function is
#   CharacterMoveToPosition(CHARACTER, x, y, z, SpeedString, Event) -- six parameters, and the
#   fifth is a STRING ('Walk'), not a REAL.** Passing 0.0 there takes no arity and reads as
#   "the game refused the order" when it is the probe that is wrong; the
#   number form is kept as the second candidate so the typing stays visible. Note that the
#   refusal counts the Lua stack, not the declared parameters: "String expected for argument 6"
#   is about the *fifth* parameter. aigrid-move reports the fired= count from before it ran, so
#   a later fired>=1 can be attributed to the order rather than to anything before it.
#   Three calibrations, because "fired=0" is what a missing hook, an unserviced request and a
#   refused order all produce: aigrid-event must be true (the event object exists whether or not
#   the hook does), aigrid-subscribe must return a subscription id, and aigrid-begin must read
#   active>=1, which says the *request* exists independently of the hook. That count is taken
#   **inside aigrid-begin's own chunk**: the game drains the queue
#   within a frame, so the separate aigrid-requests probe 0.7 s later reads active=0 on a
#   perfectly serviced request and cannot calibrate anything. It is kept as the drain reading.
#   aigrid-counts runs last, after notify and the surf-* block, for the ecb-* block's ordering
#   reason: it reads fired= (pre-hook entries), callbacks= (async completions, which need
#   AiPath::SearchComplete to go true) and first=, the field dump taken inside the first pre-hook.
#   In a pre-hook `started=false complete=false` is the expected reading -- the game sets both a
#   few instructions later -- so `started=true` on the first event would say the offsets are off
#   by something that happens to land on a set byte, not that the hook is early.
# aigrid-sync is the witness for the *other* half of the pair, eoc__AiGrid__FindPathImmediate
# 0x47b30b0, which Ext.Level.FindPath calls. It takes its **own** path
# through Ext.Level.BeginPathfindingImmediate rather than reusing aigrid-begin's: that function
# spins `while (!path->SearchComplete) UpdatePathfinding()`, so running it on the async request
# would complete that search itself and rob the FindPath pre-hook of the event it is there to
# produce. The reading is the bool, which on this build is AiPath::GoalFound read out of +0x272
# (the oracle never sets al; this build does), beside SearchComplete, which the spin cannot exit
# without -- so `returned X complete=true` is the row, `complete=false` would mean the call never
# ran. Ext.Level.FindPath also carries a null guard for a build with no row:
# on such a build it refuses by name instead of jumping to address 0.
# Two failure shapes are worth naming in advance because
# both are Larian's: a hang would be a search that never completes, and a segfault inside the
# call would be the sentinel or not-found arm, which both fall into a dereference of a null path
# -- neither is reachable through the GetPathId that feeds it here.
# io-* cover Ext.IO. Five functions, and the Linux risk is the *path root*:
# SaveFile/AppendFile go through script::GetPathForExternalIo -> StaticSymbols::ToPath("/Script
# Extender", PathRootType::UserProfile), and ToPath is a PORT (linux) divergence
# (GameHelpers.cpp:77) -- Windows indexes ls__PathRoots itself, the port calls the game's own
# ls::Path::Path(EPathRoot, StringView const&) 0x22f3300 (manifest row 57), which reads
# s_Roots[root]. ls::Path::UpdateRoots 0x3f62120 (manifest row 94) fills eighteen of those
# entries, one per PathRootType 1..18, and s_Roots[4] -- UserProfile -- is the one written at
# 0x3f627a3; PathRootType's numbering is the game's, and this block is the independent witness
# for UserProfile = 4 in it (Mods = 8 has one of its own). LoadFile with context 'data' takes
# the other branch (MakeFileReader -> MakeFileReaderAbsolute, itself a PORT (linux): the Linux
# ls::FileReader ctor writes to +0x88, so the object gets a 0x100 allocation), so the two
# contexts are two separate things to witness. The write itself is a PORT (linux) narrow-path
# std::ofstream
# (ScriptHelpers.cpp:117) and the directory it needs comes from the win32 shim's
# CreateDirectoryW/PathFileExistsW (ext/linux/win32.cpp:199) through
# CreateParentDirectoryRecursive -- which is why io-save writes to a *nested* relative path
# (bg3se_sweep/io-probe.txt): a flat one would never call mkdir.
#   The out-of-band witness is the point of this block and no probe can give it: the file is on
#   disk afterwards, so `find ~ -name 'io-probe.txt'` outside the game names the root
#   independently of anything the extender reports. Do that, and write the resolved path
#   down -- it is the measurement. io-root prints the token so the file can be told from an
#   older run's.
#   Four calibrations, because a green round-trip through a wrong root is still green (it would
#   simply write somewhere else): io-refuse-traversal, io-refuse-chars and io-load-missing must
#   all refuse -- IsSafeRelativePath (ScriptHelpers.cpp:30) applies Windows' file-naming rules
#   on Linux *by design*, so a colon, a star, an empty path and a trailing dot are all false
#   here and that is not a port defect -- and io-data must come back with the 88 bytes that
#   offline lspk.py reads out of ExtenderSampleMod.pak, which is the pak-reader arm.
#   io-append's expected reading is the surprise and is upstream's, not the port's:
#   SaveExternalFile opens with ios::out|ios::binary, which *truncates*, and only then seeks to
#   the end -- so AppendFile overwrites, on Windows exactly as here. Expect file=BBB len=3, not
#   AAAABBB len=7. Do not file that as a Linux divergence; upstream has the same two lines.
#   io-override is the one probe that can read a composed absolute path back out of the game:
#   AddPathOverride/GetPathOverride store ToPath(..., PathRootType::Data) of both sides, so the
#   `after=` field is the Data root as the game composed it. before= and unknown= must be nil.
#   io-mcm-save is a second-order check and needs no new mod: with MCM installed, MCM itself
#   calls Ext.IO.SaveFile for its own config and
#   Ext.IO.LoadFile(..., 'data') for its blueprint, so a rewrite of that config through MCM's own
#   ModConfig:SaveAllSettings() is a real mod exercising this path. It rewrites the same values,
#   so it changes nothing. Check the mtimes of Script Extender/BG3MCM/Profiles/Default/<mod>/
#   settings.json -- one per mod MCM holds settings for -- and NOT
#   mod_configuration_menu_config.json, which is the profile pointer and is
#   written elsewhere. 'no BG3MCM mod table' or 'ModConfig is not' is a miss, not a failure.
# timer-* cover Ext.Timer on the server.
# **Server context, not client**: ecl__GameStateMachine__Update is in sAbsentOnLinux (the game
# inlines it into App::Update) and the client tick comes from ecs__EntityWorld__Update via
# ecl::ScriptExtender::OnECSTick instead -- that is the ctick-* block below. Two separate
# things here:
#   The clock primitives rest on the port's own shim -- QueryPerformanceCounter
#   (ext/linux/win32.cpp:167, CLOCK_MONOTONIC ns) with QueryPerformanceFrequency hardcoded to
#   1e9 (:174) -- so timer-clocks is a sanity read (a plausible Unix ClockEpoch, a parseable
#   ClockTime out of std::format under libc++) and timer-advance is the check that matters:
#   dMonotonic and dMicrosec come from the shim and must both be ~700 ms / ~700000 us apart
#   (one probe interval), while dGameTime comes from the *game* and is the interesting one.
#   The callback path is the content. timers_.Update(time.Time) runs from State::OnUpdate
#   (Lua/LuaBinding.cpp:671), reached on the server from ScriptExtender::PreUpdate
#   (ScriptExtenderServer.cpp:203), which is hooked on esv__GameStateMachine__Update 0x2c66a10
#   (manifest row 23). Server Tick events, timer callbacks and the variable-manager flush all
#   ride that hook. timer-tick-subscribe plus
#   timer-tick-counts is its direct witness, and it reads the hook's *second argument* as well:
#   PreUpdate takes GameTime* and Ext.Events.Tick carries it whole, so Ticks increasing and a
#   DeltaTime in the 1/30..1/300 s band say the pointer is right; a DeltaTime of 0 or a wild
#   Ticks would say the Linux register use is not what the row assumes.
#   timer-waitfor takes the same question from the other end. Note what upstream does *not*
#   do here: TimerSystem::Update
#   hands the **same** time to both managers (Timer.inl:286-287, as upstream),
#   so WaitForRealtime is not a second clock on this revision -- realtime_ and game_ differ
#   only by the RealtimeFlag in the handle, which routes Pause/Resume/Cancel, and by
#   persistence. So `game fired=0 with realtime fired=1` is *not* a GameTime-does-not-advance
#   reading; both ride time.Time and both fire or neither does. The flagword field is the
#   witness for the routing: 0 for WaitFor, 2 for WaitForRealtime (0x200000000 >> 32), 1 for
#   WaitForPersistent (0x100000000 >> 32).
#   timer-pause is the calibration for the handle table rather than for the tick: seven values
#   in one chunk, and the last two are the two-sided part -- the first Cancel must be true and
#   the second false, so a Cancel that returns true for a handle it has already dropped would
#   be caught. timer-realtime-pause asks the same of the other manager.
#   timer-persistent registers a handler and a 5 s persistent timer and reports the handle; it
#   prints SWEEP-TIMER when it fires. The restore half is **not** claimed: WaitForPersistent
#   serialises through esv__OsirisVariableHelper__SavegameVisit 0x4198970, so a full witness
#   needs a save *and* a load, which one sweep run does not do -- the same caveat already
#   documented for pvars-read above.
set -u
here=$(dirname "$(realpath "$0")")
state=${XDG_STATE_HOME:-$HOME/.local/state}/bg3-se-linux
log=$state/extender.log
probes=(
    "globals:Ext.Osiris ~= nil and Osi ~= nil"
    "host:Osi.GetHostCharacter()"
    "isplayer:Osi.IsPlayer(Osi.GetHostCharacter())"
    "query-out:select(2, pcall(Osi.GetPosition, Osi.GetHostCharacter()))"
    "call:Osi.DebugBreak('bg3se-linux osi-sweep') or 'called'"
    "db-get:#Osi.DB_Players:Get(nil)"
    "db-get-row:Osi.DB_Players:Get(nil)[1][1]"
    "listener:Ext.Osiris.RegisterListener('TurnEnded', 1, 'after', function() end) or 'registered'"
    "timer-listener:Ext.Osiris.RegisterListener('TimerFinished', 1, 'after', function(t) Ext.Utils.Print('SWEEP-TIMER ' .. tostring(t)) end) or 'registered'"
    "proc-timer:Osi.PROC_Debug_StartRecurrent('bg3se_sweep', 1, 1) or 'sent'"
    "db-insert:Osi.DB_NOOP(4242) or 'inserted'"
    "db-count-inserted:#Osi.DB_NOOP:Get(4242)"
    "db-delete:Osi.DB_NOOP:Delete(4242) or 'deleted'"
    "db-count-deleted:#Osi.DB_NOOP:Get(4242)"
    "proc-status:Osi.ApplyStatus(Osi.GetHostCharacter(), 'BLESS', 30.0, 1) or 'sent'"
    "status-check:Osi.HasActiveStatus(Osi.GetHostCharacter(), 'BLESS')"
    "mod:Ext.Mod.GetMod('787ce468-859f-4e07-83e2-61c31139e1bc')"
    "mod-name:Ext.Mod.GetMod('787ce468-859f-4e07-83e2-61c31139e1bc').Info.Name"
    "mod-dir:Ext.Mod.GetMod('787ce468-859f-4e07-83e2-61c31139e1bc').Info.Directory"
    "mod-base:Ext.Mod.GetModManager().BaseModule.Info.Name"
    "loadorder:#Ext.Mod.GetLoadOrder()"
    "version:Ext.Utils.Version()"
    "game-version:Ext.Utils.GameVersion()"
    # GlobalSwitches -- the GetGlobalSwitches() probe for the two places upstream's mirror
    # differs from the native layout: SoundSetting's stride (wrong, it shows as a refusal of
    # `MuteSoundWhenNotFocused`) and two floats before TimelinesAD plus an eight-byte tail that
    # takes sizeof to the game's 0x1440 (wrong, a refusal of `TimelinesEmoteMimicry`).
    # The *first* of them is the witness: under RuntimeCheckLevel::Once the first push of the
    # object runs upstream's property validator, so `gswitches` returning nil means a
    # `Validation of property 'X' failed on GlobalSwitches` line was just written to extender.log,
    # and returning `GlobalSwitches` means the class validated whole.  The other three read the
    # class on both sides of the drifts: `-front` is the part in front of them (Language 0xc0 and
    # the story flags), `-past` is everything after ScreenshotDir where
    # they live, and `-validate` re-runs the validator explicitly once the class is proxied
    # at all (Ext.Types.Validate bypasses the Once cache, so it answers every launch).
    #
    # The expected answer is **`GlobalSwitches`**, not `nil`, with `gswitches-past` and
    # `gswitches-tail` printing real values.  A `nil` naming `TimelinesEmoteMimicry` means the
    # Timelines layout regressed; naming `MuteSoundWhenNotFocused` means SoundSetting's stride
    # did; naming anything past the Timelines run means a third drift, and `gswitches-tail`
    # says how far the good part goes.
    "gswitches:(function() local g = Ext.Utils.GetGlobalSwitches() if g == nil then return 'nil -- proxying refused; the Validation of property line for GlobalSwitches is in extender.log just above this' end return Ext.Types.GetObjectType(g) end)()"
    "gswitches-front:(function() local g = Ext.Utils.GetGlobalSwitches() if g == nil then return 'nil -- see gswitches' end return 'Language=' .. tostring(g.Language) .. ' Story=' .. tostring(g.Story) .. ' StoryLog=' .. tostring(g.StoryLog) .. ' SaveStory=' .. tostring(g.SaveStory) .. ' Stats=' .. tostring(g.Stats) .. ' StoryEvents=' .. tostring(g.StoryEvents) .. ' ScreenshotDir=' .. tostring(g.ScreenshotDir) end)()"
    "gswitches-past:(function() local g = Ext.Utils.GetGlobalSwitches() if g == nil then return 'nil -- see gswitches' end return 'MuteSoundWhenNotFocused=' .. tostring(g.MuteSoundWhenNotFocused) .. ' CentreDialogue=' .. tostring(g.CentreDialogue) .. ' ForceMono=' .. tostring(g.ForceMono) .. ' SoundVolumeDynamicRange=' .. tostring(g.SoundVolumeDynamicRange) .. ' SoundOcclusionQuality=' .. tostring(g.SoundOcclusionQuality) .. ' GameCameraRotation=' .. tostring(g.GameCameraRotation) .. ' GameCameraSpeedMultiplier=' .. tostring(g.GameCameraSpeedMultiplier) .. ' UseLevelCache=' .. tostring(g.UseLevelCache) .. ' GodMode=' .. tostring(g.GodMode) end)()"
    # `-tail` is everything past the second drift: the Timelines bool run (game 0x1344), the
    # flags before the string (0x13e7, 0x13ea), the float at 0x13f0, the STDString at 0x13f8
    # and the two ints at 0x1418/0x141c -- the members the layout is measured on.  With the
    # layout wrong this cannot run at all (the validator refuses the whole class); real values
    # here are the in-game witness.  ActiveCustomDice is a string, StartYear/StartDay ints.
    "gswitches-tail:(function() local g = Ext.Utils.GetGlobalSwitches() if g == nil then return 'nil -- see gswitches' end return 'TimelinesAD=' .. tostring(g.TimelinesAD) .. ' TimelinesEmoteMimicry=' .. tostring(g.TimelinesEmoteMimicry) .. ' TimelinesFade=' .. tostring(g.TimelinesFade) .. ' ShowFirstTimeSetup=' .. tostring(g.ShowFirstTimeSetup) .. ' PointAndClickSoundFrequency=' .. tostring(g.PointAndClickSoundFrequency) .. ' ActiveCustomDice=' .. tostring(g.ActiveCustomDice) .. ' StartYear=' .. tostring(g.StartYear) .. ' StartDay=' .. tostring(g.StartDay) .. ' HideCompletedQuests=' .. tostring(g.HideCompletedQuests) end)()"
    "gswitches-validate:(function() local g = Ext.Utils.GetGlobalSwitches() if g == nil then return 'unreachable -- proxying is refused, so there is no proxy to validate; see gswitches' end return tostring(Ext.Types.Validate(g)) end)()"
    "loca-set:Ext.Loca.UpdateTranslatedString('hbg3se_sweep', 'bg3se-linux loca ok') and 'set'"
    "loca-get:Ext.Loca.GetTranslatedString('hbg3se_sweep')"
    "template:select(2, pcall(Ext.Template.GetAllRootTemplates))"
    "template-count:(function() local n = 0 for _ in pairs(Ext.Template.GetAllRootTemplates()) do n = n + 1 end return n end)()"
    "template-one:(function() for k, v in pairs(Ext.Template.GetAllRootTemplates()) do return tostring(k) .. ' ' .. tostring(v.Name) end end)()"
    "gcp-fields:(function() local n, withp, bad, first = 0, 0, 0, nil for k, v in pairs(Ext.Template.GetAllRootTemplates()) do n = n + 1 local ok, ps = pcall(function() return v.ConstellationConfigGlobalParameters end) if ok and ps ~= nil and #ps > 0 then withp = withp + 1 for i = 1, #ps do local okn, nm = pcall(function() return ps[i].Name end) local okv, val = pcall(function() return ps[i].Value end) if not okn or type(nm) ~= 'string' or not okv then bad = bad + 1 if first == nil then first = 'refused ' .. tostring(k) .. ': name=' .. tostring(okn and nm or 'error') .. ' value=' .. tostring(okv and 'ok' or 'error') end elseif first == nil then first = tostring(k) .. ' #' .. #ps .. ' [1]=' .. tostring(nm) .. '=' .. type(val) .. '(' .. tostring(type(val) == 'table' and '#' .. #val or val) .. ')' end end end end return withp .. '/' .. n .. ' root templates with ConstellationConfigGlobalParameters, ' .. bad .. ' with an unreadable parameter; first: ' .. tostring(first) .. ' (libc++ pads a std::variant nested in another by 8 bytes at the front and ls::Variant does not, which put both of GlobalConfigParameter discriminants 8 bytes high and had the validator refuse ItemTemplate with it -- a refusal, a non-string Name or a nil Value here is the variant layout regressing -- the libc++ sfinae-tag overlay is what closes the hole)' end)()"
    "sexpr-params:(function() local kinds = {'RollBonusBoost', 'SkillBoost', 'WeaponDamageBoost', 'CharacterWeaponDamageBoost', 'DamageBonusBoost', 'WeaponAttackRollBonusBoost', 'CharacterUnarmedDamageBoost'} local exprs, params, bad, first = 0, 0, 0, nil for _, k in ipairs(kinds) do local oke, es = pcall(Ext.Entity.GetAllEntitiesWithComponent, k) if oke and es ~= nil then for i = 1, #es do local okc, c = pcall(function() return es[i][k] end) local ex = okc and c ~= nil and c.Amount or nil if type(ex) == 'userdata' then local okp, ps = pcall(function() return ex.Params end) local okcode, code = pcall(function() return ex.Code end) if not okp or ps == nil or not okcode or type(code) ~= 'string' then bad = bad + 1 if first == nil then first = 'refused ' .. k .. ': params=' .. tostring(okp and ps ~= nil) .. ' code=' .. tostring(okcode and type(code)) end else exprs = exprs + 1 local shown = nil for j = 1, #ps do local okv, v = pcall(function() return ps[j] end) if not okv then bad = bad + 1 if first == nil then first = 'refused ' .. k .. ' param ' .. j .. ' of ' .. #ps end else params = params + 1 if shown == nil then shown = type(v) .. '(' .. tostring(v) .. ')' end end end if first == nil and shown ~= nil then first = k .. ' code=' .. string.sub(code, 1, 40) .. ' #' .. #ps .. ' [1]=' .. shown end end end end end end return exprs .. ' stats expressions, ' .. params .. ' params read, ' .. bad .. ' refused; first: ' .. tostring(first) .. ' (StatsExpressionInternal::Param is a std::variant whose alternative 1 is another std::variant, so libc++ put 8 bytes of leading padding in front of the union: the outer discriminant sat at +0x20 and the inner at +0x18 of a 40-byte element where bin/bg3 0x352bc90 strides 0x20 and reads them at +0x18 and +0x10.  A refusal, a non-string Code or an unreadable param is the variant layout regressing (the libc++ sfinae-tag overlay closes the hole); 0 stats expressions means this save has no boost carrying one and the probe says nothing either way)' end)()"
    "ecs-types:#Ext.Entity.GetRegisteredComponentTypes()"
    "ecs-types-mapped:#Ext.Entity.GetRegisteredComponentTypes(nil, true)"
    "ecs-types-one:Ext.Entity.GetRegisteredComponentTypes()[1]"
    "ecs-fixed-mapped:(function() local want = {'esv::death::DelayedDeathComponent', 'esv::death::DeathRequestOneFrameComponent', 'esv::SoundVolumeTrigger', 'esv::TimelineSceneTrigger', 'ecl::TimelineSceneTrigger', 'ls::Construction', 'esv::Projectile', 'esv::projectile::InitializationComponent'} local have = {} for _, v in ipairs(Ext.Entity.GetRegisteredComponentTypes(nil, true)) do have[v] = true end local n, miss = 0, '' for _, v in ipairs(want) do if have[v] then n = n + 1 else miss = miss .. ' ' .. v end end return n .. '/' .. #want .. miss end)()"
    "ecs-never-broken:(function() local have = {} for _, v in ipairs(Ext.Entity.GetRegisteredComponentTypes(nil, true)) do have[v] = true end return tostring(have['ecl::SoundVolumeTrigger'] == true) end)()"
    "ecs-uuid:tostring(Ext.Entity.UuidToHandle(Osi.GetHostCharacter()))"
    "ecs-host:tostring(Ext.Entity.Get(Osi.GetHostCharacter()))"
    "ecs-host-comps:#Ext.Entity.Get(Osi.GetHostCharacter()):GetAllComponentNames(true)"
    "ecs-host-uuid:tostring(Ext.Entity.Get(Osi.GetHostCharacter()).Uuid.EntityUuid)"
    "level-info:tostring(Ext.Level.GetLevelInfo(Osi.GetRegion(Osi.GetHostCharacter())))"
    "level-state:tostring(Ext.Level.GetLevelInfo(Osi.GetRegion(Osi.GetHostCharacter())).State)"
    "level-tracked:(function() local li = Ext.Level.GetLevelInfo(Osi.GetRegion(Osi.GetHostCharacter())) local keys, tmpl = 0, 0 for k, v in pairs(li.TrackedTemplates) do keys = keys + 1 tmpl = tmpl + #v end return keys .. ' keys ' .. tmpl .. ' templates' end)()"
    "cache-templates:(function() local n = 0 for _ in pairs(Ext.Template.GetAllCacheTemplates()) do n = n + 1 end return n end)()"
    "cache-create:(function() local before = 0 for _ in pairs(Ext.Template.GetAllCacheTemplates()) do before = before + 1 end local t = Ext.Entity.Get(Osi.GetHostCharacter()).ServerCharacter:CreateCacheTemplate() local after = 0 for _ in pairs(Ext.Template.GetAllCacheTemplates()) do after = after + 1 end return tostring(t and t.Id) .. ' ' .. before .. '->' .. after end)()"
    "res-banks:(function() local out = {} for _, t in ipairs({'Visual','Texture','Material','Sound','VirtualTexture','TileSet'}) do out[#out+1] = t .. '=' .. #Ext.Resource.GetAll(t) end return table.concat(out, ' ') end)()"
    "vt-count:#Ext.Resource.GetAll('VirtualTexture')"
    "vt-one:(function() local ids = Ext.Resource.GetAll('VirtualTexture') local r = Ext.Resource.Get(ids[1], 'VirtualTexture') return tostring(r.RootPath) .. ' gts=' .. tostring(r.GTSIndex) .. ' gtex=' .. tostring(r.GTexFileName) .. ' tileset=' .. tostring(r.TileSetFileName) .. ' valid=' .. tostring(r.IsValid) end)()"
    "vt-all-gtex:(function() local ids = Ext.Resource.GetAll('VirtualTexture') local named = 0 for _, id in ipairs(ids) do local r = Ext.Resource.Get(id, 'VirtualTexture') if r and r.GTexFileName ~= nil and tostring(r.GTexFileName) ~= '' then named = named + 1 end end return named .. '/' .. #ids end)()"
    "res-anim-one:(function() local ids = Ext.Resource.GetAll('Animation') local r = Ext.Resource.Get(ids[1], 'Animation') return tostring(r.Template) .. ' skel=' .. tostring(r.SkeletonResource) .. ' step=' .. tostring(r.TimeStep) .. ' dur=' .. tostring(r.Duration) .. ' off=' .. tostring(r.Offset) .. ' loop=' .. tostring(r.Looping) .. ' events=' .. tostring(#r.Events) end)()"
    "res-anim-durations:(function() local ids = Ext.Resource.GetAll('Animation') local n, good = 0, 0 for i = 1, math.min(#ids, 3000) do local r = Ext.Resource.Get(ids[i], 'Animation') if r then n = n + 1 if r.Duration > 0 and r.Duration < 600 and r.TimeStep > 0 and r.TimeStep < 1 then good = good + 1 end end end return good .. '/' .. n end)()"
    "res-visual-one:(function() local ids = Ext.Resource.GetAll('Visual') local r = Ext.Resource.Get(ids[1], 'Visual') return 'objs=' .. tostring(#r.Objects) .. ' atts=' .. tostring(#r.Attachments) .. ' skel=' .. tostring(r.SkeletonResource) .. ' slot=' .. tostring(r.Slot) .. ' tmpl=' .. tostring(r.Template) end)()"
    "res-visual-objects:(function() local ids = Ext.Resource.GetAll('Visual') local n, good = 0, 0 for i = 1, math.min(#ids, 3000) do local r = Ext.Resource.Get(ids[i], 'Visual') if r then n = n + 1 local o = #r.Objects if o > 0 and o < 1000 then good = good + 1 end end end return good .. '/' .. n end)()"
    "res-blendspace-one:(function() local ids = Ext.Resource.GetAll('BlendSpace') local r = Ext.Resource.Get(ids[1], 'BlendSpace') return 'x=' .. tostring(r.XAxisName) .. ' y=' .. tostring(r.YAxisName) .. ' nx=' .. tostring(r.XElements) .. ' ny=' .. tostring(r.YElements) .. ' inputs=' .. tostring(#r.Inputs) end)()"
    "staticdata-feat:#Ext.StaticData.GetAll('Feat')"
    "staticdata-banks:(function() local out = {} for _, t in ipairs({'Feat','ActionResource','ClassDescription','Race','Background','Origin','God'}) do out[#out+1] = t .. '=' .. #Ext.StaticData.GetAll(t) end return table.concat(out, ' ') end)()"
    "staticdata-feat-one:(function() local ids = Ext.StaticData.GetAll('Feat') local r = Ext.StaticData.Get(ids[1], 'Feat') return tostring(r.Name) .. ' uuid=' .. tostring(r.ResourceUUID) .. ' multi=' .. tostring(r.CanBeTakenMultipleTimes) end)()"
    "staticdata-action-one:(function() local ids = Ext.StaticData.GetAll('ActionResource') local r = Ext.StaticData.Get(ids[1], 'ActionResource') return tostring(r.Name) .. ' uuid=' .. tostring(r.ResourceUUID) .. ' max=' .. tostring(r.MaxValue) .. ' lvl=' .. tostring(r.MaxLevel) end)()"
    "staticdata-sources:(function() local s = Ext.StaticData.GetSources('Feat') local mods, ids = 0, 0 for _, v in pairs(s) do mods = mods + 1 ids = ids + #v end return mods .. ' mods ' .. ids .. ' resources' end)()"
    "staticdata-bymod:(function() for k in pairs(Ext.StaticData.GetSources('Feat')) do return tostring(k) .. ' -> ' .. #Ext.StaticData.GetByModId('Feat', k) end return 'no sources' end)()"
    "net-version:Ext.Net.Version()"
    "net-ishost:tostring(Ext.Net.IsHost())"
    "net-host-userid:Ext.Entity.Get(Osi.GetHostCharacter()).ServerCharacter.UserID"
    "netid-host:tostring(Ext.Entity.Get(Osi.GetHostCharacter()):GetNetId())"
    "netid-roundtrip:(function() local e = Ext.Entity.Get(Osi.GetHostCharacter()) local n = e:GetNetId() local back = Ext.Entity.Get(n) return tostring(n) .. ' -> ' .. tostring(back and back.Uuid.EntityUuid == e.Uuid.EntityUuid) end)()"
    "net-channel:(function() if not _G.bg3se_sweep_ch then local ch = Ext.CoreLib('NetworkManager'):AddChannel('787ce468-859f-4e07-83e2-61c31139e1bc', 'bg3se_sweep') ch.MessageHandler = function(m, u) Ext.Utils.Print('SWEEP-NET server message text=' .. tostring(m.text) .. ' from ' .. tostring(u)) end ch.RequestHandler = function(req, u) Ext.Utils.Print('SWEEP-NET server request ping=' .. tostring(req.ping) .. ' from ' .. tostring(u)) return {pong = req.ping + 1} end _G.bg3se_sweep_ch = ch end return tostring(_G.bg3se_sweep_ch.Channel) end)()"
    "net-request:(function() _G.bg3se_sweep_ch:RequestToClient({ping = 41}, 65537, function(r) Ext.Utils.Print('SWEEP-NET server got reply pong=' .. tostring(r.pong)) end) return 'sent' end)()"
    "net-send:(function() _G.bg3se_sweep_ch:SendToClient({text = 'hello'}, 65537) return 'sent' end)()"
    "net-legacy:Ext.Net.BroadcastMessage('bg3se_sweep_legacy', Ext.Json.Stringify({text = 'legacy'})) or 'sent'"
    "vars-mod-register:(function() Ext.Vars.RegisterModVariable('787ce468-859f-4e07-83e2-61c31139e1bc', 'bg3seSweepModVar', {Server = true, Persistent = true}) return 'registered' end)()"
    "vars-mod-write:(function() local v = Ext.Vars.GetModVariables('787ce468-859f-4e07-83e2-61c31139e1bc') v.bg3seSweepModVar = 'sweep-' .. tostring(Ext.Timer.MonotonicTime()) return tostring(v.bg3seSweepModVar) end)()"
    "vars-mod-read:(function() local v = Ext.Vars.GetModVariables('787ce468-859f-4e07-83e2-61c31139e1bc') Ext.Vars.SyncModVariables() return tostring(v.bg3seSweepModVar) end)()"
    "vars-user-register:(function() Ext.Vars.RegisterUserVariable('bg3seSweepUserVar', {Server = true, Persistent = true}) return 'registered' end)()"
    "vars-user-roundtrip:(function() local e = Ext.Entity.Get(Osi.GetHostCharacter()) e.Vars.bg3seSweepUserVar = 'sweep-user' Ext.Vars.SyncUserVariables() return tostring(Ext.Entity.Get(Osi.GetHostCharacter()).Vars.bg3seSweepUserVar) end)()"
    "vars-user-entities:#Ext.Vars.GetEntitiesWithVariable('bg3seSweepUserVar')"
    "pvars-write:(function() local t = Mods.ExtenderSampleMod if t == nil then return 'no ExtenderSampleMod mod table' end t.PersistentVars = t.PersistentVars or {} t.PersistentVars.bg3seSweep = 'sweep-token-v1' t.PersistentVars.bg3seSweepCount = (t.PersistentVars.bg3seSweepCount or 0) + 1 return tostring(t.PersistentVars.bg3seSweep) .. ' n=' .. tostring(t.PersistentVars.bg3seSweepCount) end)()"
    "pvars-read:(function() local t = Mods.ExtenderSampleMod if t == nil or t.PersistentVars == nil then return 'no PersistentVars' end return tostring(t.PersistentVars.bg3seSweep) .. ' n=' .. tostring(t.PersistentVars.bg3seSweepCount) end)()"
    "ecb-type-mapped:(function() for _, v in ipairs(Ext.Entity.GetRegisteredComponentTypes(nil, true)) do if v == 'eoc::status::IDComponent' then return 'mapped' end end return 'NOT mapped' end)()"
    "ecb-subscribe:(function() if _G.bg3se_sweep_ecb == nil then local s = {create = 0, destroy = 0, last = nil} s.cid = Ext.Entity.OnCreate('StatusID', function(e, c) s.create = s.create + 1 s.last = tostring(e) end) s.did = Ext.Entity.OnDestroy('StatusID', function(e, c) s.destroy = s.destroy + 1 end) _G.bg3se_sweep_ecb = s end local s = _G.bg3se_sweep_ecb return 'create=' .. tostring(s.cid) .. ' destroy=' .. tostring(s.did) end)()"
    "ecb-apply:Osi.ApplyStatus(Osi.GetHostCharacter(), 'BLESS', 60.0, 1) or 'sent'"
    "ecb-active:tostring(Osi.HasActiveStatus(Osi.GetHostCharacter(), 'BLESS'))"
    "ecb-remove:Osi.RemoveStatus(Osi.GetHostCharacter(), 'BLESS') or 'sent'"
    "ecb-gone:tostring(Osi.HasActiveStatus(Osi.GetHostCharacter(), 'BLESS'))"
    "dmg-osi:type(Osi.ApplyDamage)"
    "dmg-subscribe:(function() if _G.bg3se_sweep_dmg == nil then local s = {deal = 0, dealt = 0, before = 0, last = nil} s.did = Ext.Events.DealDamage:Subscribe(function(e) s.deal = s.deal + 1 end) s.tid = Ext.Events.DealtDamage:Subscribe(function(e) s.dealt = s.dealt + 1 s.last = tostring(e.Target) end) s.bid = Ext.Events.BeforeDealDamage:Subscribe(function(e) s.before = s.before + 1 end) _G.bg3se_sweep_dmg = s end local s = _G.bg3se_sweep_dmg return 'deal=' .. tostring(s.did) .. ' dealt=' .. tostring(s.tid) .. ' before=' .. tostring(s.bid) end)()"
    "dmg-hp-before:(function() local e = Ext.Entity.Get(Osi.GetHostCharacter()) return tostring(e.Health and e.Health.Hp) end)()"
    "dmg-apply:(function() local h = Osi.GetHostCharacter() local e = Ext.Entity.Get(h) local hp = (e.Health and e.Health.Hp) or -1 if hp <= 10 then return 'skipped: hp=' .. tostring(hp) end Osi.ApplyDamage(h, 1, 'Slashing', h) return 'sent hp=' .. tostring(hp) end)()"
    "dmg-hp-after:(function() local e = Ext.Entity.Get(Osi.GetHostCharacter()) return tostring(e.Health and e.Health.Hp) end)()"
    "dmg-counts:(function() local s = _G.bg3se_sweep_dmg if s == nil then return 'not subscribed' end return 'deal=' .. s.deal .. ' dealt=' .. s.dealt .. ' before=' .. s.before .. ' last=' .. tostring(s.last) end)()"
    "notify:Osi.ShowNotification(Osi.GetHostCharacter(), 'bg3se-linux: Osi.* is live') or 'shown'"
    "ecb-counts:(function() local s = _G.bg3se_sweep_ecb if s == nil then return 'not subscribed' end return 'create=' .. s.create .. ' destroy=' .. s.destroy .. ' last=' .. tostring(s.last) end)()"
    "proto-pick:(function() local sp = Ext.Stats.Get('Projectile_MagicMissile') and 'Projectile_MagicMissile' or Ext.Stats.GetStats('SpellData')[1] local st = Ext.Stats.Get('BLESS') and 'BLESS' or Ext.Stats.GetStats('StatusData')[1] local it = Ext.Stats.GetStats('InterruptData')[1] local pa = Ext.Stats.GetStats('PassiveData')[1] _G.bg3se_sweep_proto = {spell = sp, status = st, interrupt = it, passive = pa} return 'spell=' .. tostring(sp) .. ' status=' .. tostring(st) .. ' interrupt=' .. tostring(it) .. ' passive=' .. tostring(pa) .. ' spells=' .. #Ext.Stats.GetStats('SpellData') .. ' statuses=' .. #Ext.Stats.GetStats('StatusData') .. ' interrupts=' .. #Ext.Stats.GetStats('InterruptData') .. ' passives=' .. #Ext.Stats.GetStats('PassiveData') end)()"
    "proto-spell-cached:(function() local n = _G.bg3se_sweep_proto.spell local p = Ext.Stats.GetCachedSpell(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end return 'SpellId=' .. tostring(p.SpellId) .. ' Level=' .. tostring(p.Level) .. ' School=' .. tostring(p.SpellSchool) .. ' Flags=' .. (type(p.SpellFlags) == 'table' and ('#' .. #p.SpellFlags) or tostring(p.SpellFlags)) .. ' Idx=' .. tostring(p.StatsObjectIndex) end)()"
    "proto-status-cached:(function() local n = _G.bg3se_sweep_proto.status local p = Ext.Stats.GetCachedStatus(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end return 'StatusName=' .. tostring(p.StatusName) .. ' Flags=' .. tostring(p.Flags) .. ' HitAnim=' .. tostring(p.HitAnimationType) .. ' Absorb=' .. tostring(p.AbsorbSurfaceTypes and #p.AbsorbSurfaceTypes) .. ' Idx=' .. tostring(p.StatsObjectIndex) end)()"
    "proto-spell-sync:(function() local n = _G.bg3se_sweep_proto.spell local p = Ext.Stats.GetCachedSpell(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end local school, level = p.SpellSchool, p.Level p.SpellSchool, p.Level = 0, 0 if p.SpellSchool ~= 0 or p.Level ~= 0 then return 'could not zero the prototype: School=' .. tostring(p.SpellSchool) .. ' Level=' .. tostring(p.Level) end Ext.Stats.Sync(n) p = Ext.Stats.GetCachedSpell(n) if p == nil then return 'synced, no cached prototype' end local ran = (tostring(p.SpellSchool) == tostring(school) and tostring(p.Level) == tostring(level)) return (ran and 'Init ran: ' or 'Init DID NOT run: ') .. 'School=' .. tostring(p.SpellSchool) .. '/' .. tostring(school) .. ' Level=' .. tostring(p.Level) .. '/' .. tostring(level) .. ' SpellId=' .. tostring(p.SpellId) .. ' Idx=' .. tostring(p.StatsObjectIndex) end)()"
    "proto-status-sync:(function() local n = _G.bg3se_sweep_proto.status local p = Ext.Stats.GetCachedStatus(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end local hit = p.HitAnimationType local dirty = (tostring(hit) == '3') and 4 or 3 p.HitAnimationType = dirty if tostring(p.HitAnimationType) ~= tostring(dirty) then return 'could not dirty the prototype: HitAnim=' .. tostring(p.HitAnimationType) end Ext.Stats.Sync(n) p = Ext.Stats.GetCachedStatus(n) if p == nil then return 'synced, no cached prototype' end local ran = (tostring(p.HitAnimationType) == tostring(hit)) return (ran and 'Init ran: ' or 'Init DID NOT run: ') .. 'HitAnim=' .. tostring(p.HitAnimationType) .. '/' .. tostring(hit) .. ' (dirtied to ' .. tostring(dirty) .. ') StatusName=' .. tostring(p.StatusName) .. ' Flags=' .. tostring(p.Flags) .. ' Idx=' .. tostring(p.StatsObjectIndex) end)()"
    "proto-interrupt-cached:(function() local n = _G.bg3se_sweep_proto.interrupt if n == nil then return 'no InterruptData entries' end local p = Ext.Stats.GetCachedInterrupt(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end return 'Name=' .. tostring(p.Name) .. ' Context=' .. tostring(p.InterruptContext) .. ' Scope=' .. tostring(p.InterruptContextScope) .. ' Container=' .. tostring(p.Container) .. ' Stack=' .. tostring(p.Stack) .. ' Default=' .. tostring(p.InterruptDefaultValue) .. ' Flags=' .. tostring(p.InterruptFlags) .. ' Costs=' .. tostring(p.Costs and #p.Costs) end)()"
    "proto-interrupt-sync:(function() local n = _G.bg3se_sweep_proto.interrupt if n == nil then return 'no InterruptData entries' end local p = Ext.Stats.GetCachedInterrupt(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end local ctx, costs = p.InterruptContext, (p.Costs and #p.Costs) local dirty = (tostring(ctx) == '7') and 5 or 7 p.InterruptContext = dirty if tostring(p.InterruptContext) ~= tostring(dirty) then return 'could not dirty the prototype: Context=' .. tostring(p.InterruptContext) end Ext.Stats.Sync(n) p = Ext.Stats.GetCachedInterrupt(n) if p == nil then return 'synced, no cached prototype' end local ran = (tostring(p.InterruptContext) == tostring(ctx)) if not ran then p.InterruptContext = ctx end return (ran and 'Init ran: ' or 'Init DID NOT run: ') .. 'Context=' .. tostring(p.InterruptContext) .. '/' .. tostring(ctx) .. ' (dirtied to ' .. tostring(dirty) .. ') Name=' .. tostring(p.Name) .. ' Costs=' .. tostring(p.Costs and #p.Costs) .. '/' .. tostring(costs) end)()"
    "proto-passive-cached:(function() local n = _G.bg3se_sweep_proto.passive if n == nil then return 'no PassiveData entries' end local p = Ext.Stats.GetCachedPassive(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end return 'Name=' .. tostring(p.Name) .. ' Properties=' .. tostring(p.Properties) .. ' Priority=' .. tostring(p.PriorityOrder) .. ' ToggleGroup=' .. tostring(p.ToggleGroup) .. ' Boosts=' .. tostring(p.Boosts and #p.Boosts) end)()"
    "proto-passive-sync-absent:(function() local n = _G.bg3se_sweep_proto.passive if n == nil then return 'no PassiveData entries' end local p = Ext.Stats.GetCachedPassive(n) if p == nil then return 'no cached prototype for ' .. tostring(n) end local prio = p.PriorityOrder local dirty = prio + 17 p.PriorityOrder = dirty if tostring(p.PriorityOrder) ~= tostring(dirty) then return 'could not dirty the prototype: Priority=' .. tostring(p.PriorityOrder) end Ext.Stats.Sync(n) p = Ext.Stats.GetCachedPassive(n) if p == nil then return 'synced, no cached prototype' end local ran = (tostring(p.PriorityOrder) ~= tostring(dirty)) p.PriorityOrder = prio local restored = (tostring(p.PriorityOrder) == tostring(prio)) return (ran and 'UNEXPECTED: something ran: ' or 'refused as designed: ') .. 'Priority=' .. tostring(dirty) .. ' survived the Sync, restored=' .. tostring(restored) .. ' Name=' .. tostring(p.Name) end)()"
    "proto-status-live:(function() local h = Osi.GetHostCharacter() Osi.ApplyStatus(h, _G.bg3se_sweep_proto.status, 12.0, 1) return 'applied ' .. tostring(_G.bg3se_sweep_proto.status) .. ' active=' .. tostring(Osi.HasActiveStatus(h, _G.bg3se_sweep_proto.status)) end)()"
    "dmg-counts-late:(function() local s = _G.bg3se_sweep_dmg if s == nil then return 'not subscribed' end return 'deal=' .. s.deal .. ' dealt=' .. s.dealt .. ' before=' .. s.before .. ' last=' .. tostring(s.last) end)()"
    "aigrid-event:tostring(Ext.Events.FindPath ~= nil)"
    "aigrid-subscribe:(function() if _G.bg3se_sweep_aigrid == nil then local s = {n = 0, done = 0, first = nil} s.id = Ext.Events.FindPath:Subscribe(function(e) s.n = s.n + 1 if s.first == nil then local ok, desc = pcall(function() local q = e.Path if q == nil then return 'nil path' end return 'type=' .. Ext.Types.GetObjectType(q) .. ' started=' .. tostring(q.SearchStarted) .. ' complete=' .. tostring(q.SearchComplete) .. ' goal=' .. tostring(q.GoalFound) .. ' inuse=' .. tostring(q.InUse) .. ' nodes=' .. tostring(q.Nodes and #q.Nodes) .. ' checkpoints=' .. tostring(q.Checkpoints and #q.Checkpoints) .. ' close=' .. tostring(q.CloseEnoughMin) .. '/' .. tostring(q.CloseEnoughMax) .. ' step=' .. tostring(q.StepHeight) .. ' player=' .. tostring(q.IsPlayer) end) s.first = ok and desc or ('raised: ' .. tostring(desc)) end end) _G.bg3se_sweep_aigrid = s end return 'id=' .. tostring(_G.bg3se_sweep_aigrid.id) end)()"
    "aigrid-begin:(function() local h = Osi.GetHostCharacter() if h == nil then return 'no host character' end local ent = Ext.Entity.Get(h) if ent == nil then return 'no entity for ' .. tostring(h) end local x, y, z = Osi.GetPosition(h) if x == nil then return 'no position for ' .. tostring(h) end local s = _G.bg3se_sweep_aigrid local ok, path = pcall(Ext.Level.BeginPathfinding, ent, {x + 4.0, y, z + 4.0}, function() if s ~= nil then s.done = s.done + 1 end end) if not ok then return 'raised: ' .. tostring(path) end if path == nil then return 'nil -- no level, or CreatePath refused' end _G.bg3se_sweep_path = path local active = #Ext.Level.GetActivePathfindingRequests() return 'path=' .. Ext.Types.GetObjectType(path) .. ' active=' .. tostring(active) .. ' inuse=' .. tostring(path.InUse) .. ' complete=' .. tostring(path.SearchComplete) .. ' from ' .. string.format('%.1f,%.1f,%.1f', x, y, z) end)()"
    "aigrid-requests:(function() local ok, r = pcall(Ext.Level.GetActivePathfindingRequests) if not ok then return 'raised: ' .. tostring(r) end return 'active=' .. tostring(#r) .. ' (drained by now; aigrid-begin holds the calibration)' end)()"
    "aigrid-sync:(function() local h = Osi.GetHostCharacter() if h == nil then return 'no host character' end local ent = Ext.Entity.Get(h) if ent == nil then return 'no entity' end local x, y, z = Osi.GetPosition(h) if x == nil then return 'no position' end local ok, q = pcall(Ext.Level.BeginPathfindingImmediate, ent, {x + 2.0, y, z + 2.0}) if not ok then return 'BeginPathfindingImmediate raised: ' .. tostring(q) end if q == nil then return 'nil -- no level, or CreatePath refused' end local ok2, r = pcall(Ext.Level.FindPath, q) if not ok2 then return 'FindPath raised: ' .. tostring(r) end return 'returned ' .. tostring(r) .. ' complete=' .. tostring(q.SearchComplete) .. ' goal=' .. tostring(q.GoalFound) .. ' nodes=' .. tostring(q.Nodes and #q.Nodes) .. ' checkpoints=' .. tostring(q.Checkpoints and #q.Checkpoints) .. ' error=' .. tostring(q.ErrorCause) end)()"
    "io-root:(function() local t = 'bg3se-sweep-' .. tostring(Ext.Timer.MonotonicTime()) _G.bg3se_sweep_io = {token = t, rel = 'bg3se_sweep/io-probe.txt', app = 'bg3se_sweep/io-append.txt'} return 'token=' .. t .. ' rel=' .. _G.bg3se_sweep_io.rel .. ' (nested on purpose: the first write has to mkdir bg3se_sweep/ through the win32 shim)' end)()"
    "io-save:(function() local s = _G.bg3se_sweep_io if s == nil then return 'io-root did not run' end return 'SaveFile=' .. tostring(Ext.IO.SaveFile(s.rel, s.token)) .. ' rel=' .. s.rel end)()"
    "io-load:(function() local s = _G.bg3se_sweep_io if s == nil then return 'io-root did not run' end local r = Ext.IO.LoadFile(s.rel) return (r == s.token and 'round-trip ok ' or 'MISMATCH ') .. 'len=' .. tostring(r and #r) .. ' got=' .. tostring(r) end)()"
    "io-append:(function() local s = _G.bg3se_sweep_io if s == nil then return 'io-root did not run' end local w = Ext.IO.SaveFile(s.app, 'AAAA') local a = Ext.IO.AppendFile(s.app, 'BBB') local r = Ext.IO.LoadFile(s.app) return 'SaveFile=' .. tostring(w) .. ' AppendFile=' .. tostring(a) .. ' file=' .. tostring(r) .. ' len=' .. tostring(r and #r) .. ' -- upstream opens ios::out (truncating) and only then seeks to the end, so BBB/3 is the expected reading and AAAABBB/7 would mean this port diverged' end)()"
    "io-data:(function() local p = 'Mods/ExtenderSampleMod/ScriptExtender/Config.json' local r = Ext.IO.LoadFile(p, 'data') if r == nil then return 'nil -- the pak reader gave nothing for ' .. p .. ' (MakeFileReaderAbsolute, or the Data root)' end return 'len=' .. tostring(#r) .. ' (88 read offline with lspk.py) ModTable=' .. tostring(r:match('ModTable[^:]*:%s*.([%w_]+)')) end)()"
    "io-override:(function() local a, b = 'Public/bg3se_sweep/from.lsx', 'Public/bg3se_sweep/to.lsx' local before = Ext.IO.GetPathOverride(a) Ext.IO.AddPathOverride(a, b) local after = Ext.IO.GetPathOverride(a) return 'before=' .. tostring(before) .. ' after=' .. tostring(after) .. ' unknown=' .. tostring(Ext.IO.GetPathOverride('Public/bg3se_sweep/never.lsx')) .. ' -- after= is ToPath(Data) as the game composed it; before and unknown must be nil' end)()"
    "io-refuse-traversal:(function() local ok = Ext.IO.SaveFile('bg3se_sweep/../escaped.txt', 'x') return 'SaveFile=' .. tostring(ok) .. ' (must be false; IsSafeRelativePath refuses any .. and logs its own OsiError)' end)()"
    "io-refuse-chars:(function() local a = Ext.IO.SaveFile('bg3se_sweep/co:lon.txt', 'x') local b = Ext.IO.SaveFile('bg3se_sweep/star*.txt', 'x') local c = Ext.IO.SaveFile('', 'x') local d = Ext.IO.SaveFile('bg3se_sweep/trailing.', 'x') return 'colon=' .. tostring(a) .. ' star=' .. tostring(b) .. ' empty=' .. tostring(c) .. ' trailing-dot=' .. tostring(d) .. ' (all four must be false -- Windows naming rules applied on Linux by design, not a port defect)' end)()"
    "io-load-missing:(function() local r = Ext.IO.LoadFile('bg3se_sweep/absent-' .. tostring(Ext.Timer.MonotonicTime()) .. '.txt') return 'nil expected, got ' .. tostring(r) end)()"
    "io-mcm-save:(function() local m = Mods and Mods.BG3MCM if m == nil then return 'no BG3MCM mod table in this context -- nothing claimed' end local cfg = m.ModConfig if type(cfg) ~= 'table' or type(cfg.SaveAllSettings) ~= 'function' then return 'BG3MCM is reachable but ModConfig:SaveAllSettings is not (' .. type(cfg) .. ') -- nothing claimed' end local ok, err = pcall(function() cfg:SaveAllSettings() end) return ok and 'MCM rewrote its own settings through Ext.IO.SaveFile -- one Script Extender/BG3MCM/Profiles/Default/<mod>/settings.json per mod it holds settings for; check their mtimes' or ('raised: ' .. tostring(err)) end)()"
    "timer-clocks:(function() local m = Ext.Timer.MonotonicTime() local u = Ext.Timer.MicrosecTime() local g = Ext.Timer.GameTime() local e = Ext.Timer.ClockEpoch() local c = Ext.Timer.ClockTime() _G.bg3se_sweep_timer = {m = m, g = g, u = u} return 'Monotonic=' .. tostring(m) .. 'ms Microsec=' .. string.format('%.0f', u) .. 'us GameTime=' .. string.format('%.3f', g) .. 's ClockEpoch=' .. tostring(e) .. ' ClockTime=' .. tostring(c) end)()"
    "timer-advance:(function() local s = _G.bg3se_sweep_timer if s == nil then return 'timer-clocks did not run' end local m, u, g = Ext.Timer.MonotonicTime(), Ext.Timer.MicrosecTime(), Ext.Timer.GameTime() return string.format('dMonotonic=%dms dMicrosec=%.0fus dGameTime=%.3fs -- the first two are the QPC shim over CLOCK_MONOTONIC and want one probe interval (~700ms); the third is the game GameTime carried in by the server hook, and near 0 there would mean the tick is not reaching Lua', m - s.m, u - s.u, g - s.g) end)()"
    "timer-tick-subscribe:(function() if _G.bg3se_sweep_tick == nil then local s = {n = 0, first = nil, last = nil, t0 = nil, t1 = nil, mono = 0, scaled = 0} s.id = Ext.Events.Tick:Subscribe(function(e) s.n = s.n + 1 local ok, d = pcall(function() local t = e.Time if s.t1 ~= nil and t.Ticks > s.t1 then s.mono = s.mono + 1 end if s.t0 == nil then s.t0 = t.Ticks end s.t1 = t.Ticks if math.abs(t.DeltaTime - t.UnscaledDeltaTime) < 1e-9 then s.scaled = s.scaled + 1 end return string.format('Time=%.3f DeltaTime=%.5f UnscaledDeltaTime=%.5f Ticks=%d', t.Time, t.DeltaTime, t.UnscaledDeltaTime, t.Ticks) end) s.last = ok and d or ('raised: ' .. tostring(d)) if s.first == nil then s.first = s.last end end) _G.bg3se_sweep_tick = s end return 'id=' .. tostring(_G.bg3se_sweep_tick.id) .. ' (the subscription exists whether or not the hook ever fires)' end)()"
    "timer-waitfor:(function() if _G.bg3se_sweep_waits == nil then _G.bg3se_sweep_waits = {game = 0, real = 0} end local w = _G.bg3se_sweep_waits local function fw(h) if math.type(h) ~= 'integer' then return type(h) .. '/' .. tostring(math.type(h)) end return tostring(h // 4294967296) end w.gh = Ext.Timer.WaitFor(500, function() w.game = w.game + 1 end) w.rh = Ext.Timer.WaitForRealtime(500, function() w.real = w.real + 1 end) return 'WaitFor=' .. tostring(w.gh) .. ' flagword=' .. fw(w.gh) .. ' (want 0) WaitForRealtime=' .. tostring(w.rh) .. ' flagword=' .. fw(w.rh) .. ' (want 2 = RealtimeFlag 0x200000000 >> 32)' end)()"
    "timer-pause:(function() local h = Ext.Timer.WaitFor(60000, function() end) local p0 = Ext.Timer.IsPaused(h) local pr = Ext.Timer.Pause(h) local p1 = Ext.Timer.IsPaused(h) local rr = Ext.Timer.Resume(h) local p2 = Ext.Timer.IsPaused(h) local c1 = Ext.Timer.Cancel(h) local c2 = Ext.Timer.Cancel(h) return 'handle=' .. tostring(h) .. ' IsPaused=' .. tostring(p0) .. ' Pause=' .. tostring(pr) .. ' IsPaused=' .. tostring(p1) .. ' Resume=' .. tostring(rr) .. ' IsPaused=' .. tostring(p2) .. ' Cancel=' .. tostring(c1) .. ' Cancel-again=' .. tostring(c2) .. ' (want false true true true false true false -- the last pair is the two-sided half)' end)()"
    "timer-realtime-pause:(function() local h = Ext.Timer.WaitForRealtime(60000, function() end) local pr = Ext.Timer.Pause(h) local p = Ext.Timer.IsPaused(h) local rr = Ext.Timer.Resume(h) local c1 = Ext.Timer.Cancel(h) local c2 = Ext.Timer.Cancel(h) return 'handle=' .. tostring(h) .. ' Pause=' .. tostring(pr) .. ' IsPaused=' .. tostring(p) .. ' Resume=' .. tostring(rr) .. ' Cancel=' .. tostring(c1) .. ' Cancel-again=' .. tostring(c2) .. ' -- TimerSystem routes by the RealtimeFlag, so this is the other manager answering' end)()"
    "timer-persistent:(function() local ok, err = pcall(Ext.Timer.RegisterPersistentHandler, 'bg3seSweepPersistent', function(args, h) Ext.Utils.Print('SWEEP-TIMER persistent handler fired handle=' .. tostring(h) .. ' tag=' .. tostring(args and args.tag)) end) if not ok then return 'RegisterPersistentHandler raised: ' .. tostring(err) end local ok2, h = pcall(Ext.Timer.WaitForPersistent, 5000, 'bg3seSweepPersistent', {tag = 'sweep'}) if not ok2 then return 'WaitForPersistent raised: ' .. tostring(h) end local fw = (math.type(h) == 'integer') and tostring(h // 4294967296) or (type(h) .. '/' .. tostring(math.type(h))) return 'handle=' .. tostring(h) .. ' flagword=' .. fw .. ' (want 1 = PersistentFlag 0x100000000 >> 32); it should print a SWEEP-TIMER line within 5s of GameTime. The restore half needs a save AND a load and is not claimed here' end)()"
    "aigrid-move:(function() local h = Osi.GetHostCharacter() if h == nil then return 'no host character' end local x, y, z = Osi.GetPosition(h) if x == nil then return 'no position' end local s = _G.bg3se_sweep_aigrid local before = s and s.n or -1 local forms = {{'char,x,y,z,SpeedString,Event', function() return Osi.CharacterMoveToPosition(h, x + 3.0, y, z + 3.0, 'Walk', '') end}, {'char,x,y,z,speed-as-number,Event', function() return Osi.CharacterMoveToPosition(h, x + 3.0, y, z + 3.0, 0.0, '') end}, {'char,x,y,z,SpeedString', function() return Osi.CharacterMoveToPosition(h, x + 3.0, y, z + 3.0, 'Walk') end}} local tried = {} for _, f in ipairs(forms) do local ok, err = pcall(f[2]) if ok then return 'CharacterMoveToPosition took (' .. f[1] .. ') after ' .. #tried .. ' refusals, fired was ' .. before end tried[#tried+1] = f[1] .. ': ' .. tostring(err) end return 'no arity took -- ' .. table.concat(tried, ' | ') end)()"
    "surf-create:(function() local a = Ext.Level.CreateSurfaceAction('CreateSurface') if a == nil then return 'nil -- no level, or the row is missing' end _G.bg3se_sweep_surf = a return Ext.Types.GetObjectType(a) .. ' story=' .. tostring(a.StoryActionID) .. ' handle=' .. tostring(a.Handle) .. ' cells=' .. tostring(#a.BaseCells) end)()"
    "surf-types:(function() local names = {} for k in pairs(Ext.Enums.SurfaceActionType) do if type(k) == 'string' then names[#names+1] = k end end table.sort(names) local out = {} for _, t in ipairs(names) do local ok, a = pcall(Ext.Level.CreateSurfaceAction, t) out[#out+1] = t .. '=' .. (ok and (a ~= nil and Ext.Types.GetObjectType(a) or 'nil') or 'raised') end return table.concat(out, ' ') end)()"
    # surf-zone is the in-game witness for the ZoneActionParams layout:
    # Params is 52 bytes, not upstream's 28, so with upstream's size everything past it in
    # esv::ZoneAction is 24 bytes early.  `Validate` re-runs the property validator explicitly,
    # so this probe answers even after the Once cache has been filled; the three container sizes
    # are the members a refusal names.  Expected: Validate=true, the three counts 0 on a fresh
    # action.
    "surf-zone:(function() local a = Ext.Level.CreateSurfaceAction('Zone') if a == nil then return 'nil -- the factory declined, or the proxy was refused; check extender.log for a Validation line' end local ok, v = pcall(Ext.Types.Validate, a) local r = Ext.Types.GetObjectType(a) .. ' validate=' .. (ok and tostring(v) or ('raised: ' .. tostring(v))) .. ' Shape=' .. tostring(a.Params.Shape) .. ' RangeMinimum=' .. tostring(a.Params.RangeMinimum) .. ' RangeWanted=' .. tostring(a.Params.RangeWanted) .. ' RangeMaximum=' .. tostring(a.Params.RangeMaximum) .. ' RangeCeiling=' .. tostring(a.Params.RangeCeiling) .. ' RangeFloor=' .. tostring(a.Params.RangeFloor) .. ' ZoneParam=' .. tostring(a.Params.ZoneParam) .. ' FrontOffset=' .. tostring(a.Params.FrontOffset) .. ' Height=' .. tostring(a.Params.Height) .. ' SourcePosition=' .. tostring(a.Params.SourcePosition) .. ' Flags=' .. tostring(a.Params.Flags) .. ' GrowTimer=' .. tostring(a.GrowTimer) .. ' GrowStep=' .. tostring(a.GrowStep) .. ' cells=' .. tostring(#a.Cells) .. ' targets=' .. tostring(#a.Targets) .. ' hits=' .. tostring(#a.Hits) .. ' count=' .. tostring(a.CurrentCellCount) pcall(function() a.Finished = true end) return r end)()"
    # surf-rect is the in-game witness for the esv::RectangleSurfaceAction layout: upstream's
    # mirror is 8 bytes short before Width and another 16 before
    # Cells, which makes `Validation of property 'Cells' failed` fire on some launches
    # and not others -- Cells' size_ word lands on four bytes of the game's padding that no
    # constructor writes.  Every value below is a constructor default read off bin/bg3
    # 0x3ee73e0, so this is a differential and not a smoke test: GrowTimer 0.1 and MaxHeight
    # FLT_MAX come from the one movsd at 0x3ee741c, GrowStep 10 from 0x3ee7424, LineCheckBlock
    # 0x800008 from 0x3ee743c, and the three containers are empty on a fresh action.  A wrong
    # offset moves at least one of them.
    "surf-rect:(function() local a = Ext.Level.CreateSurfaceAction('RectangleSurface') if a == nil then return 'nil -- the factory declined, or the proxy was refused; check extender.log for a Validation line' end local ok, v = pcall(Ext.Types.Validate, a) local want = 0 local got = 0 local function chk(name, val, expect, eps) got = got + 1 local good if eps == nil then good = (val == expect) else good = (type(val) == 'number' and math.abs(val - expect) <= eps) end if good then want = want + 1 end return name .. '=' .. tostring(val) .. (good and '' or (' WANT=' .. tostring(expect))) end local parts = {Ext.Types.GetObjectType(a), 'validate=' .. (ok and tostring(v) or ('raised: ' .. tostring(v)))} parts[#parts+1] = chk('GrowTimer', a.GrowTimer, 0.1, 1e-6) parts[#parts+1] = chk('MaxHeight', a.MaxHeight, 3.4028234663852886e+38, 1e32) parts[#parts+1] = chk('GrowStep', a.GrowStep, 10) parts[#parts+1] = chk('LineCheckBlock', a.LineCheckBlock, 0x800008) parts[#parts+1] = chk('CurrentGrowTimer', a.CurrentGrowTimer, 0.0, 1e-9) parts[#parts+1] = chk('Width', a.Width, 0.0, 1e-9) parts[#parts+1] = chk('Length', a.Length, 0.0, 1e-9) parts[#parts+1] = chk('cells', #a.Cells, 0) parts[#parts+1] = chk('characters', #a.Characters, 0) parts[#parts+1] = chk('items', #a.Items, 0) parts[#parts+1] = chk('count', a.CurrentCellCount, 0) parts[#parts+1] = 'Direction=' .. tostring(a.Direction) .. ' Target=' .. tostring(a.Target) .. ' DeathType=' .. tostring(a.DeathType) local r = table.concat(parts, ' ') .. ' -- ' .. want .. '/' .. got .. ' constructor defaults agree' return r end)()"
    "surf-transform-init:(function() local a = Ext.Level.CreateSurfaceAction('TransformSurface') if a == nil then return 'nil -- no level, or the row is missing' end a.SurfaceTransformAction = 'Ignite' local before = a.GrowCellPerSecond local fbefore = a.Finished local ok, err = pcall(Ext.Level.ExecuteSurfaceAction, a) if not ok then return 'ExecuteSurfaceAction raised: ' .. tostring(err) end local after, fafter = a.GrowCellPerSecond, a.Finished a.Finished = true return 'Ignite GrowCellPerSecond ' .. tostring(before) .. ' -> ' .. tostring(after) .. ' (Init sets 400.0 for Ignite; the constructor leaves 1.0, and that before reading is taken in this same chunk) finished ' .. tostring(fbefore) .. ' -> ' .. tostring(fafter) .. ', retired=' .. tostring(a.Finished) end)()"
    "surf-transform-table:(function() local want = {[1]=400,[2]=400,[5]=400,[6]=400,[3]=800,[4]=800,[9]=800,[10]=800,[11]=800,[7]=200,[8]=200,[12]=200,[13]=200} local vals = {} for k, v in pairs(Ext.Enums.SurfaceTransformActionType) do if type(k) == 'number' then vals[#vals+1] = {k, tostring(v)} end end table.sort(vals, function(x, y) return x[1] < y[1] end) local out, agree, n = {}, 0, 0 for _, kv in ipairs(vals) do local v, name = kv[1], kv[2] local a = Ext.Level.CreateSurfaceAction('TransformSurface') if a == nil then out[#out+1] = name .. '=no-action' else a.SurfaceTransformAction = name local before = a.GrowCellPerSecond local ok, err = pcall(Ext.Level.ExecuteSurfaceAction, a) if not ok then out[#out+1] = name .. '=raised: ' .. tostring(err) else local after, fin = a.GrowCellPerSecond, a.Finished a.Finished = true n = n + 1 local w = want[v] local good if w ~= nil then good = (math.abs(after - w) < 0.5 and after ~= before) else good = (after == before and fin == true) end if good then agree = agree + 1 end out[#out+1] = name .. '/' .. v .. '=' .. tostring(before) .. '->' .. tostring(after) .. (fin and '+fin' or '') .. (good and '' or ' WANT=' .. tostring(w)) end end end return agree .. '/' .. n .. ' agree with the offline table (400 for 1,2,5,6; 800 for 3,4,9,10,11; 200 for 7,8,12,13; untouched+Finished for 0 and out of range): ' .. table.concat(out, ' ') end)()"
    "aigrid-counts:(function() local s = _G.bg3se_sweep_aigrid if s == nil then return 'not subscribed' end return 'fired=' .. s.n .. ' callbacks=' .. s.done .. ' first=' .. tostring(s.first) end)()"
    "timer-counts:(function() local w = _G.bg3se_sweep_waits if w == nil then return 'timer-waitfor did not run' end return 'WaitFor fired=' .. w.game .. ' WaitForRealtime fired=' .. w.real .. ' (both want >= 1; both managers ride the same time.Time on this revision, so both fire or neither does -- neither firing is the server tick never reaching State::OnUpdate)' end)()"
    "timer-tick-counts:(function() local s = _G.bg3se_sweep_tick if s == nil then return 'not subscribed' end local span = (s.t0 ~= nil and s.t1 ~= nil) and (s.t1 - s.t0) or -1 return 'ticks=' .. s.n .. ' increased=' .. s.mono .. '/' .. math.max(s.n - 1, 0) .. ' span=' .. span .. ' scale1=' .. s.scaled .. '/' .. s.n .. ' first=' .. tostring(s.first) .. ' last=' .. tostring(s.last) .. ' (Ticks is the int32 frame counter at +0x10, so increased must equal the denominator and span must be >= ticks; UnscaledDeltaTime is the float at +0xc and equals DeltaTime while the Global+0x40 time scale is 1.0, which is what scale1 counts. DeltaTime still wants the 1/30..1/300s band; all three come out of the GameTime* the hook is handed)' end)()"
    "query-route:(function() local names = {'Uuid', 'Transform', 'DisplayName', 'ServerCharacter'} local q, scan = {}, {} for _, n in ipairs(names) do local ok, r = pcall(Ext.Entity.GetAllEntitiesWithComponent, n) q[n] = (ok and r ~= nil) and #r or -1 scan[n] = 0 end local all = Ext.Entity.GetAllEntities() for i = 1, #all do local e = all[i] for _, n in ipairs(names) do local okc, c = pcall(function() return e[n] end) if okc and c ~= nil then scan[n] = scan[n] + 1 end end end local out, agree = {}, 0 for _, n in ipairs(names) do if q[n] == scan[n] then agree = agree + 1 end out[#out+1] = n .. ' query=' .. q[n] .. ' scan=' .. scan[n] .. (q[n] == scan[n] and '' or ' MISMATCH') end return agree .. '/' .. #names .. ' agree, entities=' .. #all .. ' -- ' .. table.concat(out, ' ') .. ' (the query route is GetAllEntitiesWithComponent, which walks the game single-component query bound in UpdateQueryCache; the scan route is GetAllEntities filtered by the component itself. Equal on every name means UpdateQueryCache binds the right branch; a query count far below the scan count (query=7 against scan=2762) means it does not)' end)()"
    "combat-system:(function() local s = Ext.System.ServerCombat if s == nil then return 'nil -- the system map refused it; look for a Validation line in extender.log just above' end local ok, v = pcall(Ext.Types.Validate, s) local function b(x) return tostring(x) .. '/' .. type(x) end local names = {'EndCombat','SkipCombatStartDelay','DestroyCombats','ResumeCombat','PauseCombat','ResetCombatTimer','JoinCombat','JoinNarrativeCombat','CombatGroupUpdates','ResetTurn','LeaveCombat','CombatRequests','NarrativeCombatRequests','Flee','ActivatedPeers_PeerId','field_3E0_PeerId','field_410_PeerId'} local out, bad = {}, 0 for _, n in ipairs(names) do local okc, c = pcall(function() return s[n] end) if not okc or c == nil then bad = bad + 1 out[#out+1] = n .. '=REFUSED' else local okl, l = pcall(function() return #c end) out[#out+1] = n .. '=' .. (okl and tostring(l) or 'no-len') end end return Ext.Types.GetObjectType(s) .. ' validate=' .. (ok and tostring(v) or ('raised: ' .. tostring(v))) .. ' field_328=' .. b(s.field_328) .. ' field_329=' .. b(s.field_329) .. ' refused=' .. bad .. ' -- ' .. table.concat(out, ' ') .. ' (the two flags must be booleans: bin/bg3 0x32739f1 tests field_328 as a byte at +0x320, which is where it lands only without upstream field_18; every container must answer a length)' end)()"
    "status-flags5:(function() local host = Osi.GetHostCharacter() local e = host ~= nil and Ext.Entity.Get(host) or nil local sc = e ~= nil and e.ServerCharacter or nil if sc == nil then return 'no ServerCharacter on the host character' end local sm = sc.StatusManager if sm == nil then return 'no StatusManager on the host ServerCharacter' end local sts = sm.Statuses local n = (sts ~= nil) and #sts or -1 if n < 1 then return n .. ' statuses on the host -- proc-status applies BLESS earlier in this sweep, so 0 means that call did not land and the probe says nothing either way' end local known, bits = {}, {} local oke, en = pcall(function() return Ext.Enums.ServerStatusFlags5 end) if oke and en ~= nil then for k, v in pairs(en) do if type(k) == 'string' then known[k] = true end if type(v) == 'string' then known[v] = true end if type(k) == 'number' then bits[k] = true end if type(v) == 'number' then bits[v] = true end end end local nbits = 0 for _ in pairs(bits) do nbits = nbits + 1 end local function desc(x) if type(x) == 'number' then if nbits == 0 then return 'number(' .. tostring(x) .. ')[bits-unchecked: the enum table has no numeric entries]', 0 end local unknown, b, v = 0, 1, x while v > 0 do if v % 2 == 1 and not bits[b] then unknown = unknown + 1 end v = math.floor(v / 2) b = b * 2 end return 'number(' .. tostring(x) .. ')', unknown end if type(x) == 'userdata' or type(x) == 'string' then local t = tostring(x) local inner = string.match(t, '%((.*)%)$') if inner == nil then return type(x) .. '(' .. t .. ')[bits-unchecked: not name(flags) shaped]', 0 end local unknown = 0 for nm in string.gmatch(inner, '[^,]+') do nm = string.match(nm, '^%s*(.-)%s*$') if nm ~= '' and not known[nm] then unknown = unknown + 1 end end return t, unknown end if type(x) ~= 'table' then return type(x) .. '(' .. tostring(x) .. ')[bits-unchecked: unexpected representation]', 0 end local names, unknown = {}, 0 for k, v in pairs(x) do local nm = (type(k) == 'string') and k or tostring(v) names[#names+1] = nm if not known[nm] then unknown = unknown + 1 end end table.sort(names) return '{' .. table.concat(names, ',') .. '}', unknown end local read, bad, unknowns, set, out = 0, 0, 0, 0, {} for i = 1, n do local st = sts[i] local okid, id = pcall(function() return tostring(st.StatusId) end) id = okid and id or '?' local okv, v = pcall(Ext.Types.Validate, st) local ok5, f5 = pcall(function() return st.Flags5 end) local ok4, f4 = pcall(function() return st.Flags4 end) if (not okv) or v ~= true or (not ok5) or f5 == nil or (not ok4) then bad = bad + 1 out[#out+1] = id .. '=REFUSED(validate=' .. tostring(okv and v or 'raised') .. ' Flags5=' .. tostring(ok5 and f5 ~= nil) .. ' Flags4=' .. tostring(ok4) .. ')' else read = read + 1 local d5, u5 = desc(f5) local d4 = desc(f4) unknowns = unknowns + u5 if d5 ~= '{}' and d5 ~= 'number(0)' and string.sub(d5, -2) ~= '()' then set = set + 1 end out[#out+1] = id .. ' Flags4=' .. d4 .. ' Flags5=' .. d5 .. (u5 > 0 and (' UNKNOWN=' .. u5) or '') end end return read .. '/' .. n .. ' host statuses read, ' .. bad .. ' refused, ' .. unknowns .. ' unknown Flags5 bits, ' .. set .. ' with a bit set -- ' .. table.concat(out, ' ') .. ' (Flags5 is the byte at 0x182 that upstream commented out as \'gone in patch3\'; bin/bg3 0x2bb98b0 writes bit 0 of it from a bool at 0x2bba0a3 and reads it back at 0x2bba041, and the only bit the bitmask declares is HasTriedEntering. bad > 0 or unknowns > 0 is the HasTriedEntering member regressing -- a refusal means the new member broke the esv::Status proxy, an unknown bit means it is not on the byte it was measured on. The count with a bit set is an observation, not a check: no setter for it is among the 732 matched esv::Status* functions, so this probe cannot say what the value ought to be)' end)()"
    "wander-anchor:(function() local ok, es = pcall(Ext.Entity.GetAllEntitiesWithComponent, 'ServerCharacter') if not ok or es == nil then return 'GetAllEntitiesWithComponent(ServerCharacter) refused -- the probe says nothing either way' end local chars, ctls, tasks, wander, bad, kinds, out = 0, 0, 0, 0, 0, {}, {} local function num(v) return (type(v) == 'number') and string.format('%.2f', v) or (type(v) .. '(' .. tostring(v) .. ')') end local function v3(v) local o, r = pcall(function() return string.format('(%.1f,%.1f,%.1f)', v[1], v[2], v[3]) end) if o and r ~= nil then return r end return type(v) .. '(' .. tostring(v) .. ')' end for i = 1, #es do local e = es[i] local okc, sc = pcall(function() return e.ServerCharacter end) if okc and sc ~= nil then chars = chars + 1 local okl, ctl = pcall(function() return sc.OsirisController end) if okl and ctl ~= nil then ctls = ctls + 1 local okt, ts = pcall(function() return ctl.Tasks end) if okt and ts ~= nil then for j = 1, #ts do local okj, t = pcall(function() return ts[j] end) if not okj or t == nil then bad = bad + 1 else tasks = tasks + 1 local okty, ty = pcall(function() return tostring(t.Type) end) ty = okty and ty or '?' kinds[ty] = (kinds[ty] or 0) + 1 if string.find(ty, 'Wander', 1, true) ~= nil then wander = wander + 1 local g = {} local function rd(n) local o, v = pcall(function() return t[n] end) if not o or v == nil then bad = bad + 1 return nil end g[n] = v return v end rd('Range') rd('Trigger') rd('Anchor') rd('Duration') rd('Start') rd('BehaviourTransactionId') local pos = nil local okp, tc = pcall(function() return e.Transform.Transform.Translate end) if okp then pos = tc end local d = nil if pos ~= nil and g.Start ~= nil then local okd, dd = pcall(function() return math.sqrt((g.Start[1]-pos[1])^2 + (g.Start[2]-pos[2])^2 + (g.Start[3]-pos[3])^2) end) if okd then d = dd end end if #out < 4 then out[#out+1] = 'Range=' .. num(g.Range) .. ' Trigger=' .. tostring(g.Trigger) .. ' Anchor=' .. v3(g.Anchor) .. ' Duration=' .. num(g.Duration) .. ' Start=' .. v3(g.Start) .. ' charPos=' .. v3(pos) .. ' dist=' .. (d ~= nil and string.format('%.1f', d) or 'n/a') .. ' Btid=' .. tostring(g.BehaviourTransactionId) end end end end end end end end local ks, nk = {}, 0 for k, v in pairs(kinds) do ks[#ks+1] = k .. '=' .. v nk = nk + 1 end table.sort(ks) local head = wander .. ' Wander tasks in ' .. tasks .. ' queued tasks over ' .. ctls .. '/' .. chars .. ' server characters with an OsirisController, ' .. bad .. ' reads refused; kinds: ' .. (nk > 0 and table.concat(ks, ',') or 'none') if wander == 0 then return head .. ' -- NO WANDER TASK, as expected: no Osiris call queues one and an Anubis Wander lives in AnubisRuntime.Operation, not here. Neither a pass nor a fail; the route is what this proves.' end return head .. ' -- ' .. table.concat(out, ' | ') .. ' (esv::OsirisWanderTask::Anchor was upstream std::variant<EntityHandle, glm::vec3>, 24 bytes under libc++ where the game packs 16 with the tag at +0xc; that put Duration and Start 8 bytes too far. The mirror holds upstream EntityOrVec3Variant and asserts Duration 0x50 / Start 0x54 from bin/bg3 0x7124b30. The witness is Start: it is where the wander began, so dist should be within a few metres of the character and Duration a small finite number. Under the old layout Duration would read the games Start.y and Start would read (Start.z, BehaviourTransactionId as a float, junk) -- a huge or denormal coordinate and a dist in the thousands.)' end)()"
)
# The client half, run over a second console connection ('client' instead of 'server'),
# because rendering objects only exist there.
#
# mat-* are the witness for ls::AppliedMaterial::TryOverrideTexture2DParameter (0x2942580)
# and its null guard. The route is the one Render.h declares --
# Visual.Visual.ObjectDescs[i].Renderable.AppliedMaterials[j]; 'Visual::Materials' is not a
# property on this build's mirror.
# mat-route scans every entity with a Visual component -- `Visual.Visual` is nil on most of
# them -- and keeps the first AppliedMaterial **that has a
# named Texture2D parameter**; mat-settex-* then call SetTexture2D on it.
# Keeping the first material of any kind would make the whole Texture2D differential
# luck-dependent: a small client world (seven Visual entities is a real reading) can put a
# material with two unnamed Texture2DParameters first, mat-param then reads `nil of {,}`, and
# mat-settex-real/-bogus degrade to "no material, texture or parameter" -- three lines that
# read like a refusal rather than like a probe that never ran. Searching for a named parameter
# -- the same search mat-vtroute does for a non-empty VirtualTextureParams -- finds one
# (`Glowmap`) even on such a world. mat-route sets _G.bg3se_sweep_param itself; mat-param
# still prints the full name list and only fills the global if the route left it empty. A run
# where no material has a named parameter says so and names the count it searched, and keeps a
# fallback material so mat-param still has something to describe. There is no Osi on the
# client console, so the route starts from GetAllEntitiesWithComponent rather than from the
# host character. On a build with the symbol unrowed the expected reading is a *refusal naming
# the symbol*, not a crash and not true: that is the whole point of the guard. With the row the
# pair is **differential**, which a single bogus-GUID call could never be:
# a texture the resource manager does not have makes the function return false whether it ran or
# refused by name. mat-settex-pick takes a real 2D Texture resource out of Ext.Resource.GetAll
# and mat-param takes a parameter name the material actually
# has -- 'BaseMap' is *not* one on the material this route reaches, and a name the material
# lacks reads false/false even with the row in place: the
# function's immediate path scans AppliedMaterial::Texture2DParams for the name and returns 0
# when it misses, exactly as it does for a missing resource. The expected reading is
# **true / false / false**: a real texture on a parameter that exists, the same parameter with
# a GUID nobody has, and a real texture on a parameter that does not exist. Only the first can
# distinguish a working row from the guard, since the guard always returns false (and logs).
# mat-setvt-* are the same differential for ls__AppliedMaterial__LoadVirtualTexture 0x24f9c20,
# and they need their *own* material: SetVirtualTexture scans
# AppliedMaterial::VirtualTextureParams, which is a different array from Texture2DParams and is
# empty on most materials -- so mat-vtroute walks every AppliedMaterial on every Visual entity and
# keeps the first one with a non-empty VirtualTextureParams, rather than reusing mat-route's pick.
# mat-setvt-pick takes a real resource out of Ext.Resource.GetAll('VirtualTexture') -- bank type 23,
# which is the `mov esi, 0x17` the Linux function passes on its non-default-bank path. Expected
# reading is again **true / false / false**: a real virtual texture on a parameter the material has,
# the same parameter with a GUID nobody has (upstream logs "Failed to load virtual texture"), and a
# real one on a parameter that does not exist ("Material has no VT parameter named"). Only the first
# separates the row from the guard's own always-false refusal.
# cshape-fields reads the ls::Shape mirror on the same route mat-vtroute walks: a Renderable that
# answers TriangulationFailed is a Shape (StaticRTTI 0x103), and the probe requires that byte to be
# a boolean, Lifetime a finite number and UseShapeRender a boolean, over every Shape it finds. It
# exists because upstream's `uint32_t Preset` is not in the native build:
# carrying it across puts TriangulationFailed at 0xf4 where the game has it at 0xee, and the
# validator refuses the byte and disables proxying of the whole class. The route reaches a
# Shape at all only when the client Visual query covers the pool (cvisual-* below). A refusal,
# a nil or a non-boolean here is the ls::Shape layout regressing; `no ls::Shape among the
# renderables` means the save has none and the probe says nothing either way.
# proto-* cover eoc__SpellPrototype__Init 0x5e35980 and eoc__StatusPrototype__Init 0x27e4d90.
# Neither is a hook: upstream calls them through SYM_OFF from
# SpellPrototypeManager::SyncStat / StatusPrototypeManager::SyncStat, each behind its own null guard.
# proto-pick names the two entries the rest use (Projectile_MagicMissile and BLESS, falling back to
# the first of each list, as stats-one does). proto-*-cached read the prototype the *game* filled at
# module load, through the manager HashMap mirrors -- they are the live check on the
# Prototype.h asserts (SpellFlags +0x10, Level +0x3C, StatusPrototype::Flags +0xB4,
# AbsorbSurfaceTypes +0xB8, HitAnimationType +0x108): the expected reading is a flag set
# that is semantically right for Magic Missile, not merely non-garbage.
# proto-*-sync are *differential*, which is the whole point: reading the same values back after a
# Sync is equally what "Ext.Stats.Sync did nothing" looks like, and that is exactly what a
# ModifierList::Name four bytes out produces (SyncWithPrototypeManager's four-way branch on the
# modifier list's name matches nothing). Each
# probe breaks the prototype first and requires the Init to put it back: the spell one zeroes
# SpellSchool and Level, which eoc__SpellPrototype__Init 0x5e35980 refills from the stat entry, and
# the status one writes a wrong HitAnimationType, which eoc__StatusPrototype__Init 0x27e4d90
# overwrites unconditionally (Prototype.h's +0x3C, +0x108 asserts are the offsets). Both report
# `Init ran` or `Init DID NOT run` and print the pair, so a reading is never ambiguous. They are
# the in-game witness for the two rows; if one reads DID NOT run, start at the
# ModifierList name, not at the addresses.
# proto-interrupt-* cover eoc__InterruptPrototype__Init 0x2fc33d0, the third of the family.
# Same shape as the spell pair: -cached reads what the game filled at module
# load through the InterruptPrototypeManager mirror (the live check on Prototype.h's sizeof 0x1f0,
# Name +0, Description +8, InterruptContext +0x90 and the manager's IsLoaded +0x48), and -sync is
# differential -- it writes a wrong InterruptContext, which the Init zeroes and refills
# unconditionally at bin/bg3 0x2fc3426, and requires it back. #Costs is printed before and after
# because upstream's SyncStat clears Costs before calling the Init: if the Init does not refill
# them, a Sync empties them, and the pair is what would show it.
# proto-passive-* are the witness for the *other* side of that family -- eoc__PassivePrototype__Init
# is inlined into eoc::Passives::Init 0x2fc1b00 and has no address to call, so it is in
# sAbsentOnLinux with no substitute. -sync-absent is therefore a probe that expects a *refusal*: it
# dirties PriorityOrder, Syncs, and requires the dirty value to survive, because
# PassivePrototypeManager::SyncStat's `if (!sync)` guard returns before it touches anything. It
# restores the field itself, since no Init will. Reading `UNEXPECTED: something ran` would mean the
# name found a row after all -- go back to targets-hand.tsv before believing it. The direct witness
# of the guard is the OsiError line in extender.log; this probe is the one that says the prototype
# was left alone. proto-status-live applies the status to the host on the
# same footing.
# surf-* are the witness for esv__SurfaceActionFactory__DoCreateAction 0x38a5fd0, which
# takes five arguments here where upstream's proc declares three.
# surf-create is the primary reading: without the row the member is null and
# SurfaceManager::CreateAction returns nullptr, so Lua gets nil -- with it, a SurfaceAction comes
# back with StoryActionID 0 (the third argument, the value the game's own call site passes) and a
# Handle the factory allocated, which it only does when the fifth argument is NullHandle; through
# upstream's prototype that register held whatever the caller left. surf-types walks the jump table
# at 0x1bd4efc, so each type must name its own concrete class.
# surf-types walks **Ext.Enums.SurfaceActionType itself** rather than a hand-written list: a
# list naming a label this build lacks ('ExtinguishFire', say) reads `X=raised` with the error
# "not a valid 'SurfaceActionType' enum label", which is the probe being wrong, not the row.
# The eleven labels and their values,
# read off the live enum: None=0 CreateSurface=1 CreatePuddle=2 RemoveSurface=3 Zone=4
# TransformSurface=5 ChangeSurfaceOnPath=6 RectangleSurface=7 PolygonSurface=8 ForceCreateSurface=9
# CapsuleSurface=10. The expected reading is **ten concrete classes, with only None nil**:
#   None=0  -- the dispatch is `dec esi; cmp esi, 9; ja <return 0>` at bg3+0x38a60e5, so the table
#             covers values 1..10 and 0 falls out of range. There is nothing to construct.
#   Zone=4  -- index 3 of the table, target bg3+0x38a6164, structurally identical to every other
#             case (NullHandle check, vtable call at +8, then the per-type allocator, here
#             bg3+0x3ee7240) and joining the common tail at bg3+0x38a612a, whose `test rax, rax`
#             has a null arm. That arm exists -- 0x3ee7240 opens `test dword [rsi+4], 0x3fffff;
#             je <return 0>` -- but `isnil=true` from Lua does not
#             distinguish "the factory returned null" from "the proxy was refused": ObjectProxy::
#             MakeRef pushes nil when ValidateIfNecessary fails, and a refusal names a
#             *non-null* esv::ZoneAction in extender.log between surf-create and surf-types.
# The refusal to watch for is `Targets`: esv::ZoneActionParams is 52 bytes in the game where
# upstream's mirror has 28, so with upstream's size every member past it reads early (Surface.h).
# The expected reading is `Zone=esv::ZoneAction` and no Validation line. `Zone=nil` *with* one
# is the ZoneActionParams size regressing; `Zone=nil` *without* one means the null arm was
# taken and that is a new fact about the factory's guard.
# The fourth argument of DoCreateAction (the ClassDescriptions bank) is NOT
# witnessed in game and cannot be: its only effect is the store at +0x50 that upstream's
# Surface.inl performs by hand anyway, so the disassembly is its whole evidence.
#
# surf-transform-init and surf-transform-table are the witness for
# esv__TransformSurfaceAction__Init bg3+0x2682850. Neither is a survival check: with the row
# in, the null guard's refusal cannot be produced on this build, so "it survived" says nothing
# -- a null guard produces exactly the same silence (as with mat-settex and aigrid's fired=0).
#
# The positive only the working path can produce is **GrowCellPerSecond**, which nothing but Init
# writes: the function's whole tail is three float immediates into [this+0x84], chosen by
# `lea ecx, [type-1] / cmp ecx, 2` and then three bit tests, `bt 0xe18`, `bt 0x3180`, `bt 0x60`.
# Read off the Linux body, that is 400.0 for types 1,2,5,6 (Ignite, Douse, Freeze, Melt), 800.0 for
# 3,4,9,10,11 (Electrify, Deelectrify, DestroyWater, Clear, Daylight), 200.0 for 7,8,12,13
# (Vaporize, Condense, TurnHellfire, UnturnHellfire), and for 0 (None) or anything above 13 the
# rate is left alone and `byte [this+0x88]` (Finished) is set to 1 instead. surf-transform-table
# walks the whole enum against that prediction and prints its own agreement count; a wrong address
# cannot reproduce a fourteen-row table it was never told. The expected reading is **15/15**,
# with a before reading of 1.0 on every row -- the constructor's value, not zero.
#
# Both take the **before** reading in the same chunk as the after (1.0 -> 400.0, not "it is 400.0
# now"), and both set `a.Finished = true` the instant the reading is taken. That is deliberate and
# it is what makes activating a real surface action safe to do from a probe: the game's
# IsFinished is `return [this+0x88]` and Update's first test is `cmp byte [this+0x88], 0 / jne
# <ret>`, so a finished action is retired by SurfaceManager::Update without its cell searcher ever
# running over the grid. Read off the macOS oracle (Update 0x105ad1ed0, IsFinished 0x105ad2b60).
# stats-* are the witness for RPGStats__ParseDataBuffers 0x27ddb00.
# Upstream fires Ext.Events.StatsStructureLoaded from that pre-hook and nowhere else, and
# it fires during module load -- before any console exists -- so the subscription lives in the
# overlay's BootstrapClient.lua and the primary reading is the SWEEP-STATS line it prints into
# extender.log, not a probe. stats-structure-count reads the counter back if the Load-context
# Lua state survived into the game (it may not: 'no counter' there is not a failure of the hook,
# the SWEEP-STATS line is). stats-weapon-count and stats-one are the cheap check that the parse
# the hook sits in front of actually produced stat objects, and that ModId/OriginalModId --
# StatLoadOrderHelper's half of OnParseDataBuffers -- are filled rather than nil.
# blend-* are the witness for the *absence* of ls__Visual__UpdateBlendshapeWeightsFromSkeleton:
# LTO inlined it into ls::Visual::NotifySkeletonUpdated(bool) 0x24a6af0, so
# there is no function to post-hook and an override recorded from Lua would never be applied.
# The pair is a differential, not a survival check. blend-route finds a Renderable whose
# BlendShape is non-null -- which is upstream's *own* first test in both calls -- and then
# blend-refused calls both on it: ClearBlendShapeWeights passes that test and returns **true**,
# while SetBlendShapeWeight returns **false**. Nothing but the PORT (linux) guard in
# VisualHelpers::SetWeight can produce that combination, so `clear=true set=false` is the
# positive reading; `clear=true set=true` would mean the guard did not fire and the absent-by-
# design entry is wrong. If the client world holds no Renderable with a BlendShape the probe
# falls back to any Renderable and says so -- then only the ERR line in extender.log witnesses
# it, because upstream's own BlendShape test would refuse too.
# blend-fields is the same route again, for the BlendShapeWeights mirror rather than for the
# absent function: it reads #Weights on every Renderable whose BlendShape is non-null and
# requires the count to be a non-negative integer below 4096 (the largest morph target count in
# the shipped meshes is in the low hundreds; the bound is a sanity rail, not a measurement).  It
# exists because upstream's BlendShapeObjectDataVK has one buffer where the native build has
# three, which puts Weights at 0x40 instead of 0x50 and hands the validator a BufferBindingInfo's
# padding as size_ -- so the class is refused on some launches and not others.  `refused` on a
# renderable that has a BlendShape, or a count outside the rail, is that
# layout regressing; `0/N` means the save's client world holds no blend-shaped renderable and the
# probe says nothing either way.
# gcp-fields is the same question for ls::GlobalConfigParameter, reached through the root
# template bank rather than through a Visual: it walks every root template, and on the ones that
# carry ConstellationConfigGlobalParameters it reads Name and Value off every parameter.  The
# defect it witnesses is not a member of that struct but the shape of the type: libc++ gives a
# std::variant nested inside another 8 bytes of leading padding (its two empty bases would
# otherwise share an address with the inner variant's, which Itanium forbids), where Larian's
# ls::Variant has no bases and no hole, so with stock libc++ both of GlobalConfigParameter's
# discriminants sit 8 bytes high and the validator refuses Value, then ItemTemplate with it, then
# all proxying of ItemTemplate.  Which instance the validator is asked to proxy first decides
# whether the refusal is recorded at all (template-one, which only reads v.Name off the first
# root template, can ERR on one launch and not the next), so this
# probe reads every parameter of every template rather than one.  A refusal, a non-string Name or
# an unreadable Value is the libc++ sfinae-tag overlay (ext/toolchain/) not taking; `0/N` means
# this save's template bank has no constellation parameters and the probe says nothing either way.
# sexpr-params is the same question for eoc::StatsExpressionInternal, reached through the boost
# components that carry one (RollBonus, Skill, WeaponDamage, CharacterWeaponDamage, DamageBonus,
# WeaponAttackRollBonus, CharacterUnarmedDamage all hold a StatsExpressionInternal Amount).  Its
# Params is an Array of a std::variant whose alternative 1 is *itself* a std::variant, which is
# the same defect class gcp-fields witnesses: stock libc++ gives the outer union 8 bytes of
# leading padding, so the element is 40 bytes with the discriminants at +0x20 and +0x18 where
# bin/bg3 0x352bc90 -- the copy constructor of ls::DynamicArray<Param> -- sizes the buffer n<<5,
# strides the loop by 0x20 and reads them at +0x18 and +0x10.  With the stride
# wrong as well as the members, every param past the first reads out of a neighbouring element,
# so this probe walks every param of every expression it can reach rather than the first.
# ctick-* are the client half of the tick (client Ext.Timer / Ext.OnNextTick / Ext.Events.Tick).
# The server timer-* block above
# is the same questions asked of esv__GameStateMachine__Update; here the hook is a different one
# and that is the whole point: ecl__GameStateMachine__Update is in sAbsentOnLinux (the game
# inlines it into App::Update), so the client's State::OnUpdate is reached from
# ecl::ScriptExtender::OnECSTick, called out of the ecs__EntityWorld__Update post-hook for the
# *client* world only (ScriptExtender.cpp:350, ScriptExtenderClient.cpp:332).  Everything the
# client tick owes rides that one call -- timers_.Update, the Tick event, network_.Update (the
# Ext.Net local queue), RunPendingTasks, IMGUI().Update and the variable flush -- so a probe that
# only proves Ext.Timer exists in the client state proves nothing about any of them: the module is
# DECLARE_MODULE(Timer, Both) and answers in a state that never ticks.  What witnesses the tick is
# a count that moves across the sweep's own span, which is why these four register at the top of
# the client block and the ctick-*-counts probes read them back at the bottom, ~20 s of probe
# intervals later (the GameTime probe's shape).
#   ctick-clocks/ctick-advance: dGameTime is the client ExtensionStateBase::time_, assigned in
#   OnUpdate from the GameTime the ecs hook was handed.  dMonotonic and dMicrosec come from the
#   port's QueryPerformanceCounter shim and advance whether or not the game ticks, so they are the
#   control: dMonotonic large with dGameTime ~0 is precisely "the client tick is not reaching Lua".
#   ctick-subscribe/ctick-tick-counts: the same reading as timer-tick-subscribe, on the other
#   hook.  Ticks must increase on every tick and the span must be >= the count; DeltaTime in the
#   1/30..1/300 s band says the GameTime& the post-hook forwards is a real one.  This is also the
#   client-side check on the argument: OnECSTick takes `GameTime const&` from the hook's second
#   parameter, and a wrong register there would show as a wild Ticks or a zero DeltaTime.
#   ctick-nexttick/ctick-nexttick-count: Ext.OnNextTick is Ext.Events.Tick:Subscribe(f, {Once =
#   true}) (BuiltinLibrary.lua:73) and nothing else, so two registrations must fire exactly once
#   each -- 0 is the tick never firing, >1 is Once not honoured -- and the tick index each fired at
#   says it was the *next* tick and not a later one.
#   ctick-waitfor/ctick-counts: the client timer managers.  Same caveat as the server's: both ride
#   time.Time on this revision, so both fire or neither does, and the flagword is the routing
#   witness (0 for WaitFor, 2 for WaitForRealtime).
# cvisual-* checks that the client's single-component query covers the Visual pool: in
# character creation Ext.Entity.GetAllEntitiesWithComponent('Visual') can return three entities,
# two of which answer nil to .Visual, while every ClientCharacter entity holds a VisualComponent
# with a non-null Visual and none is returned.  The query path explains how that
# is possible: GetAllEntitiesWithComponent (Entity.inl:83) uses the game's own single-component
# query when meta.SingleComponentQuery != UndefinedQuery and walks only *that* query's
# EntityStorages, falling back to every storage class otherwise.  GetAllEntities() (Entity.inl:131)
# always walks every storage class, so it is the independent route, and ClientCharacter is the
# second one.  inVisualQuery < withVisual is that reading reproduced; equal on both
# routes says the query covers the pool in this context.
# onchange-subscribe/onchange-counts are the in-game witness for the FieldTracker vtable:
# the game's ecs::sync::_private::FieldTracker virtuals are
# Track / Dispatch(entity) / Dispatch(), not upstream's Add / FireEvents / Remove; with
# upstream's order the per-frame flush (ComponentDeserializer<T>::DispatchChanges, bin/bg3
# 0x5dded00) runs Remove and no client Ext.Entity.OnChange handler is ever called (the
# subscription is accepted and nothing fires).  The probe is split in two because the
# sweep sends every probes[] entry in *server* context and only then switches to client for the
# cprobes[], so subscribe -> trigger -> read cannot happen in one pass.  The shape is **two sweep
# runs in one launch**: run 1's onchange-subscribe parks the subscriptions and the counters in
# _G.bg3se_sweep_onchange (the ctick-* probes are the pattern for _G state carried between
# probes) and run 1's onchange-counts reads ~20 s later; run 2 finds the state already there,
# says so, and its onchange-counts reads after run 2's whole server pass -- which begins with
# proc-status applying BLESS to the host for 30 s and therefore changes eoc::status::*
# replication on the host entity, plus the expiry of the same status while that pass runs.
# The nine subscribed types are the ones the TypeId table lists under
# ecs::sync::ReplicatedTypeContext that a status application or an idle party can touch
# (typeids/a187af94*.tsv: eoc::HealthComponent, eoc::status::{ContainerComponent,
# LifetimeComponent, IDComponent, CauseComponent}, eoc::ActionResourcesComponent,
# eoc::TurnBasedComponent, eoc::combat::ParticipantComponent, eoc::exp::ExperienceComponent).
# **The probe carries its own calibration, and the control is what makes a 0 readable**:
# Ext.Entity.OnCreate/OnDestroy('StatusID') ride the component-event hooks on the client entity
# world, not the replication trackers, so they fire iff a status component actually appeared on
# the client in the window.  So: events > 0 naming an entity and a field mask is the witness that
# the fix works; events 0 with control created > 0 is SUSPECT (a component *create* goes through
# DeserializeComponent, which calls no tracker, so only a window that also held a change to an
# existing component can fail -- run 1 reads 0/SUSPECT by construction and run 2 is the reading);
# events 0 with control created 0 is a still save in which nothing replicated to the client at all
# and the probe says nothing either way.  Expected: run 1 reads events=0 SUSPECT with
# created > 0, run 2 reads events in the hundreds to thousands, nearly all StatusLifetime on the
# host entity with fields=0x1 -- BLESS's lifetime ticking once a frame for its 30 s, so run 2's
# count scales with the frame rate and is not a fixed figure.  subs < 9
# names the refused types -- a raise is "No replication events are available for components of
# type X" (no replication index, i.e. the TypeId row did not bind), a nil return is
# ClientEntityReplicationEventHooks::AddComponentType refusing, and extender.log carries the
# "has no deserializer" ERR with the mask dump next to it.
cprobes=(
    "onchange-subscribe:(function() local s = _G.bg3se_sweep_onchange if s ~= nil then return 'already subscribed in an earlier run of this sweep: ' .. s.subs .. '/' .. #s.types .. ' types, ' .. s.n .. ' change events and ' .. s.created .. ' control creates so far -- this run\'s onchange-counts reads after this run\'s server pass, which is the trigger' end s = {n = 0, subs = 0, per = {}, ids = {}, refused = {}, first = nil, last = nil, created = 0, destroyed = 0, types = {'Health', 'StatusContainer', 'StatusLifetime', 'StatusID', 'StatusCause', 'ActionResources', 'TurnBased', 'CombatParticipant', 'Experience'}} _G.bg3se_sweep_onchange = s local function handler(e, comp, fields) s.n = s.n + 1 local nm = tostring(comp) s.per[nm] = (s.per[nm] or 0) + 1 local d = nm .. ' entity=' .. tostring(e) .. ' fields=' .. string.format('0x%x', fields) if s.first == nil then s.first = d end s.last = d end for _, t in ipairs(s.types) do s.per[t] = s.per[t] or 0 local ok, id = pcall(Ext.Entity.OnChange, t, handler) if ok and id ~= nil then s.subs = s.subs + 1 s.ids[t] = id elseif ok then s.refused[#s.refused+1] = t .. ':nil (AddComponentType refused -- extender.log has the deserializer ERR)' else s.refused[#s.refused+1] = t .. ':' .. tostring(id) end end local okc, cerr = pcall(function() s.cid = Ext.Entity.OnCreate('StatusID', function() s.created = s.created + 1 end) s.did = Ext.Entity.OnDestroy('StatusID', function() s.destroyed = s.destroyed + 1 end) end) s.control = okc and 'ok' or ('raised: ' .. tostring(cerr)) return 'subscribed ' .. s.subs .. '/' .. #s.types .. ' replicated types' .. (#s.refused > 0 and (' refused=' .. table.concat(s.refused, ' | ')) or '') .. ' control(OnCreate/OnDestroy StatusID)=' .. s.control .. ' (the subscriptions exist whether or not DummyFieldTracker::Dispatch() ever fires)' end)()"
    "ctick-clocks:(function() local m = Ext.Timer.MonotonicTime() local u = Ext.Timer.MicrosecTime() local g = Ext.Timer.GameTime() _G.bg3se_sweep_ctimer = {m = m, g = g, u = u} return 'IsClient=' .. tostring(Ext.IsClient()) .. ' Monotonic=' .. tostring(m) .. 'ms Microsec=' .. string.format('%.0f', u) .. 'us GameTime=' .. string.format('%.3f', g) .. 's (IsClient must be true or every ctick-* reading below is the wrong state)' end)()"
    "ctick-subscribe:(function() if _G.bg3se_sweep_ctick == nil then local s = {n = 0, first = nil, last = nil, t0 = nil, t1 = nil, mono = 0, scaled = 0} s.id = Ext.Events.Tick:Subscribe(function(e) s.n = s.n + 1 local ok, d = pcall(function() local t = e.Time if s.t1 ~= nil and t.Ticks > s.t1 then s.mono = s.mono + 1 end if s.t0 == nil then s.t0 = t.Ticks end s.t1 = t.Ticks if math.abs(t.DeltaTime - t.UnscaledDeltaTime) < 1e-9 then s.scaled = s.scaled + 1 end return string.format('Time=%.3f DeltaTime=%.5f UnscaledDeltaTime=%.5f Ticks=%d', t.Time, t.DeltaTime, t.UnscaledDeltaTime, t.Ticks) end) s.last = ok and d or ('raised: ' .. tostring(d)) if s.first == nil then s.first = s.last end end) _G.bg3se_sweep_ctick = s end return 'id=' .. tostring(_G.bg3se_sweep_ctick.id) .. ' (the subscription exists whether or not ecl::ScriptExtender::OnECSTick ever fires)' end)()"
    "ctick-nexttick:(function() if _G.bg3se_sweep_cnext ~= nil then return 'already registered' end local s = {a = 0, b = 0, at = {}} _G.bg3se_sweep_cnext = s local function note(w) local t = _G.bg3se_sweep_ctick s.at[#s.at+1] = w .. '@tick' .. tostring(t and t.n or -1) end Ext.OnNextTick(function() s.a = s.a + 1 note('a') end) Ext.OnNextTick(function() s.b = s.b + 1 note('b') end) return 'two Ext.OnNextTick callbacks registered (Once = true subscriptions on Ext.Events.Tick)' end)()"
    "ctick-waitfor:(function() if _G.bg3se_sweep_cwaits == nil then _G.bg3se_sweep_cwaits = {game = 0, real = 0} end local w = _G.bg3se_sweep_cwaits local function fw(h) if math.type(h) ~= 'integer' then return type(h) .. '/' .. tostring(math.type(h)) end return tostring(h // 4294967296) end w.gh = Ext.Timer.WaitFor(500, function() w.game = w.game + 1 end) w.rh = Ext.Timer.WaitForRealtime(500, function() w.real = w.real + 1 end) return 'WaitFor=' .. tostring(w.gh) .. ' flagword=' .. fw(w.gh) .. ' (want 0) WaitForRealtime=' .. tostring(w.rh) .. ' flagword=' .. fw(w.rh) .. ' (want 2 = RealtimeFlag 0x200000000 >> 32)' end)()"
    # input-manager is the in-game witness for the AllowDeviceEvents layout: with upstream's
    # the class is unproxyable -- its second bool reads stale heap at +0x2f1 --
    # and a mod that asks for it at client startup then indexes the nil it gets back.  Expected:
    # `input::InputManager validate=true`, with the counts non-zero on a running client.
    # Client-side only: ls::gInputManager is the client process's.
    "input-manager:(function() local m = Ext.Input.GetInputManager() if m == nil then return 'nil -- proxying refused; the Validation of property line is in extender.log just above' end local ok, v = pcall(Ext.Types.Validate, m) return Ext.Types.GetObjectType(m) .. ' validate=' .. (ok and tostring(v) or ('raised: ' .. tostring(v))) .. ' ControllerAllowKeyboardMouseInput=' .. tostring(m.ControllerAllowKeyboardMouseInput) .. ' PressedModifiers=' .. tostring(m.PressedModifiers) .. ' LastUpdateTime=' .. string.format('%.1f', m.LastUpdateTime) .. ' definitions=' .. tostring(#m.InputDefinitions) .. ' states=' .. tostring(#m.InputStates) .. ' rawinputs=' .. tostring(#m.RawInputs) .. ' players=' .. tostring(#m.PlayerDevices) .. ' bindings0=' .. tostring(#m.InputScheme.InputBindings[1]) end)()"
    "input-manager-noattr:(function() local m = Ext.Input.GetInputManager() if m == nil then return 'nil -- see input-manager' end local ok, err = pcall(function() return m.AllowDeviceEvents end) return 'AllowDeviceEvents reachable=' .. tostring(ok and err ~= nil) .. ' (the member does not exist in the port; nil or an error is the expected answer) -> ' .. tostring(err) end)()"
    "mat-entities:#Ext.Entity.GetAllEntitiesWithComponent('Visual')"
    "mat-visuals:(function() local es = Ext.Entity.GetAllEntitiesWithComponent('Visual') local n = 0 for i = 1, #es do local c = es[i].Visual if c ~= nil and c.Visual ~= nil then n = n + 1 end end return n .. '/' .. #es .. ' have a Visual' end)()"
    "mat-route:(function() local es = Ext.Entity.GetAllEntitiesWithComponent('Visual') local seen, fallback = 0, nil for i = 1, #es do local v = es[i].Visual and es[i].Visual.Visual local descs = v and v.ObjectDescs if descs ~= nil then for j = 1, #descs do local r = descs[j].Renderable if r ~= nil and r.AppliedMaterials ~= nil then for k = 1, #r.AppliedMaterials do local m = r.AppliedMaterials[k] seen = seen + 1 if fallback == nil then fallback = m end local mm = m.Material local ps = mm and mm.Parameters and mm.Parameters.Texture2DParameters if ps ~= nil then for n = 1, #ps do if tostring(ps[n].ParameterName) ~= '' then _G.bg3se_sweep_mat = m _G.bg3se_sweep_param = tostring(ps[n].ParameterName) return 'entity ' .. i .. '/' .. #es .. ' objdesc ' .. j .. '/' .. #descs .. ' material ' .. k .. '/' .. #r.AppliedMaterials .. ' param=' .. _G.bg3se_sweep_param .. ' name=' .. tostring(m.MaterialName) end end end end end end end end _G.bg3se_sweep_mat = fallback return 'no NAMED Texture2D parameter on any of ' .. seen .. ' materials over ' .. #es .. ' Visual entities (kept ' .. tostring(fallback ~= nil and 'a fallback material' or 'nothing') .. ')' end)()"
    "mat-param:(function() local m = _G.bg3se_sweep_mat if m == nil then return 'no material' end local mm = m.Material local names = {} if mm ~= nil and mm.Parameters ~= nil then for _, p in ipairs(mm.Parameters.Texture2DParameters) do local n = tostring(p.ParameterName) if n ~= '' and _G.bg3se_sweep_param == nil then _G.bg3se_sweep_param = n end names[#names+1] = n end end return tostring(_G.bg3se_sweep_param) .. ' of {' .. table.concat(names, ',') .. '} flags=' .. tostring(m.Flags) end)()"
    "mat-settex-pick:(function() local ids = Ext.Resource.GetAll('Texture') for i = 1, #ids do local r = Ext.Resource.Get(ids[i], 'Texture') if r ~= nil and tostring(r.Type) == 'T2D' and r.Width >= 128 and r.Width <= 1024 and r.Depth <= 1 then _G.bg3se_sweep_tex = ids[i] return ids[i] .. ' ' .. r.Width .. 'x' .. r.Height .. ' of ' .. #ids end end return 'no 2D texture in ' .. #ids end)()"
    "mat-settex-real:(function() local m, t, p = _G.bg3se_sweep_mat, _G.bg3se_sweep_tex, _G.bg3se_sweep_param if m == nil or t == nil or p == nil then return 'no material, texture or parameter' end return 'real=' .. tostring(m:SetTexture2D(p, t)) .. ' on ' .. p end)()"
    "mat-settex-bogus:(function() local m, p = _G.bg3se_sweep_mat, _G.bg3se_sweep_param if m == nil or p == nil then return 'no material or parameter' end return 'bogus=' .. tostring(m:SetTexture2D(p, 'b3f4e1a0-0000-0000-0000-000000000000')) end)()"
    "mat-settex-badparam:(function() local m, t = _G.bg3se_sweep_mat, _G.bg3se_sweep_tex if m == nil or t == nil then return 'no material or texture' end return 'badparam=' .. tostring(m:SetTexture2D('NoSuchParam_bg3se', t)) end)()"
    "mat-vtroute:(function() local es = Ext.Entity.GetAllEntitiesWithComponent('Visual') local seen = 0 for i = 1, #es do local v = es[i].Visual and es[i].Visual.Visual local descs = v and v.ObjectDescs if descs ~= nil then for j = 1, #descs do local r = descs[j].Renderable if r ~= nil and r.AppliedMaterials ~= nil then for k = 1, #r.AppliedMaterials do local m = r.AppliedMaterials[k] seen = seen + 1 if _G.bg3se_sweep_vtmat == nil and m.VirtualTextureParams ~= nil and #m.VirtualTextureParams > 0 then _G.bg3se_sweep_vtmat = m end end end end end end return tostring(_G.bg3se_sweep_vtmat ~= nil) .. ' over ' .. seen .. ' materials on ' .. #es .. ' Visual entities' end)()"
    "cshape-fields:(function() local es = Ext.Entity.GetAllEntitiesWithComponent('Visual') local shapes, bad, first = 0, 0, nil for i = 1, #es do local v = es[i].Visual and es[i].Visual.Visual local descs = v and v.ObjectDescs if descs ~= nil then for j = 1, #descs do local r = descs[j].Renderable if r ~= nil then local ok, tf = pcall(function() return r.TriangulationFailed end) if ok and tf ~= nil then shapes = shapes + 1 local okl, lt = pcall(function() return r.Lifetime end) local oku, us = pcall(function() return r.UseShapeRender end) if type(tf) ~= 'boolean' or not okl or type(lt) ~= 'number' or lt ~= lt or not oku or type(us) ~= 'boolean' then bad = bad + 1 end if first == nil then local okf, f4 = pcall(function() return r.field_F4 end) first = 'TriangulationFailed=' .. tostring(tf) .. ' Lifetime=' .. tostring(lt) .. ' UseShapeRender=' .. tostring(us) .. ' field_F4=' .. tostring(okf and f4 or 'refused') end end end end end end if shapes == 0 then return 'no ls::Shape among the renderables of ' .. #es .. ' Visual entities' end return shapes .. ' Shape renderables, ' .. bad .. ' with a member out of range; first: ' .. first .. ' (ls::Shape has no uint32 Preset on the native build, so TriangulationFailed is at 0xee and not 0xf4 -- a refusal, a nil or a non-boolean here is the ls::Shape mirror regressing)' end)()"
    "mat-vtparam:(function() local m = _G.bg3se_sweep_vtmat if m == nil then return 'no material with VirtualTextureParams' end local names = {} for i, p in ipairs(m.VirtualTextureParams) do names[i] = tostring(p.ParameterName) if _G.bg3se_sweep_vtparam == nil and names[i] ~= '' then _G.bg3se_sweep_vtparam = names[i] end end return tostring(_G.bg3se_sweep_vtparam) .. ' of {' .. table.concat(names, ',') .. '} on ' .. tostring(m.MaterialName) .. ' flags=' .. tostring(m.Flags) end)()"
    "mat-setvt-pick:(function() local ids = Ext.Resource.GetAll('VirtualTexture') if ids == nil then return 'Ext.Resource.GetAll(VirtualTexture) is nil' end for i = 1, #ids do local r = Ext.Resource.Get(ids[i], 'VirtualTexture') if r ~= nil then _G.bg3se_sweep_vtex = ids[i] return ids[i] .. ' of ' .. #ids .. ' (' .. tostring(r.GTexFileName) .. ')' end end return 'no VirtualTexture resource in ' .. #ids end)()"
    "mat-setvt-real:(function() local m, t, p = _G.bg3se_sweep_vtmat, _G.bg3se_sweep_vtex, _G.bg3se_sweep_vtparam if m == nil or t == nil or p == nil then return 'no material, virtual texture or parameter' end return 'real=' .. tostring(m:SetVirtualTexture(p, t)) .. ' on ' .. p end)()"
    "mat-setvt-bogus:(function() local m, p = _G.bg3se_sweep_vtmat, _G.bg3se_sweep_vtparam if m == nil or p == nil then return 'no material or parameter' end return 'bogus=' .. tostring(m:SetVirtualTexture(p, 'b3f4e1a0-0000-0000-0000-000000000000')) end)()"
    "mat-setvt-badparam:(function() local m, t = _G.bg3se_sweep_vtmat, _G.bg3se_sweep_vtex if m == nil or t == nil then return 'no material or virtual texture' end return 'badparam=' .. tostring(m:SetVirtualTexture('NoSuchParam_bg3se', t)) end)()"
    "blend-route:(function() local es = Ext.Entity.GetAllEntitiesWithComponent('Visual') local seen, withbs = 0, 0 for i = 1, #es do local v = es[i].Visual and es[i].Visual.Visual local descs = v and v.ObjectDescs if descs ~= nil then for j = 1, #descs do local r = descs[j].Renderable if r ~= nil then seen = seen + 1 if _G.bg3se_sweep_rend == nil then _G.bg3se_sweep_rend = r end if r.BlendShape ~= nil then withbs = withbs + 1 if _G.bg3se_sweep_blend == nil then _G.bg3se_sweep_blend = r end end end end end end return withbs .. '/' .. seen .. ' renderables over ' .. #es .. ' Visual entities have a BlendShape' end)()"
    "blend-fields:(function() local es = Ext.Entity.GetAllEntitiesWithComponent('Visual') local seen, withbs, bad, first = 0, 0, 0, nil for i = 1, #es do local v = es[i].Visual and es[i].Visual.Visual local descs = v and v.ObjectDescs if descs ~= nil then for j = 1, #descs do local r = descs[j].Renderable if r ~= nil then seen = seen + 1 if r.BlendShape ~= nil then withbs = withbs + 1 local ok, n = pcall(function() return #r.BlendShape.Weights end) if not ok or type(n) ~= 'number' or n < 0 or n > 4096 or n % 1 ~= 0 then bad = bad + 1 if first == nil then first = 'refused: ' .. tostring(ok and n or 'error') end elseif first == nil then first = 'weights=' .. n end end end end end end return withbs .. '/' .. seen .. ' renderables with a BlendShape, ' .. bad .. ' with an unreadable Weights; first: ' .. tostring(first) .. ' (BlendShapeObjectDataVK has three buffers on the native build, so Weights is at 0x50 and not 0x40 -- a refusal or a count outside 0..4096 here is the BlendShapeObjectDataVK mirror regressing)' end)()"
    "blend-refused:(function() local r, kind = _G.bg3se_sweep_blend, 'with a BlendShape' if r == nil then r, kind = _G.bg3se_sweep_rend, 'without a BlendShape' end if r == nil then return 'no renderable at all' end local w = r.BlendShape local n = w and #w.Weights or -1 local cleared = r:ClearBlendShapeWeights('') local set = r:SetBlendShapeWeight('WrinkleWeightsR', 0.5) return 'renderable ' .. kind .. ' weights=' .. n .. ' clear=' .. tostring(cleared) .. ' set=' .. tostring(set) end)()"
    "stats-structure-count:(function() local s = _G.bg3se_sweep_stats if s == nil then return 'no counter: the Load-context state that ran BootstrapClient was reset' end return tostring(s.structure) .. ' StatsStructureLoaded, last at ' .. tostring(s.at[#s.at]) end)()"
    "stats-weapon-count:#Ext.Stats.GetStats('Weapon')"
    "stats-one:(function() local o = Ext.Stats.Get('WPN_Dagger') if o == nil then local ids = Ext.Stats.GetStats('Weapon') o = Ext.Stats.Get(ids[1]) end return tostring(o and o.Name) .. ' mod=' .. tostring(o and o.ModId) .. ' orig=' .. tostring(o and o.OriginalModId) end)()"
    "ctick-advance:(function() local s = _G.bg3se_sweep_ctimer if s == nil then return 'ctick-clocks did not run' end local m, u, g = Ext.Timer.MonotonicTime(), Ext.Timer.MicrosecTime(), Ext.Timer.GameTime() return string.format('dMonotonic=%dms dMicrosec=%.0fus dGameTime=%.3fs -- the first two are the QPC shim and advance regardless; the third is the client ExtensionState time_ set from ecs::EntityWorld::Update, and near 0 with dMonotonic large is the client tick not reaching Lua', m - s.m, u - s.u, g - s.g) end)()"
    "ctick-tick-counts:(function() local s = _G.bg3se_sweep_ctick if s == nil then return 'not subscribed' end local span = (s.t0 ~= nil and s.t1 ~= nil) and (s.t1 - s.t0) or -1 return 'ticks=' .. s.n .. ' increased=' .. s.mono .. '/' .. math.max(s.n - 1, 0) .. ' span=' .. span .. ' scale1=' .. s.scaled .. '/' .. s.n .. ' first=' .. tostring(s.first) .. ' last=' .. tostring(s.last) .. ' (client half of timer-tick-counts: increased must equal the denominator, span >= ticks, DeltaTime in the 1/30..1/300s band, and scale1 = n while the time scale is 1.0)' end)()"
    "ctick-nexttick-count:(function() local s = _G.bg3se_sweep_cnext if s == nil then return 'ctick-nexttick did not run' end return 'a=' .. s.a .. ' b=' .. s.b .. ' fired ' .. table.concat(s.at, ',') .. ' (both want exactly 1: Ext.OnNextTick is a Once subscription, so 0 is the client tick never firing and >1 is Once not honoured; the tick index says it was the next tick and not a later one)' end)()"
    "ctick-counts:(function() local w = _G.bg3se_sweep_cwaits if w == nil then return 'ctick-waitfor did not run' end return 'WaitFor fired=' .. w.game .. ' WaitForRealtime fired=' .. w.real .. ' (both want >= 1; the client managers ride the same time.Time as the server ones do, so both fire or neither does, and neither firing is OnECSTick never reaching State::OnUpdate)' end)()"
    "onchange-counts:(function() local s = _G.bg3se_sweep_onchange if s == nil then return 'onchange-subscribe did not run' end local per = {} for _, t in ipairs(s.types) do per[#per+1] = t .. '=' .. tostring(s.per[t] or 0) end local extra = {} for k, v in pairs(s.per) do local known = false for _, t in ipairs(s.types) do if t == k then known = true end end if not known then extra[#extra+1] = k .. '=' .. v end end local verdict if s.n > 0 then verdict = 'WITNESS: the fix works -- DummyFieldTracker::Dispatch() fired and the handler ran' elseif s.created > 0 then verdict = 'SUSPECT: statuses appeared on this client in the window (control created=' .. s.created .. ') and no change event fired. A component *create* goes through ComponentDeserializer<T>::DeserializeComponent, which calls no tracker at all, so this is a failure only if the window also held a change to an existing component -- which run 2 guarantees and run 1 does not (run 1 subscribes after its own server pass, so it reads 0/SUSPECT by construction). Read run 2.' else verdict = 'INCONCLUSIVE: nothing replicated a status to this client in the window (control created=0), so 0 events says nothing either way' end return 'events=' .. s.n .. ' subs=' .. s.subs .. '/' .. #s.types .. ' per: ' .. table.concat(per, ' ') .. (#extra > 0 and (' other: ' .. table.concat(extra, ' ')) or '') .. ' control: created=' .. s.created .. ' destroyed=' .. s.destroyed .. ' first=' .. tostring(s.first) .. ' last=' .. tostring(s.last) .. ' -- ' .. verdict end)()"
    # ctimeline-* is the in-game witness for ecl::TimelineSystem's layout. Upstream's
    # `void* field_18` -- a Win32 slot the native build does not have -- puts Lighting at 0x378
    # where the Linux ctor (bin/bg3 0x50378a0) lea's 0x370, which reads as `Validation of
    # property 'VolumetricShadow' failed on LightingDetails` -> `'Lighting' failed on
    # ecl::TimelineSystem`.  Expected: validate=true, a
    # LightingDetails whose bools read as bools, and a MaterialGroups the Head VFX UAP lookup can
    # find.  A `nil` system is the map refusing it; an ERR here is the fix not holding in game.
    "ctimeline-system:(function() local s = Ext.System.ClientTimeline if s == nil then return 'nil -- the system map refused it; the Validation line is in extender.log just above' end local ok, v = pcall(Ext.Types.Validate, s) return Ext.Types.GetObjectType(s) .. ' validate=' .. (ok and tostring(v) or ('raised: ' .. tostring(v))) .. ' SessionGuid=' .. tostring(s.SessionGuid) end)()"
    "ctimeline-lighting:(function() local s = Ext.System.ClientTimeline if s == nil then return 'no system' end local l = s.Lighting if l == nil then return 'Lighting is nil' end local ok, v = pcall(Ext.Types.Validate, l) local function b(x) return tostring(x) .. '/' .. type(x) end return 'validate=' .. (ok and tostring(v) or ('raised: ' .. tostring(v))) .. ' Type=' .. tostring(l.Type) .. ' CastShadow=' .. b(l.CastShadow) .. ' VolumetricShadow=' .. b(l.VolumetricShadow) .. ' PreExpose=' .. b(l.PreExpose) .. ' Radius=' .. tostring(l.Radius) .. ' Intensity=' .. tostring(l.Intensity) .. ' ScatteringScale=' .. tostring(l.ScatteringScale) .. ' (all three bools must be booleans, not numbers, and the floats finite)' end)()"
    "ctimeline-materialgroups:(function() local s = Ext.System.ClientTimeline if s == nil then return 'no system' end local ok, r = pcall(function() local m = s.MaterialGroups return tostring(m) .. ' type=' .. Ext.Types.GetObjectType(m) end) return 'MaterialGroups ' .. (ok and r or ('raised: ' .. tostring(r))) .. ' -- Head VFX UAP asks for ClientTimeline and MaterialGroups by name' end)()"
    "cvisual-query:(function() local vs = Ext.Entity.GetAllEntitiesWithComponent('Visual') local key, withc = {}, 0 for i = 1, #vs do key[tostring(vs[i])] = true local c = vs[i].Visual if c ~= nil and c.Visual ~= nil then withc = withc + 1 end end local out = {} local function route(label, es, cap) if es == nil then out[#out+1] = label .. '=nil' return end local n = math.min(#es, cap) local withv, inq = 0, 0 local ok, err = pcall(function() for i = 1, n do local c = es[i].Visual if c ~= nil and c.Visual ~= nil then withv = withv + 1 if key[tostring(es[i])] then inq = inq + 1 end end end end) out[#out+1] = label .. '=' .. #es .. (n < #es and (' scanned=' .. n) or '') .. ' withVisual=' .. withv .. ' inVisualQuery=' .. inq .. (ok and '' or (' raised: ' .. tostring(err))) end local okc, cc = pcall(Ext.Entity.GetAllEntitiesWithComponent, 'ClientCharacter') route('ClientCharacter', okc and cc or nil, 4096) local oka, all = pcall(Ext.Entity.GetAllEntities) route('GetAllEntities', oka and all or nil, 20000) return 'Visual query=' .. #vs .. ' withVisual=' .. withc .. ' -- ' .. table.concat(out, ' ') .. ' (inVisualQuery < withVisual on either route is the single-component query missing part of the pool; equal on both says the single-component query covers the pool)' end)()"
    "cquery-route:(function() local names = {'Visual', 'Transform', 'Bound', 'ClientCharacter'} local q, scan = {}, {} for _, n in ipairs(names) do local ok, r = pcall(Ext.Entity.GetAllEntitiesWithComponent, n) q[n] = (ok and r ~= nil) and #r or -1 scan[n] = 0 end local all = Ext.Entity.GetAllEntities() for i = 1, #all do local e = all[i] for _, n in ipairs(names) do local okc, c = pcall(function() return e[n] end) if okc and c ~= nil then scan[n] = scan[n] + 1 end end end local out, agree = {}, 0 for _, n in ipairs(names) do if q[n] == scan[n] then agree = agree + 1 end out[#out+1] = n .. ' query=' .. q[n] .. ' scan=' .. scan[n] .. (q[n] == scan[n] and '' or ' MISMATCH') end return agree .. '/' .. #names .. ' agree, entities=' .. #all .. ' -- ' .. table.concat(out, ' ') .. ' (the query route is GetAllEntitiesWithComponent, which walks the game single-component query bound in UpdateQueryCache; the scan route is GetAllEntities filtered by the component itself. Equal on every name means UpdateQueryCache binds the right branch; a query count far below the scan count (query=7 against scan=2762) means it does not)' end)()"
    # ui-* is the client UI block, the sweep's only probes of
    # ext/src/BG3Extender/Lua/Libs/ClientUI/.  Both entry
    # points go through ls::gGlobalResourceManager -> ui::GameUI, which is a manifest row, so a
    # missing row is a named LuaError rather than a crash (Module.inl guards the null itself).
    # ui-root reads ui::GameUI +0xe0 (NoesisUIManager.MainCanvas) and ui-statemachine +0x818
    # (StateMachine.StateMachineComponent) -- the second is the member 0x40 below where
    # upstream's two Win32 slots would put it, witnessed at bin/bg3 0x2472930
    # (gui::GameUI::IsUIModal reads [r14+0x818]); garbage or a refusal there is that measurement
    # being wrong and not the API.  ui-tree walks the visual tree: VisualChild and Child are both
    # **1-based** here (Noesis::VisualHelpers::GetVisualChild refuses index 0 and passes index-1
    # on), which is the trap a 0-based walk falls into -- it would report every node as childless.
    # ui-datacontext is the property-by-name half: a Noesis DependencyObject stores only the
    # properties something has written into DependencyObject::mValues, so an *unset* dependency
    # property reads nil rather than its default, and nil is therefore not by itself a refusal.
    # ui-write-opacity is the one write, and it is deliberately **left applied**: the frame pair
    # (a shot before the sweep, a shot after it) is the witness that the write reached the game's
    # renderer, which no log line can give.  Restore with `Ext.UI.GetRoot().Opacity = 1.0` on the
    # client console.  It runs **before** the two ui-dc- probes, because those can kill the game
    # (rip=0, a call through a null pointer; an open reading) and
    # a shot cannot be taken of a process that is gone.  ui-dc-scan prints a SWEEP-UI line before
    # each node it reads for exactly that reason: the probe is one console line, so without
    # those lines the log cannot say which of the three null-call candidates
    # (the ClassCache populate, the mValues lookup, the polymorphic push of the stored value)
    # a crash was in.
    "ui-root:(function() local ok, r = pcall(Ext.UI.GetRoot) if not ok then return 'raised: ' .. tostring(r) end if r == nil then return 'nil -- GetRoot returned nothing. A LuaError naming ls::gGlobalResourceManager is that manifest row missing; any other error is in extender.log just above; nil with no error at all is MainCanvas not created yet on this client' end _G.bg3se_sweep_uiroot = r local function g(o, n) local o2, v = pcall(function() return o[n] end) if not o2 then return nil end return v end local function count(x) if type(x) ~= 'table' then return -1 end local n = 0 for _ in pairs(x) do n = n + 1 end return n end local okti, ti = pcall(function() return r:TypeInfo() end) local tprops, tdeps, tbase = -1, -1, 'none' if okti and ti ~= nil then tprops = count(g(ti, 'Properties')) tdeps = count(g(ti, 'DependencyProperties')) local b = g(ti, 'Base') if b ~= nil then tbase = 'a TypeClass' end end local okt, ot = pcall(Ext.Types.GetObjectType, r) return (okt and tostring(ot) or 'GetObjectType raised') .. ' Type=' .. tostring(g(r, 'Type')) .. ' TypeInfo=' .. (okti and tostring(ti ~= nil) or ('raised: ' .. tostring(ti))) .. ' TypeInfo.Properties=' .. tprops .. ' TypeInfo.DependencyProperties=' .. tdeps .. ' TypeInfo.Base=' .. tbase .. ' visualChildren=' .. tostring(g(r, 'VisualChildrenCount')) .. ' logicalChildren=' .. tostring(g(r, 'ChildrenCount')) .. ' refs=' .. tostring(g(r, 'NumReferences')) .. ' properties=' .. (function() local okp, all = pcall(function() return r:GetAllProperties() end) if not okp then return 'raised: ' .. tostring(all) end return tostring(count(all)) end)() .. ' (TypeInfo is a P_FUN and must be CALLED, and Noesis::TypeClass deliberately inherits nothing from Noesis::Type, so it has no Name -- .Type is the class name here (treating TypeInfo as a getter reads nil). The root is ui::GameUI +0xe0 NoesisUIManager.MainCanvas narrowed to Noesis::FrameworkElement*)' end)()"
    "ui-tree:(function() local r = _G.bg3se_sweep_uiroot if r == nil then local ok, rr = pcall(Ext.UI.GetRoot) r = ok and rr or nil end if r == nil then return 'no root -- see ui-root' end local kinds, n, maxd, bad, head = {}, 0, 0, 0, {} local function walk(el, d) if n >= 400 or d > 12 then return end n = n + 1 if d > maxd then maxd = d end local okt, t = pcall(function() return tostring(el.Type) end) if not okt then bad = bad + 1 t = 'REFUSED' end kinds[t] = (kinds[t] or 0) + 1 if #head < 14 then head[#head+1] = d .. ':' .. t end local okc, c = pcall(function() return el.VisualChildrenCount end) if not okc or type(c) ~= 'number' then bad = bad + 1 return end for i = 1, c do local okk, ch = pcall(function() return el:VisualChild(i) end) if okk and ch ~= nil then walk(ch, d + 1) elseif not okk then bad = bad + 1 end end end walk(r, 0) local ks, nk = {}, 0 for k, v in pairs(kinds) do ks[#ks+1] = k .. '=' .. v nk = nk + 1 end table.sort(ks) return n .. ' visual nodes (cap 400, depth cap 12), maxdepth=' .. maxd .. ', ' .. nk .. ' distinct types, ' .. bad .. ' reads refused; types: ' .. table.concat(ks, ',') .. ' -- head: ' .. table.concat(head, ' ') .. ' (n=1 with maxdepth=0 in a loaded save is the walk not descending rather than an empty HUD, and the likeliest cause is a 0-based index: VisualChild refuses 0 and passes index-1 on. A type name of REFUSED is NoesisPush failing to narrow the class, which needs the Noesis::TypeClass chain in Symbols.inl to be bound)' end)()"
    "ui-statemachine:(function() local ok, sm = pcall(Ext.UI.GetStateMachine) if not ok then return 'raised: ' .. tostring(sm) end if sm == nil then return 'nil -- a nil here is NOT raised and has two causes: ui-root ERRs too (the resource-manager row is missing from the manifest), or the proxy was refused. Grep extender.log for \"failed validation\" / \"proxying of this class is disabled\" on ui::UIStateMachine -- that is what a RootState at the Windows 0x200 instead of the native 0x270 produces' end _G.bg3se_sweep_uism = sm local function g(o, n) local o2, v = pcall(function() return o[n] end) if not o2 then return nil end return v end local okv, v = pcall(Ext.Types.Validate, sm) local okt, ot = pcall(Ext.Types.GetObjectType, sm) local st = g(sm, 'State') local wtxt = 'no State' if st ~= nil then local okvs, vs = pcall(Ext.Types.Validate, st) wtxt = 'State.validate=' .. (okvs and tostring(vs) or ('raised: ' .. tostring(vs))) .. ' State.StateGuid=' .. tostring(g(st, 'StateGuid')) .. ' State.TutorialLayer=' .. tostring(g(st, 'TutorialLayer')) .. ' State.field_40=' .. tostring(g(st, 'field_40')) .. ' State.field_44=' .. tostring(g(st, 'field_44')) .. ' State.Layout=' .. tostring(g(st, 'Layout')) .. ' State.Owner=' .. tostring(g(st, 'Owner')) .. ' State.StateWidgets=' .. tostring(g(st, 'StateWidgets') ~= nil) .. ' State.Template=' .. tostring(g(st, 'State') ~= nil) .. ' ' local ws = g(st, 'Widgets') local okl, nw = pcall(function() return #ws end) if not okl or ws == nil then wtxt = wtxt .. 'State.Widgets unreadable' else wtxt = wtxt .. 'State.Widgets=' .. tostring(nw) local w = (nw > 0) and g(ws, 1) or nil if w ~= nil then wtxt = wtxt .. ' widget1: Type=' .. tostring(g(w, 'Type')) .. ' FileName=' .. tostring(g(w, 'FileName')) .. ' StateId=' .. tostring(g(w, 'StateId')) .. ' LayerIndex=' .. tostring(g(w, 'LayerIndex')) .. ' IndexInLayer=' .. tostring(g(w, 'IndexInLayer')) .. ' Flags=' .. tostring(g(w, 'Flags')) .. ' PlayerId=' .. tostring(g(w, 'PlayerId')) end end end return (okt and tostring(ot) or 'GetObjectType raised') .. ' validate=' .. (okv and tostring(v) or ('raised: ' .. tostring(v))) .. ' PlayerID=' .. tostring(g(sm, 'PlayerID')) .. ' RootState=' .. tostring(g(sm, 'RootState')) .. ' State=' .. tostring(st) .. ' ' .. wtxt .. ' (this object is ui::GameUI +0x818. RootState is the witnessed reading: it is a Noesis::String at 0x270, asserted from bin/bg3 0x418d6e1, and it sits past three 48-byte ls::CriticalSection in WindowManager, so a plausible RootState is the measured 0x40 shift holding in game. PlayerID is NOT a witness and must not be read as one: the mirror puts it at 0x400 from the Windows header, and the destructor at bin/bg3 0x508a70f..0x508a747 tears a container down at 0x400 instead, so whatever this prints is unmeasured. ui::UIStateInstance is measured end to end on bin/bg3 a187af94 -- the native class has no States array, what upstream calls State is the template pointer at 0x20 and a 48-byte ls::CriticalSection sits at 0x30, so everything from PlayerID on is 0x28 further along than the Windows header puts it; thirteen asserts carry those addresses. A nil State with a \"Validation of property\" line for ui::UIStateInstance in extender.log is that measurement being wrong, not the API. The widget itself is ui::UIWidget, measured on bin/bg3 0x3cc3400 with eleven asserts)' end)()"
    "ui-write-opacity:(function() local r = _G.bg3se_sweep_uiroot if r == nil then local ok, rr = pcall(Ext.UI.GetRoot) r = ok and rr or nil end if r == nil then return 'no root -- see ui-root' end local function rd() local ok, v = pcall(function() return r.Opacity end) if not ok then return 'raised: ' .. tostring(v) end return v end local before = rd() local oks, err = pcall(function() r.Opacity = 0.35 end) local after = rd() local okd, deps = pcall(function() return r:DependencyProperties() end) local listed = 'not listed' if okd and type(deps) == 'table' then listed = tostring(deps.Opacity) elseif not okd then listed = 'DependencyProperties raised: ' .. tostring(deps) end return 'before=' .. tostring(before) .. ' set=' .. (oks and 'ok' or ('raised: ' .. tostring(err))) .. ' after=' .. tostring(after) .. ' inDependencyProperties=' .. listed .. ' -- THE WRITE IS LEFT APPLIED on purpose: the whole Noesis HUD renders at 35 percent from here on, so a headless.sh shot taken after this sweep differs from one taken before it, and that frame pair is the witness that the write reached the renderer. Restore with: Ext.UI.GetRoot().Opacity = 1.0 on the client console. before=nil is expected and is not a failure -- Noesis reads an unset dependency property out of DependencyObject::mValues, which has no entry until something writes one, so nil -> 0.35 is exactly the write landing, and 0.35 -> 0.35 means an earlier run of this sweep already did it' end)()"
    "ui-dc-scan:(function() local r = _G.bg3se_sweep_uiroot if r == nil then local ok, rr = pcall(Ext.UI.GetRoot) r = ok and rr or nil end if r == nil then return 'no root -- see ui-root' end local n, found, refused, dcel, dc, kinds = 0, 0, 0, nil, nil, {} local function walk(el, d) if n >= 400 or d > 12 then return end n = n + 1 local okt, t = pcall(function() return tostring(el.Type) end) t = okt and t or 'REFUSED' Ext.Utils.Print('SWEEP-UI dc node=' .. n .. ' depth=' .. d .. ' type=' .. t) local okd, val = pcall(function() return el.DataContext end) if not okd then refused = refused + 1 elseif val ~= nil then found = found + 1 kinds[t] = (kinds[t] or 0) + 1 if dc == nil then dcel, dc = el, val end end local okc, c = pcall(function() return el.VisualChildrenCount end) if okc and type(c) == 'number' then for i = 1, c do local okk, ch = pcall(function() return el:VisualChild(i) end) if okk and ch ~= nil then walk(ch, d + 1) end end end end walk(r, 0) _G.bg3se_sweep_uidc = dc local ks = {} for k, v in pairs(kinds) do ks[#ks+1] = k .. '=' .. v end table.sort(ks) return found .. '/' .. n .. ' visual nodes carry a DataContext, ' .. refused .. ' reads raised; carriers: ' .. (#ks > 0 and table.concat(ks, ',') or 'none') .. ' (reading DataContext is the first thing in the tree that goes through Noesis::ObjectHelpers::FallbackGetProperty, so it is also the first that populates a gClassCache ClassDefinition per distinct class and the first that pushes a stored dependency-property value. The SWEEP-UI line printed BEFORE each read is the instrument: a crash here is rip=0 with no progress inside the probe, so the last SWEEP-UI line in the log names the node and the class that did it. found=0 is not a refusal -- Noesis keeps only written dependency properties in DependencyObject::mValues)' end)()"
    "ui-dc-read:(function() local dc = _G.bg3se_sweep_uidc if dc == nil then return 'no DataContext -- see ui-dc-scan' end local function g(o, k) local o2, val = pcall(function() return o[k] end) if not o2 then return nil end return val end local function say(x) Ext.Utils.Print('SWEEP-UI dcread ' .. x) end local okt, ot = pcall(Ext.Types.GetObjectType, dc) local cls = (okt and tostring(ot) or 'GetObjectType raised') .. '/' .. tostring(g(dc, 'Type')) say('class=' .. cls) say('step=DirectProperties') local okd, direct = pcall(function() return dc:DirectProperties() end) say('step=DependencyProperties') local okp, deps = pcall(function() return dc:DependencyProperties() end) say('step=GetAllProperties') local oka, all = pcall(function() return dc:GetAllProperties() end) local function count(x) if type(x) ~= 'table' then return -1 end local n = 0 for _ in pairs(x) do n = n + 1 end return n end local names = {} if oka and type(all) == 'table' then for k in pairs(all) do names[#names+1] = tostring(k) end end table.sort(names) local one, val, tried = nil, nil, 0 for _, k in ipairs(names) do tried = tried + 1 say('step=GetProperty name=' .. k) local okr, rv = pcall(function() return dc:GetProperty(k) end) if okr and rv ~= nil then one, val = k, rv break end end say('step=done') local shown = {} for i = 1, math.min(#names, 12) do shown[i] = names[i] end return cls .. ' DirectProperties=' .. (okd and tostring(count(direct)) or ('raised: ' .. tostring(direct))) .. ' DependencyProperties=' .. (okp and tostring(count(deps)) or ('raised: ' .. tostring(deps))) .. ' GetAllProperties=' .. (oka and tostring(count(all)) or ('raised: ' .. tostring(all))) .. ' {' .. table.concat(shown, ',') .. '} -- GetProperty by name: ' .. (one ~= nil and (one .. '=' .. tostring(val) .. ' (type ' .. type(val) .. ')') or ('nothing non-nil in ' .. tried .. ' tries')) .. ' (this probe can kill the game with rip=0 rax=0, and ui-dc-scan passing right before it rules the DataContext READ out -- the crash is in one of the four steps above. The SWEEP-UI dcread lines are the instrument: the last one in the log names the step, and for GetProperty the property. The three list functions are P_FUN and must be CALLED, and their Lua names have no Get prefix: DirectProperties, DependencyProperties, GetAllProperties. ObjectHelpers::GetNamedProperty logs an OsiError and pushes nil for a name the class does not have, so a nil is not a refusal)' end)()"
)
# ---- the remote net probes ----
# Every probe above runs against one game.  These run against **two**: the `host` instance of
# the LAN rig (`headless.sh --instance host`, a server) and the `join` one (a client that
# joined its lobby).  Each entry is `role:name:expr`, role being the console the line goes to
# -- `host` the host instance's *server* console, `join` the joiner's *client* console -- and
# the array order is the order of the round: the receivers register their listeners, then each
# direction is sent, then both logs are read.
#
# `@TOK@` is substituted with this round's token and `@PEERUSER@` with the remote peer's UserId,
# which the driver discovers from the host's own handshake line ("Got extender support
# notification from user N") or takes from $BG3SE_SWEEP_PEER_USER.
# The payload of every send carries `<tok>-<direction>`, so an arrival line is unique to the
# round and a stale one from an earlier launch in the same appended log cannot be read as this
# round's (`extender.log` appends; that trap is what `--net-selftest` calibrates).
#
# Why a driver that holds both consoles rather than a flag per half: only the driver can say
# "this send arrived there", and on a one-box rig both sockets are local.  `--net-half ROLE TOK`
# is the same probes against one console for the two-box case, where the
# arrival has to be read on the other box by hand.
#
# What each direction witnesses:
#   bcast  -- `Ext.Net.BroadcastMessage` from the host server reaches the *remote* client.
#   touser -- `Ext.Net.PostMessageToUser(<remote UserId>, …)`, the one send that is addressed
#             to a peer rather than broadcast and that does not need a character; its Linux
#             guard (`ServerNet.inl:51`) takes the in-process path only for peer 1, so a peer
#             2 user is the branch that reaches the wire.
#   tochar -- `Ext.Net.PostMessageToClient(<remote character Guid>, …)`.  A lobby has no
#             characters, so this one reads `skipped` there; in a launched game the joiner's
#             own character carries it.  The lookup matches the *peer*, not the UserId: a
#             message's `UserID` is `UserId(peer, 0)` and a character's is `UserId(peer, slot)`
#             -- the joiner's character reads 131073 against a message user of 131072,
#             the same off-by-a-slot as the host's 65537/65536.
#   c2s    -- `Ext.Net.PostMessageToServer` from the remote client reaches the host server.
# The module-named (channel) path has no remote variant here: `Ext.Net.CreateChannel` exists
# only in a mod's own Ext table, so it needs the test mod installed in *both* instances'
# profiles -- so it is not faked from the console.
rprobes=(
    "join:net-r-version:'Ext.Net.Version=' .. tostring(Ext.Net.Version()) .. ' IsHost=' .. tostring(Ext.Net.IsHost())"
    "join:net-r-listen:(function() if not _G.bg3se_sweep_r then local function l(n) Ext.RegisterNetListener(n, function(ch, payload, user) Ext.Utils.Print('SWEEP-NETR recv ch=' .. tostring(ch) .. ' payload=' .. tostring(payload) .. ' user=' .. tostring(user)) end) end l('sweep_r_bcast') l('sweep_r_user') l('sweep_r_char') _G.bg3se_sweep_r = true end return 'registered 3 channels' end)()"
    "host:net-r-listen-c2s:(function() if not _G.bg3se_sweep_r_srv then Ext.RegisterNetListener('sweep_r_c2s', function(ch, payload, user) Ext.Utils.Print('SWEEP-NETR recv ch=' .. tostring(ch) .. ' payload=' .. tostring(payload) .. ' user=' .. tostring(user)) end) _G.bg3se_sweep_r_srv = true end return 'registered' end)()"
    "host:net-r-peer:(function() local u = @PEERUSER@ local pu = u - (u % 65536) local n = 0 _G.bg3se_sweep_r_char = nil _G.bg3se_sweep_r_uid = nil if u == 0 then return 'skipped: no remote peer UserId' end local ok, es = pcall(Ext.Entity.GetAllEntitiesWithComponent, 'ServerCharacter') if ok and es ~= nil then for i = 1, #es do local c = es[i].ServerCharacter local oku, uid = pcall(function() return tonumber(c.UserID) end) if oku and uid ~= nil and uid ~= 0 and uid - (uid % 65536) == pu then n = n + 1 if _G.bg3se_sweep_r_char == nil then local okg, g = pcall(function() return es[i].Uuid.EntityUuid end) if okg then _G.bg3se_sweep_r_char = g _G.bg3se_sweep_r_uid = uid end end end end end return 'peer user ' .. tostring(u) .. ', ' .. tostring(n) .. ' server characters on peer ' .. tostring(pu) .. ', uid=' .. tostring(_G.bg3se_sweep_r_uid) .. ' guid=' .. tostring(_G.bg3se_sweep_r_char) end)()"
    "host:net-r-bcast:(function() Ext.Net.BroadcastMessage('sweep_r_bcast', '@TOK@-bcast') return 'sent' end)()"
    "host:net-r-touser:(function() local u = @PEERUSER@ if u == 0 then return 'skipped: no remote peer UserId -- the host logged no extender handshake, so nothing has joined' end Ext.Net.PostMessageToUser(u, 'sweep_r_user', '@TOK@-touser') return 'sent' end)()"
    "host:net-r-tochar:(function() if _G.bg3se_sweep_r_char == nil then return 'skipped: no server character on the remote peer (a lobby has none)' end Ext.Net.PostMessageToClient(_G.bg3se_sweep_r_char, 'sweep_r_char', '@TOK@-tochar') return 'sent' end)()"
    "join:net-r-c2s:(function() Ext.Net.PostMessageToServer('sweep_r_c2s', '@TOK@-c2s') return 'sent' end)()"
)
# one row per direction: name:sender:receiver.  The sender's probe is net-r-<name> and its
# payload carries "<tok>-<name>", which the receiver's listener prints in a SWEEP-NETR line.
rdirs=("bcast:host:join" "touser:host:join" "tochar:host:join" "c2s:join:host")

net_sock() {    # a state dir (XDG_STATE_HOME or the bg3-se-linux under it) -> the dir with the socket
    if [ -S "$1/console.sock" ]; then printf '%s' "$1"
    elif [ -S "$1/bg3-se-linux/console.sock" ]; then printf '%s' "$1/bg3-se-linux"
    else return 1; fi
}

# a socket file outlives the game that made it, so -S is not "a game is listening": connect.
net_alive() { printf 'exit\n' | if command -v socat > /dev/null; then socat -T3 - "UNIX-CONNECT:$1/console.sock" > /dev/null 2>&1; else nc -U -w3 "$1/console.sock" > /dev/null 2>&1; fi; }

net_slice() {   # log mark -> the log from the *first* "<mark> begin" line of this round on
    # The first, not the last: the driver opens one console connection per run of entries with
    # the same role, each of which prints its own begin line, so a side that is written to twice
    # (the joiner: listeners first, its own send last) has a begin line *after* the arrivals the
    # host's sends produced.  Slicing from the last one would hide both host->join arrivals.
    # The mark carries the round's start time, so its first occurrence
    # cannot be an older round's.
    local n
    n=$(grep -aFn "$2 begin" "$1" 2>/dev/null | head -1 | cut -d: -f1)
    [ -n "$n" ] || return 1
    tail -n +"$n" "$1"
}

net_send() {    # sock ctx mark tok peeruser <entries, role stripped>
    local sock=$1 ctx=$2 mark=$3 tok=$4 pu=$5; shift 5
    local p name expr
    {
        printf '%s\n' "$ctx"; sleep 0.5
        printf 'Ext.Utils.Print("%s begin")\n' "$mark"; sleep 0.3
        for p in "$@"; do
            name=${p%%:*}; expr=${p#*:}
            expr=${expr//@TOK@/$tok}; expr=${expr//@PEERUSER@/$pu}
            printf 'do local ok, r = pcall(function() return %s end); Ext.Utils.Print("%s %s: " .. (ok and "ok " or "ERR ") .. tostring(r)) end\n' "$expr" "$mark" "$name"
            sleep 0.7
        done
        sleep 0.5; printf 'exit\n'
    } | if command -v socat > /dev/null; then socat - "UNIX-CONNECT:$sock/console.sock" > /dev/null; else nc -U "$sock/console.sock" > /dev/null; fi
}

net_report() {  # hostlog joinlog mark tok -> the table, and nonzero if any direction failed
    local hl=$1 jl=$2 mark=$3 tok=$4
    local hs js fails=0 arrived=0 expected=0 skipped=0 d name rest snd rcv sslice rslice v a verdict hd jd
    hs=$(net_slice "$hl" "$mark") || { echo "net-pair: no '$mark begin' in $hl -- the host console never answered"; fails=$((fails+1)); }
    js=$(net_slice "$jl" "$mark") || { echo "net-pair: no '$mark begin' in $jl -- the joiner console never answered"; fails=$((fails+1)); }
    printf '%-8s %-12s %-9s %s\n' direction route verdict 'what the sender said'
    for d in "${rdirs[@]}"; do
        name=${d%%:*}; rest=${d#*:}; snd=${rest%%:*}; rcv=${rest#*:}
        if [ "$snd" = host ]; then sslice=$hs; else sslice=$js; fi
        if [ "$rcv" = host ]; then rslice=$hs; else rslice=$js; fi
        v=$(printf '%s\n' "$sslice" | grep -aF "$mark net-r-$name: " | tail -1)
        v=${v##*"net-r-$name: "}
        a=$(printf '%s\n' "$rslice" | grep -a 'SWEEP-NETR' | grep -aF "$tok-$name" | tail -1)
        if [ -z "$v" ]; then verdict=NO-SEND; fails=$((fails+1))
        elif [ "${v#ERR }" != "$v" ]; then verdict=SEND-ERR; fails=$((fails+1))
        elif [ "${v#ok skipped}" != "$v" ]; then verdict=skipped; skipped=$((skipped+1))
        elif [ -n "$a" ]; then verdict=arrived; arrived=$((arrived+1)); expected=$((expected+1))
        else verdict=MISSING; fails=$((fails+1)); expected=$((expected+1))
        fi
        printf '%-8s %-12s %-9s %s\n' "$name" "$snd->$rcv" "$verdict" "${v:-<no verdict line in the sender log>}"
        [ -n "$a" ] && printf '         arrival: %s\n' "$a"
    done
    hd=$(printf '%s\n' "$hs" | grep -ac 'Ext.Net: dropped')
    jd=$(printf '%s\n' "$js" | grep -ac 'Ext.Net: dropped')
    [ "$hd" = 0 ] && [ "$jd" = 0 ] || fails=$((fails+1))
    echo "NETPAIR $arrived/$expected arrived, $skipped skipped, drops host=$hd join=$jd"
    [ "$fails" = 0 ]
}

# --net-selftest calibrates the *reading*, which is the half that can lie: two crafted log
# pairs, one that must pass whole and one whose three plants (a stale arrival above the begin
# marker, a missing arrival, a live drop line) must each be caught.  No game involved.
if [ "${1:-}" = --net-selftest ]; then
    tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
    mark='NETR 00:00:00'; tok='netr-000000'; rc=0
    stale="SWEEP-NETR recv ch=sweep_r_user payload=$tok-touser user=131072"
    {   echo "$stale"                                             # pre-mark: another launch
        echo "Ext.Net: dropped 'mod_x': only in-process delivery"  # pre-mark: another launch
        echo "$mark begin"
        echo "$mark net-r-listen-c2s: ok registered"
        echo "$mark net-r-peer: ok peer user 131072, 0 server characters on it, guid=nil"
        echo "$mark net-r-bcast: ok sent"
        echo "$mark net-r-touser: ok sent"
        echo "$mark net-r-tochar: ok skipped: no server character on the remote user"
        echo "SWEEP-NETR recv ch=sweep_r_c2s payload=$tok-c2s user=131072"
    } > "$tmp/host-a.log"
    {   echo "$stale"                                             # pre-mark: another launch
        echo "$mark begin"                                        # the joiner's first batch
        echo "$mark net-r-version: ok Ext.Net.Version=2 IsHost=false"
        echo "$mark net-r-listen: ok registered 3 channels"
        echo "SWEEP-NETR recv ch=sweep_r_bcast payload=$tok-bcast user=4294901760"
        echo "SWEEP-NETR recv ch=sweep_r_user payload=$tok-touser user=4294901760"
        # the joiner is written to twice, so its second batch prints a *second* begin line below
        # the arrivals the host's sends produced: slicing from the last one loses both of them
        echo "$mark begin"
        echo "$mark net-r-c2s: ok sent"
    } > "$tmp/join-a.log"
    out=$(net_report "$tmp/host-a.log" "$tmp/join-a.log" "$mark" "$tok"); ok=$?
    exp='NETPAIR 3/3 arrived, 1 skipped, drops host=0 join=0'
    if [ "$ok" != 0 ] || ! printf '%s\n' "$out" | grep -qF "$exp"; then
        echo "net-selftest: the clean pair did not pass (wanted '$exp', rc 0):"; printf '%s\n' "$out"; rc=1
    fi
    # the three plants: touser's arrival is only *above* the begin marker (a stale line from an
    # earlier launch), c2s never arrived, and one drop line is inside this round.
    sed '/payload='"$tok"'-touser/d' "$tmp/join-a.log" > "$tmp/join-b.log"
    sed "1i $stale" "$tmp/join-b.log" > "$tmp/join-b2.log" && mv "$tmp/join-b2.log" "$tmp/join-b.log"
    echo "Ext.Net: dropped 'mod_channel': only in-process delivery" >> "$tmp/join-b.log"
    grep -v 'payload='"$tok"'-c2s' "$tmp/host-a.log" > "$tmp/host-b.log"
    out=$(net_report "$tmp/host-b.log" "$tmp/join-b.log" "$mark" "$tok"); ok=$?
    for want in 'touser +host->join +MISSING' 'c2s +join->host +MISSING' 'NETPAIR 1/3 arrived, 1 skipped, drops host=0 join=1'; do
        printf '%s\n' "$out" | grep -qE "$want" || { echo "net-selftest: the planted pair did not report '$want':"; printf '%s\n' "$out"; rc=1; }
    done
    [ "$ok" != 0 ] || { echo "net-selftest: the planted pair passed, which it must not"; rc=1; }
    [ "$rc" = 0 ] && echo "net-selftest: ok -- the clean pair passes 3/3 and all three plants are caught"
    exit "$rc"
fi

# --net-half ROLE [TOK [PEERUSER]] -- one side's probes against the local console
# ($XDG_STATE_HOME), for a round across two boxes where no driver can hold both sockets.
# The two sides have to be given the same TOK, and the arrival is read on the other box.
if [ "${1:-}" = --net-half ]; then
    role=${2:-}; [ "$role" = host ] || [ "$role" = join ] || { echo "usage: $0 --net-half host|join [TOK [PEERUSER]]" >&2; exit 2; }
    tok=${3:-netr-$(date '+%H%M%S')}; pu=${4:-${BG3SE_SWEEP_PEER_USER:-0}}
    sock=$(net_sock "$state") || { echo "no console socket under $state" >&2; exit 2; }
    net_alive "$sock" || { echo "nothing is listening on $sock/console.sock (a stale socket outlives its game)" >&2; exit 2; }
    mark="NETR $(date '+%H:%M:%S')"
    half=(); for p in "${rprobes[@]}"; do [ "${p%%:*}" = "$role" ] && half+=("${p#*:}"); done
    [ "$role" = host ] && ctx=server || ctx=client
    net_send "$sock" "$ctx" "$mark" "$tok" "$pu" "${half[@]}"
    sleep 2
    echo "--- $role half sent with mark '$mark', token '$tok' ---"
    grep -a "$mark\|SWEEP-NETR" "$sock/extender.log" | tail -40
    echo "--- the other box runs: tools/osi-sweep.sh --net-half $([ "$role" = host ] && echo join || echo host) $tok, and greps SWEEP-NETR for $tok- in its extender.log ---"
    exit 0
fi

# --net-pair [HOST_STATE [JOIN_STATE]] -- the whole round, both consoles, one verdict table.
# Defaults to the two instances of a one-box rig (tools/headless.sh --instance, not published).  Run it with both games in the lobby.
if [ "${1:-}" = --net-pair ]; then
    hstate=${2:-$HOME/.local/state/bg3-instance-host}
    jstate=${3:-$HOME/.local/state/bg3-instance-join}
    hsock=$(net_sock "$hstate") || { echo "no console socket under $hstate (is the host instance up?)" >&2; exit 2; }
    jsock=$(net_sock "$jstate") || { echo "no console socket under $jstate (is the join instance up?)" >&2; exit 2; }
    [ "$hsock" != "$jsock" ] || { echo "both roles resolve to the same console ($hsock): that is one game, not a pair" >&2; exit 2; }
    net_alive "$hsock" || { echo "nothing is listening on $hsock/console.sock -- is the host instance up?" >&2; exit 2; }
    net_alive "$jsock" || { echo "nothing is listening on $jsock/console.sock -- is the join instance up?" >&2; exit 2; }
    hlog=$hsock/extender.log; jlog=$jsock/extender.log
    # the remote peer's UserId, off the host's own handshake line unless given
    pu=${BG3SE_SWEEP_PEER_USER:-}
    if [ -z "$pu" ]; then
        # this launch only (the log appends), and never the host's own client: its UserId's
        # peer is 1, the local peer, and a remote peer is any other (a joiner reads 131072 =
        # UserId(2, 0)).
        launch=$(grep -an 'built on' "$hlog" 2>/dev/null | tail -1 | cut -d: -f1)
        pu=$(tail -n +"${launch:-1}" "$hlog" 2>/dev/null \
             | sed -n 's/.*support notification from user \([0-9]*\).*/\1/p' \
             | awk '{ if (int($1 / 65536) != 1) last = $1 } END { if (last != "") print last }')
        [ -n "$pu" ] || pu=0
    fi
    mark="NETR $(date '+%H:%M:%S')"; tok="netr-$(date '+%H%M%S')"
    echo "net-pair: host=$hsock join=$jsock peer user=$pu mark='$mark' token=$tok"
    [ "$pu" = 0 ] && echo "net-pair: no extender handshake in the host's log -- touser will say so rather than send"
    # the round, in array order, one console connection per run of entries with the same role
    batch=(); brole=
    flush() { [ ${#batch[@]} -gt 0 ] || return 0
        if [ "$brole" = host ]; then net_send "$hsock" server "$mark" "$tok" "$pu" "${batch[@]}"
        else net_send "$jsock" client "$mark" "$tok" "$pu" "${batch[@]}"; fi
        batch=(); }
    for p in "${rprobes[@]}"; do
        role=${p%%:*}
        [ "$role" = "$brole" ] || { flush; brole=$role; }
        batch+=("${p#*:}")
    done
    flush
    sleep 3      # the wire, then the receiving side's tick, then its Lua queue
    echo "--- the round ---"
    net_report "$hlog" "$jlog" "$mark" "$tok"
    exit
fi

if [ "${1:-}" = --list ]; then printf '%s\n' "${probes[@]}" "${cprobes[@]}" "${rprobes[@]}"; exit 0; fi
# --check parses every probe with the host's Lua before any game is involved.  A probe with a
# syntax error is NOT caught by the pcall the sweep wraps it in -- pcall guards the call, not the
# compile of the chunk that contains it -- so the whole line fails to compile, the Print never
# runs, and the probe is simply *absent* from the output with one bare parser message in
# extender.log to say so: one `end` too few reads as "the probe did not fire" for a whole
# launch.  Lua 5.4's grammar is close enough
# to the extender's for `end`-counting and bracket errors, which is what this catches.
if [ "${1:-}" = --check ]; then
    luac=$(command -v luac5.4 || command -v luac || true)
    [ -n "$luac" ] || { echo "no luac on PATH: cannot parse the probes" >&2; exit 2; }
    tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT; bad=0
    rstripped=(); for p in "${rprobes[@]}"; do rstripped+=("${p#*:}"); done
    for p in "${probes[@]}" "${cprobes[@]}" "${rstripped[@]}"; do
        name=${p%%:*}; expr=${p#*:}
        # the remote probes carry @TOK@/@PEERUSER@; fill them in with literals so the parse is
        # of the text that is actually sent
        expr=${expr//@TOK@/netr-000000}; expr=${expr//@PEERUSER@/131072}
        printf 'do local ok, r = pcall(function() return %s end); print(ok, r) end\n' "$expr" > "$tmp"
        if ! out=$("$luac" -p "$tmp" 2>&1); then bad=$((bad+1)); echo "$name: ${out#*: }" >&2; fi
    done
    echo "$(( ${#probes[@]} + ${#cprobes[@]} + ${#rprobes[@]} )) probes, $bad with a syntax error"
    [ "$bad" = 0 ]; exit
fi
[ -S "$state/console.sock" ] || { echo "no console socket at $state/console.sock (is the game running with the extender?)" >&2; exit 2; }
mark="SWEEP $(date '+%H:%M:%S')"
{
    printf 'server\n'; sleep 0.5
    printf 'Ext.Utils.Print("%s begin")\n' "$mark"; sleep 0.3
    for p in "${probes[@]}"; do
        name=${p%%:*}; expr=${p#*:}
        # one line: pcall the expression, print label + ok/err + tostring of the result
        printf 'do local ok, r = pcall(function() return %s end); Ext.Utils.Print("%s %s: " .. (ok and "ok " or "ERR ") .. tostring(r)) end\n' "$expr" "$mark" "$name"
        sleep 0.7
    done
    sleep 2.5   # let proc-timer's 1 s timer fire while the console is still attached
    printf 'Ext.Utils.Print("%s end")\n' "$mark"; sleep 0.5
    printf 'exit\n'
} | if command -v socat > /dev/null; then socat - "UNIX-CONNECT:$state/console.sock" > /dev/null; else nc -U "$state/console.sock" > /dev/null; fi
sleep 1
{
    printf 'client\n'; sleep 0.5
    for p in "${cprobes[@]}"; do
        name=${p%%:*}; expr=${p#*:}
        printf 'do local ok, r = pcall(function() return %s end); Ext.Utils.Print("%s %s: " .. (ok and "ok " or "ERR ") .. tostring(r)) end\n' "$expr" "$mark" "$name"
        sleep 0.7
    done
    printf 'exit\n'
} | if command -v socat > /dev/null; then socat - "UNIX-CONNECT:$state/console.sock" > /dev/null; else nc -U "$state/console.sock" > /dev/null; fi
sleep 1
echo "--- $log ---"
grep -a "$mark\|SWEEP-TIMER\|SWEEP-NET\|SWEEP-STATS" "$log" || echo "(no SWEEP lines in the log: the game may have died; check coredumpctl and bg3se.log)"
echo "--- $state/osiris.log: the PROC's timer ---"
grep -a 'Debug_LaunchWithInterval_bg3se_sweep' "$state/osiris.log" | tail -3 || true
