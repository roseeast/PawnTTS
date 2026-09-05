#include <a_samp>
#include <pawn_tts>

// ============================================================================
// Real-world PawnTTS Commands Showcase (/s, /m, /gps, /payday, /atm, /stopaudio)
// ============================================================================

// /s [text] - Shout out loud (3D voice from player position, 25 meters radius)
public OnPlayerCommandText(playerid, cmdtext[])
{
    if (!strcmp(cmdtext, "/s ", true, 3))
    {
        new text[128];
        format(text, sizeof(text), "%s", cmdtext[3]);
        TTS_SpeakFromPlayer(playerid, text, "id", 25.0);
        return 1;
    }

    // /m [text] - Police / Emergency Megaphone (60 meters radius)
    if (!strcmp(cmdtext, "/m ", true, 3))
    {
        new text[128];
        format(text, sizeof(text), "%s", cmdtext[3]);
        TTS_SpeakFromPlayer(playerid, text, "id", 60.0);
        return 1;
    }

    // /gps - Spoken GPS navigation prompt
    if (!strcmp(cmdtext, "/gps", true))
    {
        TTS_Speak(playerid, "Rute telah diperbarui. Dalam dua ratus meter, belok ke kanan.", "id");
        return 1;
    }

    // /payday - Server-wide broadcast announcement
    if (!strcmp(cmdtext, "/payday", true))
    {
        TTS_SpeakToAll("Perhatian warga. Gaji Payday telah ditransfer ke rekening bank Anda masing-masing.", "id");
        return 1;
    }

    // /atm - ATM voice interaction
    if (!strcmp(cmdtext, "/atm", true))
    {
        TTS_Speak(playerid, "PIN yang Anda masukkan salah. Silakan coba kembali.", "id");
        return 1;
    }

    // /stopaudio - Stop any ongoing voice audio stream
    if (!strcmp(cmdtext, "/stopaudio", true))
    {
        TTS_Stop(playerid);
        SendClientMessage(playerid, -1, "Audio stream dihentikan.");
        return 1;
    }

    return 0;
}

