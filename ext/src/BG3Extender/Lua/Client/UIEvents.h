#pragma once

#include <NsGui/UIElement.h>
// PORT (linux): DeferredUIEvents names Noesis::BaseCommand below, and nothing above declares
// it. Upstream gets away with it because every Windows TU that reaches this header has already
// been through GameDefinitions/UI.h (which does include it) by way of
// Proxies/PropertyMapDependencies.h; the TUs that include Lua/Client/LuaBindingClient.h
// directly have not, and 35 of them fail here once the Noesis fence is down.
#include <NsGui/BaseCommand.h>
#include <Lua/Shared/LuaReference.h>

BEGIN_NS(ecl::lua)

using namespace Noesis;
using namespace bg3se::lua;

class UIEventHooks
{
public:
    using SubscriptionIndex = uint32_t;

    UIEventHooks(ClientState& state);
    ~UIEventHooks();

    SubscriptionIndex Subscribe(UIElement* target, RoutedEvent const* event, bg3se::FixedString const& eventName, RegistryEntry&& hook);
    bool Unsubscribe(SubscriptionIndex index);
    void EventFired(SubscriptionIndex index, Noesis::BaseComponent* target, const RoutedEventArgs& args);

private:
    struct EventHandler
    {
        inline EventHandler() {}
        ~EventHandler();

        EventHandler(EventHandler const&) = delete;
        EventHandler(EventHandler &&) noexcept = default;
        EventHandler& operator = (EventHandler const&) = delete;
        EventHandler& operator = (EventHandler&&) noexcept = default;

        void Unsubscribe();

        Ptr<UIElement> Target;
        bg3se::FixedString Event;
        RoutedEvent const* EventType{ nullptr };
        RegistryEntry Handler;
        SubscriptionIndex Index{ 0 };
        bool IsActive{ false };
    };

    ClientState& state_;
    SaltedPool<EventHandler> subscriptions_;
};

class DeferredUIEvents
{
public:
    DeferredUIEvents(ClientState& state);

    void PostUpdate();

    void OnCommand(lua::PersistentRegistryEntry const& handler, Noesis::BaseCommand* command, Noesis::BaseComponent* parameter);
    void OnPropertyChanged(lua::PersistentRegistryEntry const& handler, Noesis::BaseComponent* object, Noesis::Symbol property);

private:
    struct DeferredCommand
    {
        lua::PersistentRegistryEntry Handler;
        Ptr<Noesis::BaseCommand> Command;
        Ptr<Noesis::BaseComponent> Parameter;
    };
    
    struct DeferredPropertyChange
    {
        lua::PersistentRegistryEntry Handler;
        Ptr<Noesis::BaseComponent> Object;
        Noesis::Symbol Property;
    };

    ClientState& state_;
    Array<DeferredCommand> commands_;
    Array<DeferredPropertyChange> propertyChanges_;
};

END_SE()
