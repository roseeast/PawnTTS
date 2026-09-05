#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <core.hpp>

#include "../../src/natives.hpp"

extern void* pAMXFunctions;

namespace {

constexpr UID kPawnTTSComponentUID = UID(0x5061776e54545331ULL); // "PawnTTS1"

class PawnTTSComponent final
    : public IComponent
    , public PawnEventHandler
    , public CoreEventHandler
{
public:
    PROVIDE_UID(kPawnTTSComponentUID)

    StringView componentName() const override
    {
        return "pawn_tts";
    }

    SemanticVersion componentVersion() const override
    {
        return SemanticVersion(0, 1, 0, 0);
    }

    void onLoad(ICore* c) override
    {
        core_ = c;
        if (core_ != nullptr) {
            core_->printLn("[PawnTTS] component loaded (native open.mp component mode)");
            core_->getEventDispatcher().addEventHandler(this);
        }
        pawntts::initialize_pawn_tts();
    }

    void onInit(IComponentList* components) override
    {
        pawn_ = components->queryComponent<IPawnComponent>();
        if (pawn_ != nullptr) {
            pAMXFunctions = const_cast<void*>(static_cast<const void*>(pawn_->getAmxFunctions().data()));
            pawn_->getEventDispatcher().addEventHandler(this);
        } else if (core_ != nullptr) {
            core_->logLn(LogLevel::Warning, "[PawnTTS] Pawn component not available; TTS_* natives cannot be registered");
        }
    }

    void onReady() override
    {
    }

    void onFree(IComponent* component) override
    {
        if (component == pawn_) {
            pawn_ = nullptr;
        }
    }

    void free() override
    {
        pawntts::shutdown_pawn_tts();

        if (core_ != nullptr) {
            core_->getEventDispatcher().removeEventHandler(this);
            core_->printLn("[PawnTTS] component unloaded");
            core_ = nullptr;
        }

        if (pawn_ != nullptr) {
            pawn_->getEventDispatcher().removeEventHandler(this);
            pawn_ = nullptr;
        }

        pAMXFunctions = nullptr;
    }

    void reset() override
    {
    }

    // --- CoreEventHandler ---
    void onTick(Microseconds /*elapsed*/, TimePoint /*now*/) override
    {
        pawntts::process_tick();
    }

    // --- PawnEventHandler ---
    void onAmxLoad(IPawnScript& script) override
    {
        AMX* amx = script.GetAMX();
        if (amx != nullptr) {
            pawntts::register_amx(amx);
            amx_Register(amx, pawntts::get_pawn_tts_natives(), -1);
        }
    }

    void onAmxUnload(IPawnScript& script) override
    {
        AMX* amx = script.GetAMX();
        if (amx != nullptr) {
            pawntts::unregister_amx(amx);
        }
    }

private:
    ICore* core_ = nullptr;
    IPawnComponent* pawn_ = nullptr;
};

PawnTTSComponent g_component;

} // namespace

COMPONENT_ENTRY_POINT()
{
    return &g_component;
}

