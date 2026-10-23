-- bg3se-linux: the client half of osi-sweep.sh's net-* probes, overlaid on upstream's
-- SampleMod by tools/install-test-mod.sh (tools/test-mod-overlay/). The server half is
-- created from the console by the sweep (net-channel), so nothing of upstream's mod changes.
-- One server-side RequestToClient exercises all four legs of the in-process path: the
-- server's request reaches this handler (client tick), its reply goes back (server tick),
-- and the handler fires the reverse request, whose reply lands here. Every leg prints a
-- SWEEP-NET line to extender.log; the sweep greps for them.
local ch
ch = Ext.Net.CreateChannel(ModuleUUID, "bg3se_sweep",
    function (msg, user)
        Ext.Utils.Print("SWEEP-NET client message text=" .. tostring(msg.text) .. " from " .. tostring(user))
    end,
    function (req, user)
        Ext.Utils.Print("SWEEP-NET client request ping=" .. tostring(req.ping) .. " from " .. tostring(user))
        ch:RequestToServer({ ping = req.ping + 100 }, function (reply)
            Ext.Utils.Print("SWEEP-NET client got reply pong=" .. tostring(reply.pong))
        end)
        return { pong = req.ping + 1 }
    end)
Ext.Utils.Print("SWEEP-NET client channel registered, Ext.Net.Version=" .. tostring(Ext.Net.Version()) .. " IsHost=" .. tostring(Ext.Net.IsHost()))

-- The legacy (module-less) path: Ext.Net.BroadcastMessage(channel, payload) with no module
-- arrives as the deprecated NetMessage event; the sweep's net-legacy probe sends one.
Ext.RegisterNetListener("bg3se_sweep_legacy", function (channel, payload, user)
    Ext.Utils.Print("SWEEP-NET client legacy channel=" .. tostring(channel) .. " payload=" .. tostring(payload) .. " from " .. tostring(user))
end)

-- Which client game states Lua is told about, by the name the extender's ecl::GameState
-- enum gives the value: the witness for that enum against this build (a state the game is
-- in that never shows up here is a member the enum lacks).
Ext.Events.GameStateChanged:Subscribe(function (e)
    Ext.Utils.Print("SWEEP-NET client GameStateChanged " .. tostring(e.FromState) .. " -> " .. tostring(e.ToState))
end)

-- The witness for RPGStats__ParseDataBuffers 0x27ddb00: upstream fires
-- StatsStructureLoaded from that pre-hook and nowhere else, during module load -- long before
-- a console exists -- so the subscription has to be in a bootstrap and the reading has to be a
-- log line. The count goes into _G as well, for the sweep's stats-* client probes, but only the
-- SWEEP-STATS lines survive a reset of the Load-context state.
_G.bg3se_sweep_stats = { structure = 0, at = {} }
Ext.Events.StatsStructureLoaded:Subscribe(function ()
    local s = _G.bg3se_sweep_stats
    s.structure = s.structure + 1
    s.at[#s.at + 1] = Ext.Utils.MonotonicTime()
    local weapons = -1
    local ok, r = pcall(function() return #Ext.Stats.GetStats("Weapon") end)
    if ok then weapons = r end
    Ext.Utils.Print("SWEEP-STATS client StatsStructureLoaded #" .. tostring(s.structure)
        .. " Weapon=" .. tostring(weapons))
end)
