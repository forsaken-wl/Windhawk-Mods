// ==WindhawkMod==
// @id              cmd-banner-win81
// @name            CMD Banner: Windows 8.1
// @description     Replaces the cmd.exe startup banner (and `ver`) with a Windows 8.1 one
// @version         1.0.0
// @author          why-are-these-things-wh-cpp
// @include         cmd.exe
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
#### REEEEEEEEEEEEADDDDDDDDDDDDDDDDD
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- versionLine: "Microsoft Windows [Version 6.3.9600]"
  $name: Version line
  $description: Replaces the whole "Microsoft Windows [Version ...]" line
- copyrightLine: "(c) 2013 Microsoft Corporation. All rights reserved."
  $name: Copyright line
  $description: Replaces the whole "(c) ... Microsoft Corporation. All rights reserved." line
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <string>

static std::wstring g_version;
static std::wstring g_copyright;

static void LoadSettings() {
    PCWSTR v = Wh_GetStringSetting(L"versionLine");
    g_version = v ? v : L"";
    Wh_FreeStringSetting(v);

    PCWSTR c = Wh_GetStringSetting(L"copyrightLine");
    g_copyright = c ? c : L"";
    Wh_FreeStringSetting(c);
}

static bool Patch(std::wstring& s) {
    bool changed = false;

    const std::wstring verKey = L"Microsoft Windows [Version";
    size_t p = s.find(verKey);
    if (p != std::wstring::npos && !g_version.empty()) {
        size_t e = s.find(L']', p);
        if (e != std::wstring::npos) {
            s.replace(p, e + 1 - p, g_version);
            changed = true;
        }
    }

    const std::wstring crKey = L"Microsoft Corporation. All rights reserved.";
    size_t q = s.find(crKey);
    if (q != std::wstring::npos && !g_copyright.empty()) {
        size_t start = s.rfind(L'\n', q);
        start = (start == std::wstring::npos) ? 0 : start + 1;
        if (start < q && s[start] == L'(') {
            s.replace(start, q + crKey.size() - start, g_copyright);
            changed = true;
        }
    }

    return changed;
}

using WriteConsoleW_t = BOOL(WINAPI*)(HANDLE, const VOID*, DWORD, LPDWORD, LPVOID);
static WriteConsoleW_t WriteConsoleW_Orig;

BOOL WINAPI WriteConsoleW_Hook(HANDLE h, const VOID* buf, DWORD n,
                               LPDWORD written, LPVOID reserved) {
    if (buf && n > 0 && n < 4096) {
        std::wstring s((const wchar_t*)buf, n);
        if ((s.find(L"Microsoft Windows [Version") != std::wstring::npos ||
             s.find(L"Microsoft Corporation. All rights reserved.") != std::wstring::npos) &&
            Patch(s)) {
            DWORD w = 0;
            BOOL r = WriteConsoleW_Orig(h, s.c_str(), (DWORD)s.size(), &w, reserved);
            // report the ORIGINAL length so cmd doesn't think the write was short
            if (r && written) *written = n;
            return r;
        }
    }
    return WriteConsoleW_Orig(h, buf, n, written, reserved);
}

BOOL Wh_ModInit() {
    LoadSettings();

    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    void* target = kb ? (void*)GetProcAddress(kb, "WriteConsoleW") : nullptr;
    if (!target) {
        Wh_Log(L"WriteConsoleW not found");
        return FALSE;
    }
    Wh_SetFunctionHook(target, (void*)WriteConsoleW_Hook, (void**)&WriteConsoleW_Orig);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}
