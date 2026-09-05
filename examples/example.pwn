#include <a_samp>
#include <pawn_tts>

// Callback triggered when server gamemode starts
public OnGameModeInit()
{
    print("=============================================");
    print("   PawnTTS Demonstration Gamemode Initialized");
    print("=============================================");

    // Pre-cache frequent phrases into RAM & disk so playback is 0ms instant
    TTS_Precache("Selamat datang di server!", "id");
    TTS_Precache("Waktunya Payday! Silakan cek saldo ATM Anda.", "id");
    TTS_Precache("Server akan restart dalam lima menit.", "id");
    TTS_Precache("Welcome to the server!", "en");

    return 1;
}

// When a player connects to the server
public OnPlayerConnect(playerid)
{
    // Personal 2D stereo welcome greeting
    TTS_Speak(playerid, "Selamat datang di server! Gunakan garis miring help untuk bantuan.", "id");
    return 1;
}

// When player types in chat, demonstrate proximity 3D voice!
public OnPlayerText(playerid, text[])
{
    // Voice originates from the player's 3D position in the game world
    TTS_SpeakFromPlayer(playerid, text, "id", 25.0);
    return 1;
}

// Callback when a TTS phrase has been generated
public OnTTSGenerated(const hash[], const text[], const voice[], bool:success)
{
    printf("[PawnTTS] Audio generated: \"%s\" (voice: %s, success: %d, hash: %s)", text, voice, success, hash);
    return 1;
}

