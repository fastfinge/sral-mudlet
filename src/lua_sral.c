/*
 * lua_sral.c - Lua 5.1 C module for SRAL (Screen Reader Abstraction Library)
 *
 * This module dynamically loads SRAL.dll at runtime and exposes its functions
 * to Lua. It does NOT require LuaJIT FFI.
 *
 * Build (MinGW cross-compile):
 *   x86_64-w64-mingw32-gcc -shared -o sral_bridge.dll lua_sral.c -I/usr/include/lua5.1 -L. -llua51 -O2 -Wall
 *
 * Usage in Mudlet:
 *   Place sral_bridge.dll (this file) and SRAL.dll in your package directory.
 *   local sral_c = require("sral_bridge")
 *   sral_c.load("C:\\path\\to\\SRAL.dll")
 *   sral_c.initialize(0)
 *   sral_c.speak("Hello", true)
 */

#include <windows.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* SRAL engine constants (must match SRAL.h) */
#define SRAL_ENGINE_NONE              0
#define SRAL_ENGINE_NVDA              (1 << 1)
#define SRAL_ENGINE_JAWS              (1 << 2)
#define SRAL_ENGINE_ZDSR              (1 << 3)
#define SRAL_ENGINE_NARRATOR          (1 << 4)
#define SRAL_ENGINE_UIA               (1 << 5)
#define SRAL_ENGINE_SAPI              (1 << 6)
#define SRAL_ENGINE_SPEECH_DISPATCHER (1 << 7)
#define SRAL_ENGINE_NS_SPEECH         (1 << 8)
#define SRAL_ENGINE_VOICE_OVER        (1 << 9)
#define SRAL_ENGINE_AV_SPEECH         (1 << 10)

/* Function pointer types matching SRAL's C API */
typedef bool (*fn_SRAL_Initialize)(int);
typedef void (*fn_SRAL_Uninitialize)(void);
typedef bool (*fn_SRAL_IsInitialized)(void);
typedef bool (*fn_SRAL_Speak)(const char*, bool);
typedef bool (*fn_SRAL_Braille)(const char*);
typedef bool (*fn_SRAL_Output)(const char*, bool);
typedef bool (*fn_SRAL_StopSpeech)(void);
typedef bool (*fn_SRAL_PauseSpeech)(void);
typedef bool (*fn_SRAL_ResumeSpeech)(void);
typedef bool (*fn_SRAL_IsSpeaking)(void);
typedef int  (*fn_SRAL_GetCurrentEngine)(void);
typedef int  (*fn_SRAL_GetEngineFeatures)(int);
typedef int  (*fn_SRAL_GetAvailableEngines)(void);
typedef int  (*fn_SRAL_GetActiveEngines)(void);
typedef const char* (*fn_SRAL_GetEngineName)(int);
typedef bool (*fn_SRAL_SpeakEx)(int, const char*, bool);
typedef bool (*fn_SRAL_BrailleEx)(int, const char*);
typedef bool (*fn_SRAL_OutputEx)(int, const char*, bool);
typedef bool (*fn_SRAL_StopSpeechEx)(int);
typedef bool (*fn_SRAL_RegisterKeyboardHooks)(void);
typedef void (*fn_SRAL_UnregisterKeyboardHooks)(void);
typedef void (*fn_SRAL_Delay)(int);
typedef bool (*fn_SRAL_SetEnginesExclude)(int);
typedef int  (*fn_SRAL_GetEnginesExclude)(void);
typedef bool (*fn_SRAL_SetEngineParameter)(int, int, const void*);
typedef bool (*fn_SRAL_GetEngineParameter)(int, int, void*);
typedef bool (*fn_SRAL_IsSpeakingEx)(int);
typedef bool (*fn_SRAL_PauseSpeechEx)(int);
typedef bool (*fn_SRAL_ResumeSpeechEx)(int);
typedef bool (*fn_SRAL_SpeakSsml)(const char*, bool);
typedef bool (*fn_SRAL_SpeakSsmlEx)(int, const char*, bool);

/* Loaded function pointers */
static HMODULE hSRAL = NULL;
static fn_SRAL_Initialize          pInitialize = NULL;
static fn_SRAL_Uninitialize        pUninitialize = NULL;
static fn_SRAL_IsInitialized       pIsInitialized = NULL;
static fn_SRAL_Speak               pSpeak = NULL;
static fn_SRAL_Braille             pBraille = NULL;
static fn_SRAL_Output              pOutput = NULL;
static fn_SRAL_StopSpeech          pStopSpeech = NULL;
static fn_SRAL_PauseSpeech         pPauseSpeech = NULL;
static fn_SRAL_ResumeSpeech        pResumeSpeech = NULL;
static fn_SRAL_IsSpeaking          pIsSpeaking = NULL;
static fn_SRAL_GetCurrentEngine    pGetCurrentEngine = NULL;
static fn_SRAL_GetEngineFeatures   pGetEngineFeatures = NULL;
static fn_SRAL_GetAvailableEngines pGetAvailableEngines = NULL;
static fn_SRAL_GetActiveEngines    pGetActiveEngines = NULL;
static fn_SRAL_GetEngineName       pGetEngineName = NULL;
static fn_SRAL_SpeakEx             pSpeakEx = NULL;
static fn_SRAL_BrailleEx           pBrailleEx = NULL;
static fn_SRAL_OutputEx            pOutputEx = NULL;
static fn_SRAL_StopSpeechEx        pStopSpeechEx = NULL;
static fn_SRAL_RegisterKeyboardHooks   pRegisterKeyboardHooks = NULL;
static fn_SRAL_UnregisterKeyboardHooks pUnregisterKeyboardHooks = NULL;
static fn_SRAL_Delay               pDelay = NULL;
static fn_SRAL_SetEnginesExclude   pSetEnginesExclude = NULL;
static fn_SRAL_GetEnginesExclude   pGetEnginesExclude = NULL;
static fn_SRAL_SetEngineParameter  pSetEngineParameter = NULL;
static fn_SRAL_GetEngineParameter  pGetEngineParameter = NULL;
static fn_SRAL_IsSpeakingEx        pIsSpeakingEx = NULL;
static fn_SRAL_PauseSpeechEx       pPauseSpeechEx = NULL;
static fn_SRAL_ResumeSpeechEx      pResumeSpeechEx = NULL;
static fn_SRAL_SpeakSsml           pSpeakSsml = NULL;
static fn_SRAL_SpeakSsmlEx         pSpeakSsmlEx = NULL;

/* Helper to load a function from the DLL (not fatal if missing) */
#define LOAD_FUNC(name) \
    p##name = (fn_SRAL_##name)GetProcAddress(hSRAL, "SRAL_" #name);

/*
 * Local engine name lookup — used as fallback when SRAL_GetEngineName
 * is not exported (e.g. SRAL 0.3 Stable).
 */
static const char* local_get_engine_name(int engine) {
    switch (engine) {
        case SRAL_ENGINE_NONE:              return "None";
        case SRAL_ENGINE_NVDA:              return "NVDA";
        case SRAL_ENGINE_JAWS:              return "JAWS";
        case SRAL_ENGINE_ZDSR:              return "ZDSR";
        case SRAL_ENGINE_NARRATOR:          return "Narrator";
        case SRAL_ENGINE_UIA:               return "UIA";
        case SRAL_ENGINE_SAPI:              return "SAPI";
        case SRAL_ENGINE_SPEECH_DISPATCHER: return "Speech Dispatcher";
        case SRAL_ENGINE_NS_SPEECH:         return "NSSpeech";
        case SRAL_ENGINE_VOICE_OVER:        return "VoiceOver";
        case SRAL_ENGINE_AV_SPEECH:         return "AVSpeech";
        default:                            return "Unknown";
    }
}

/*
 * Extract directory from a file path.
 * Writes the directory (without trailing slash) into buf.
 * Returns buf, or NULL if no directory separator found.
 */
static char* get_directory(const char *path, char *buf, size_t bufsize) {
    const char *last_sep = NULL;
    const char *p;
    for (p = path; *p; p++) {
        if (*p == '\\' || *p == '/') last_sep = p;
    }
    if (!last_sep) return NULL;
    size_t len = (size_t)(last_sep - path);
    if (len >= bufsize) len = bufsize - 1;
    memcpy(buf, path, len);
    buf[len] = '\0';
    return buf;
}

/* sral_bridge.load(path) - Load SRAL.dll from the given path
 *
 * Before loading, calls SetDllDirectoryA to add the DLL's directory
 * to the Windows DLL search path. This ensures that SRAL.dll's own
 * dependencies (e.g. nvdaControllerClient.dll) can be found if they
 * are placed alongside it.
 *
 * Returns true on success, nil + error message on failure */
static int l_load(lua_State *L) {
    const char *path = luaL_optstring(L, 1, "SRAL.dll");

    if (hSRAL) {
        /* Already loaded */
        lua_pushboolean(L, 1);
        return 1;
    }

    /*
     * Add the directory containing SRAL.dll to the DLL search path.
     * This is critical: SRAL.dll internally loads screen reader client
     * DLLs (like nvdaControllerClient.dll, jfwapi.dll) using LoadLibrary
     * without a full path. By calling SetDllDirectoryA, we ensure Windows
     * searches the package directory for these dependencies.
     */
    char dirBuf[MAX_PATH];
    if (get_directory(path, dirBuf, sizeof(dirBuf))) {
        SetDllDirectoryA(dirBuf);
    }

    hSRAL = LoadLibraryA(path);
    if (!hSRAL) {
        DWORD err = GetLastError();
        lua_pushnil(L);
        lua_pushfstring(L, "Failed to load %s (error %d)", path, (int)err);
        return 2;
    }

    /* Load all function pointers — missing ones stay NULL */
    LOAD_FUNC(Initialize);
    LOAD_FUNC(Uninitialize);
    LOAD_FUNC(IsInitialized);
    LOAD_FUNC(Speak);
    LOAD_FUNC(Braille);
    LOAD_FUNC(Output);
    LOAD_FUNC(StopSpeech);
    LOAD_FUNC(PauseSpeech);
    LOAD_FUNC(ResumeSpeech);
    LOAD_FUNC(IsSpeaking);
    LOAD_FUNC(GetCurrentEngine);
    LOAD_FUNC(GetEngineFeatures);
    LOAD_FUNC(GetAvailableEngines);
    LOAD_FUNC(GetActiveEngines);
    LOAD_FUNC(GetEngineName);
    LOAD_FUNC(SpeakEx);
    LOAD_FUNC(BrailleEx);
    LOAD_FUNC(OutputEx);
    LOAD_FUNC(StopSpeechEx);
    LOAD_FUNC(RegisterKeyboardHooks);
    LOAD_FUNC(UnregisterKeyboardHooks);
    LOAD_FUNC(Delay);
    LOAD_FUNC(SetEnginesExclude);
    LOAD_FUNC(GetEnginesExclude);
    LOAD_FUNC(SetEngineParameter);
    LOAD_FUNC(GetEngineParameter);
    LOAD_FUNC(IsSpeakingEx);
    LOAD_FUNC(PauseSpeechEx);
    LOAD_FUNC(ResumeSpeechEx);
    LOAD_FUNC(SpeakSsml);
    LOAD_FUNC(SpeakSsmlEx);

    if (!pInitialize || !pSpeak || !pOutput) {
        FreeLibrary(hSRAL);
        hSRAL = NULL;
        lua_pushnil(L);
        lua_pushstring(L, "SRAL.dll loaded but critical functions not found");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* sral_bridge.unload() - Unload SRAL.dll */
static int l_unload(lua_State *L) {
    if (hSRAL) {
        if (pIsInitialized && pIsInitialized()) {
            if (pUninitialize) pUninitialize();
        }
        FreeLibrary(hSRAL);
        hSRAL = NULL;
        /* Restore default DLL search path */
        SetDllDirectoryA(NULL);
    }
    return 0;
}

/* sral_bridge.is_loaded() */
static int l_is_loaded(lua_State *L) {
    lua_pushboolean(L, hSRAL != NULL);
    return 1;
}

/* sral_bridge.initialize(engines_exclude) */
static int l_initialize(lua_State *L) {
    if (!pInitialize) { lua_pushboolean(L, 0); return 1; }
    int exclude = (int)luaL_optinteger(L, 1, 0);
    lua_pushboolean(L, pInitialize(exclude));
    return 1;
}

/* sral_bridge.uninitialize() */
static int l_uninitialize(lua_State *L) {
    if (pUninitialize) pUninitialize();
    return 0;
}

/* sral_bridge.is_initialized() */
static int l_is_initialized(lua_State *L) {
    lua_pushboolean(L, pIsInitialized ? pIsInitialized() : 0);
    return 1;
}

/* sral_bridge.speak(text, interrupt) */
static int l_speak(lua_State *L) {
    if (!pSpeak) { lua_pushboolean(L, 0); return 1; }
    const char *text = luaL_checkstring(L, 1);
    bool interrupt = lua_toboolean(L, 2);
    lua_pushboolean(L, pSpeak(text, interrupt));
    return 1;
}

/* sral_bridge.braille(text) */
static int l_braille(lua_State *L) {
    if (!pBraille) { lua_pushboolean(L, 0); return 1; }
    const char *text = luaL_checkstring(L, 1);
    lua_pushboolean(L, pBraille(text));
    return 1;
}

/* sral_bridge.output(text, interrupt) */
static int l_output(lua_State *L) {
    if (!pOutput) { lua_pushboolean(L, 0); return 1; }
    const char *text = luaL_checkstring(L, 1);
    bool interrupt = lua_toboolean(L, 2);
    lua_pushboolean(L, pOutput(text, interrupt));
    return 1;
}

/* sral_bridge.stop_speech() */
static int l_stop_speech(lua_State *L) {
    lua_pushboolean(L, pStopSpeech ? pStopSpeech() : 0);
    return 1;
}

/* sral_bridge.pause_speech() */
static int l_pause_speech(lua_State *L) {
    lua_pushboolean(L, pPauseSpeech ? pPauseSpeech() : 0);
    return 1;
}

/* sral_bridge.resume_speech() */
static int l_resume_speech(lua_State *L) {
    lua_pushboolean(L, pResumeSpeech ? pResumeSpeech() : 0);
    return 1;
}

/* sral_bridge.is_speaking() */
static int l_is_speaking(lua_State *L) {
    lua_pushboolean(L, pIsSpeaking ? pIsSpeaking() : 0);
    return 1;
}

/* sral_bridge.get_current_engine() */
static int l_get_current_engine(lua_State *L) {
    lua_pushinteger(L, pGetCurrentEngine ? pGetCurrentEngine() : 0);
    return 1;
}

/* sral_bridge.get_engine_features(engine) */
static int l_get_engine_features(lua_State *L) {
    int engine = (int)luaL_optinteger(L, 1, 0);
    lua_pushinteger(L, pGetEngineFeatures ? pGetEngineFeatures(engine) : 0);
    return 1;
}

/* sral_bridge.get_available_engines() */
static int l_get_available_engines(lua_State *L) {
    lua_pushinteger(L, pGetAvailableEngines ? pGetAvailableEngines() : 0);
    return 1;
}

/* sral_bridge.get_active_engines() */
static int l_get_active_engines(lua_State *L) {
    lua_pushinteger(L, pGetActiveEngines ? pGetActiveEngines() : 0);
    return 1;
}

/* sral_bridge.get_engine_name(engine)
 * Uses SRAL_GetEngineName if available, otherwise falls back to local lookup */
static int l_get_engine_name(lua_State *L) {
    int engine = (int)luaL_checkinteger(L, 1);
    const char *name = NULL;

    if (pGetEngineName) {
        name = pGetEngineName(engine);
    }

    /* Fallback to our local implementation if SRAL doesn't export the
     * function (e.g. SRAL 0.3) or if it returned NULL */
    if (!name) {
        name = local_get_engine_name(engine);
    }

    lua_pushstring(L, name);
    return 1;
}

/* sral_bridge.speak_ex(engine, text, interrupt) */
static int l_speak_ex(lua_State *L) {
    if (!pSpeakEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    const char *text = luaL_checkstring(L, 2);
    bool interrupt = lua_toboolean(L, 3);
    lua_pushboolean(L, pSpeakEx(engine, text, interrupt));
    return 1;
}

/* sral_bridge.braille_ex(engine, text) */
static int l_braille_ex(lua_State *L) {
    if (!pBrailleEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    const char *text = luaL_checkstring(L, 2);
    lua_pushboolean(L, pBrailleEx(engine, text));
    return 1;
}

/* sral_bridge.output_ex(engine, text, interrupt) */
static int l_output_ex(lua_State *L) {
    if (!pOutputEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    const char *text = luaL_checkstring(L, 2);
    bool interrupt = lua_toboolean(L, 3);
    lua_pushboolean(L, pOutputEx(engine, text, interrupt));
    return 1;
}

/* sral_bridge.stop_speech_ex(engine) */
static int l_stop_speech_ex(lua_State *L) {
    if (!pStopSpeechEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, pStopSpeechEx(engine));
    return 1;
}

/* sral_bridge.register_keyboard_hooks() */
static int l_register_keyboard_hooks(lua_State *L) {
    lua_pushboolean(L, pRegisterKeyboardHooks ? pRegisterKeyboardHooks() : 0);
    return 1;
}

/* sral_bridge.unregister_keyboard_hooks() */
static int l_unregister_keyboard_hooks(lua_State *L) {
    if (pUnregisterKeyboardHooks) pUnregisterKeyboardHooks();
    return 0;
}

/* sral_bridge.delay(ms) */
static int l_delay(lua_State *L) {
    if (pDelay) {
        int ms = (int)luaL_checkinteger(L, 1);
        pDelay(ms);
    }
    return 0;
}

/* sral_bridge.set_engines_exclude(bitmask) */
static int l_set_engines_exclude(lua_State *L) {
    if (!pSetEnginesExclude) { lua_pushboolean(L, 0); return 1; }
    int mask = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, pSetEnginesExclude(mask));
    return 1;
}

/* sral_bridge.get_engines_exclude() */
static int l_get_engines_exclude(lua_State *L) {
    lua_pushinteger(L, pGetEnginesExclude ? pGetEnginesExclude() : -1);
    return 1;
}

/* sral_bridge.speak_ssml(ssml, interrupt) */
static int l_speak_ssml(lua_State *L) {
    if (!pSpeakSsml) { lua_pushboolean(L, 0); return 1; }
    const char *ssml = luaL_checkstring(L, 1);
    bool interrupt = lua_toboolean(L, 2);
    lua_pushboolean(L, pSpeakSsml(ssml, interrupt));
    return 1;
}

/* sral_bridge.speak_ssml_ex(engine, ssml, interrupt) */
static int l_speak_ssml_ex(lua_State *L) {
    if (!pSpeakSsmlEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    const char *ssml = luaL_checkstring(L, 2);
    bool interrupt = lua_toboolean(L, 3);
    lua_pushboolean(L, pSpeakSsmlEx(engine, ssml, interrupt));
    return 1;
}

/* sral_bridge.pause_speech_ex(engine) */
static int l_pause_speech_ex(lua_State *L) {
    if (!pPauseSpeechEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, pPauseSpeechEx(engine));
    return 1;
}

/* sral_bridge.resume_speech_ex(engine) */
static int l_resume_speech_ex(lua_State *L) {
    if (!pResumeSpeechEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, pResumeSpeechEx(engine));
    return 1;
}

/* sral_bridge.is_speaking_ex(engine) */
static int l_is_speaking_ex(lua_State *L) {
    if (!pIsSpeakingEx) { lua_pushboolean(L, 0); return 1; }
    int engine = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, pIsSpeakingEx(engine));
    return 1;
}


/* Engine constants */
static const struct {
    const char *name;
    int value;
} engine_constants[] = {
    {"ENGINE_NONE",              SRAL_ENGINE_NONE},
    {"ENGINE_NVDA",              SRAL_ENGINE_NVDA},
    {"ENGINE_JAWS",              SRAL_ENGINE_JAWS},
    {"ENGINE_ZDSR",              SRAL_ENGINE_ZDSR},
    {"ENGINE_NARRATOR",          SRAL_ENGINE_NARRATOR},
    {"ENGINE_UIA",               SRAL_ENGINE_UIA},
    {"ENGINE_SAPI",              SRAL_ENGINE_SAPI},
    {"ENGINE_SPEECH_DISPATCHER", SRAL_ENGINE_SPEECH_DISPATCHER},
    {"ENGINE_NS_SPEECH",         SRAL_ENGINE_NS_SPEECH},
    {"ENGINE_VOICE_OVER",        SRAL_ENGINE_VOICE_OVER},
    {"ENGINE_AV_SPEECH",         SRAL_ENGINE_AV_SPEECH},
    {NULL, 0}
};

/* Module function table */
static const luaL_Reg sral_funcs[] = {
    {"load",                     l_load},
    {"unload",                   l_unload},
    {"is_loaded",                l_is_loaded},
    {"initialize",               l_initialize},
    {"uninitialize",             l_uninitialize},
    {"is_initialized",           l_is_initialized},
    {"speak",                    l_speak},
    {"braille",                  l_braille},
    {"output",                   l_output},
    {"stop_speech",              l_stop_speech},
    {"pause_speech",             l_pause_speech},
    {"resume_speech",            l_resume_speech},
    {"is_speaking",              l_is_speaking},
    {"get_current_engine",       l_get_current_engine},
    {"get_engine_features",      l_get_engine_features},
    {"get_available_engines",    l_get_available_engines},
    {"get_active_engines",       l_get_active_engines},
    {"get_engine_name",          l_get_engine_name},
    {"speak_ex",                 l_speak_ex},
    {"braille_ex",               l_braille_ex},
    {"output_ex",                l_output_ex},
    {"stop_speech_ex",           l_stop_speech_ex},
    {"register_keyboard_hooks",  l_register_keyboard_hooks},
    {"unregister_keyboard_hooks",l_unregister_keyboard_hooks},
    {"delay",                    l_delay},
    {"set_engines_exclude",      l_set_engines_exclude},
    {"get_engines_exclude",      l_get_engines_exclude},
    {"speak_ssml",               l_speak_ssml},
    {"speak_ssml_ex",            l_speak_ssml_ex},
    {"pause_speech_ex",          l_pause_speech_ex},
    {"resume_speech_ex",         l_resume_speech_ex},
    {"is_speaking_ex",           l_is_speaking_ex},
    {NULL, NULL}
};

/* Module entry point - called by require("sral_bridge") */
__declspec(dllexport) int luaopen_sral_bridge(lua_State *L) {
    luaL_register(L, "sral_bridge", sral_funcs);

    /* Add engine constants to the module table */
    for (int i = 0; engine_constants[i].name != NULL; i++) {
        lua_pushinteger(L, engine_constants[i].value);
        lua_setfield(L, -2, engine_constants[i].name);
    }

    return 1;
}
