#include "natives.hpp"
#include <amx/amx.h>
#include <plugincommon.h>

typedef void (*logprintf_t)(const char* format, ...);
logprintf_t logprintf;
extern void *pAMXFunctions;

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
    logprintf = (logprintf_t)ppData[PLUGIN_DATA_LOGPRINTF];

    logprintf("-------------------------------------------------");
    logprintf(" PawnTTS v0.1.0 (Text-to-Speech Engine) Loaded");
    logprintf(" Author: roseeast");
    logprintf(" Clientless audio stream for SA-MP & open.mp");
    logprintf("-------------------------------------------------");

    return pawntts::initialize_pawn_tts();
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
    pawntts::shutdown_pawn_tts();
    if (logprintf) {
        logprintf("[PawnTTS] Plugin unloaded.");
    }
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx) {
    pawntts::register_amx(amx);
    return amx_Register(amx, pawntts::get_pawn_tts_natives(), -1);
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx) {
    pawntts::unregister_amx(amx);
    return AMX_ERR_NONE;
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick() {
    pawntts::process_tick();
}
