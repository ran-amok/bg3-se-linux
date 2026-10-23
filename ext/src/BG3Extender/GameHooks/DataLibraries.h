#pragma once

#include <GameDefinitions/Character.h>
#include <GameDefinitions/Item.h>
#include <GameDefinitions/Misc.h>
#include <GameDefinitions/Osiris.h>
#include <GameDefinitions/Symbols.h>
#include <CoreLib/SymbolMapper.h>   // PORT (linux): types only; SymbolMapper.cpp (PE pattern scanning) is not built
#include <CoreLib/Wrappers.h>
#include <optional>

namespace bg3se {

    struct GameVersionInfo
    {
        uint16_t Major{ 0 }, Minor{ 0 }, Revision{ 0 }, Build{ 0 };

        inline bool IsSupported() const
        {
            // We need v4.69 or later for game
            return (Major == 4 && ((Minor == 69 && Revision >= 95) || (Minor > 69)));
        }
    };

    class LibraryManager
    {
    public:
        LibraryManager();
        bool FindLibraries(uint32_t gameRevision);
        bool PostStartupFindLibraries();
        void ApplyCodePatches();
        bool GetGameVersion(GameVersionInfo & version);
        HMODULE GetAppHandle();

        void ShowStartupError(STDString const & msg, bool wait, bool exitGame);
        void ShowStartupError(STDString const & msg, bool exitGame);
        // PORT (linux): names the symbols behind an InitializationFailed() dialog, in the log
        void LogMissingSymbols();

        bool WasPatchApplied(std::string const& mapping);
        bool ApplyCodePatch(std::string const& mapping);
        bool UndoCodePatch(std::string const& mapping);

        inline bool CriticalInitializationFailed() const
        {
            return CriticalInitFailed;
        }

        inline bool InitializationFailed() const
        {
            return InitFailed;
        }

        inline bool IsDX11() const
        {
            return DX11;
        }

        inline std::vector<std::string> const& MissingSymbols() const
        {
            return missingSymbols_;
        }

    private:
        void RegisterSymbols();

        bool CanShowError();
        bool CanShowMessages();


        // PORT (linux): addresses come from the loader's offset manifest by upstream's
        // symbol names (GameHooks/DataLibraries.cpp), not from byte patterns
        std::unordered_map<std::string, int> symbolOffsets_;   // StaticSymbols member offsets by name
        std::vector<std::string> missingSymbols_;

        bool InitFailed{ false };
        bool CriticalInitFailed{ false };
        bool PostLoaded{ false };
        bool EnabledCustomStats{ false };
        bool DX11{ false };
    };
}
