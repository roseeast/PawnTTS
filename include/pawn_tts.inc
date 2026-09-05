#if defined _pawn_tts_included
    #endinput
#endif
#define _pawn_tts_included

/**
 * ============================================================================
 *  PawnTTS - High-Performance Text-to-Speech Engine for SA-MP and open.mp
 *  Author: roseeast
 *  Version: 0.1.0
 *
 *  Clientless in-game voice streaming supporting PC and Android clients.
 *  Uses GTA San Andreas built-in BASS audio engine via PlayAudioStreamForPlayer.
 * ============================================================================
 */

// Core Lifecycle & Status
native bool:TTS_IsReady();
native TTS_GetBaseURL(output[], maxlen = sizeof(output));
native TTS_SetPublicURL(const url[]);

// Caching
native bool:TTS_Precache(const text[], const voice[] = "id", Float:speed = 1.0);
native bool:TTS_IsCached(const text[], const voice[] = "id", Float:speed = 1.0);
native TTS_ClearCache(bool:clear_disk = false);
native TTS_GetStats(&cached_mem, &cached_disk, &total_bytes_kb);

// Speech Playback
native bool:TTS_Speak(playerid, const text[], const voice[] = "id", Float:speed = 1.0);
native bool:TTS_SpeakToAll(const text[], const voice[] = "id", Float:speed = 1.0);
native bool:TTS_SpeakAtPos(const text[], const voice[] = "id", Float:x, Float:y, Float:z, Float:distance = 30.0, Float:speed = 1.0);
native bool:TTS_SpeakAtPosForPlayer(playerid, const text[], const voice[] = "id", Float:x, Float:y, Float:z, Float:distance = 30.0, Float:speed = 1.0);

// Events
forward OnTTSGenerated(const hash[], const text[], const voice[], bool:success);
forward OnTTSReady(playerid, const hash[], const url[], Float:x, Float:y, Float:z, Float:distance, usepos);

/**
 * Speak in 3D audio originating from a specific player's position.
 * Nearby players will hear the speech coming from the speaker's body.
 */
stock bool:TTS_SpeakFromPlayer(speakerid, const text[], const voice[] = "id", Float:distance = 25.0, Float:speed = 1.0)
{
    if (!IsPlayerConnected(speakerid)) return false;
    new Float:x, Float:y, Float:z;
    GetPlayerPos(speakerid, x, y, z);
    return TTS_SpeakAtPos(text, voice, x, y, z, distance, speed);
}

/**
 * Stop any active audio stream for a specific player.
 */
stock TTS_Stop(playerid)
{
    if (IsPlayerConnected(playerid)) {
        StopAudioStreamForPlayer(playerid);
    }
}

/**
 * Stop any active audio stream for all connected players.
 */
stock TTS_StopAll()
{
    #if defined foreach
    foreach (new i : Player)
    #else
    for (new i = 0, j = GetPlayerPoolSize(); i <= j; i++)
    #endif
    {
        if (IsPlayerConnected(i)) {
            StopAudioStreamForPlayer(i);
        }
    }
}

/**
 * Default playback dispatcher.
 * Intercepts OnTTSReady and triggers PlayAudioStreamForPlayer automatically.
 */
public OnTTSReady(playerid, const hash[], const url[], Float:x, Float:y, Float:z, Float:distance, usepos)
{
    if (playerid == -1) {
        // Broadcast to all connected players
        #if defined foreach
        foreach (new i : Player)
        #else
        for (new i = 0, j = GetPlayerPoolSize(); i <= j; i++)
        #endif
        {
            if (IsPlayerConnected(i)) {
                if (usepos) {
                    PlayAudioStreamForPlayer(i, url, x, y, z, distance, 1);
                } else {
                    PlayAudioStreamForPlayer(i, url);
                }
            }
        }
    } else if (IsPlayerConnected(playerid)) {
        if (usepos) {
            PlayAudioStreamForPlayer(playerid, url, x, y, z, distance, 1);
        } else {
            PlayAudioStreamForPlayer(playerid, url);
        }
    }

    #if defined TTS_OnTTSReady
        return TTS_OnTTSReady(playerid, hash, url, x, y, z, distance, usepos);
    #else
        return 1;
    #endif
}

#if defined _ALS_OnTTSReady
    #undef OnTTSReady
#else
    #define _ALS_OnTTSReady
#endif
#define OnTTSReady TTS_OnTTSReady
#if defined TTS_OnTTSReady
    forward TTS_OnTTSReady(playerid, const hash[], const url[], Float:x, Float:y, Float:z, Float:distance, usepos);
#endif

