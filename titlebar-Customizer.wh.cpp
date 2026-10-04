// ==WindhawkMod==
// @id              titlebar-customizer
// @name            Titlebar Customizer
// @description     Title alignment, caption button geometry and window border thickness for DWM. Windows 10 21H2 (19044) only.
// @version         2.0.0
// @author          1x1x1x1
// @include         dwm.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
#### IT CUSTOMIZES THE TITLEBAR, WHAT ELSE DO YOU NEED?
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- titleAlign: left
  $name: Title alignment
  $options:
  - left: Left (stock)
  - center: Center
  - right: Right
- mode: win8
  $name: Button style
  $options:
  - vista: Windows Vista
  - basic: Windows Vista/7 Basic
  - xp: Windows XP / Classic
  - win8: Windows 8 / 8.1
  - win7: Windows 7
  - custom: Custom (use the Custom sizes below)
- topOffset: -1
  $name: Button vertical offset (px)
  $description: Negative moves the buttons up, positive moves them down. Not applied when maximized.
- rightOffset: 0
  $name: Button right offset (px)
  $description: Positive moves the button group away from the right edge, negative toward it.
- gap: 0
  $name: Gap between buttons (px)
  $description: Negative values overlap the buttons.
- customHeight: 20
  $name: Custom buttons - height (px)
- customMinWidth: 29
  $name: Custom buttons - minimize width (px)
- customMaxWidth: 27
  $name: Custom buttons - maximize/restore width (px)
- customCloseWidth: 53
  $name: Custom buttons - close width (px)
- borders: stock
  $name: Window borders
  $options:
  - stock: Windows 10 (thin)
  - thick: Thick (system frame, like 8.1)
  - custom: Custom thickness
- borderThickness: 6
  $name: Custom border thickness (px)
  $description: Used when Window borders is set to Custom.
- clientEdge: false
  $name: Account for sunken client edge
  $description: Also shrinks the margins on windows that have a sunken client edge (WS_EX_CLIENTEDGE).
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <wchar.h>

// ---------------------------------------------------------------------------
// settings
// ---------------------------------------------------------------------------

// Pixel sizes at 96 DPI. Preset values are approximate; use Custom to tune.
struct Geo { int h, wMin, wMax, wClose; };

static const Geo kVista = { 18, 28, 28, 42 };
static const Geo kBasic = { 18, 25, 25, 43 };
static const Geo kXP    = { 21, 21, 21, 21 };
static const Geo kWin8  = { 20, 29, 27, 53 };
static const Geo kWin7  = { 21, 29, 27, 55 };
static Geo       g_custom = { 20, 29, 27, 53 };

enum Align  { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };
enum Border { BORDER_STOCK = 0, BORDER_THICK = 1, BORDER_CUSTOM = 2 };

static const Geo* g_geo = &kWin8;
static int  g_align       = ALIGN_LEFT;
static int  g_border      = BORDER_STOCK;
static int  g_borderPx    = 6;
static bool g_clientEdge  = false;
static int  g_topOffset   = -1;
static int  g_rightOffset = 0;
static int  g_gap         = 0;

static int ClampPx(int v) { return v < 1 ? 1 : (v > 400 ? 400 : v); }

static void LoadSettings() {
    g_custom.h      = ClampPx(Wh_GetIntSetting(L"customHeight"));
    g_custom.wMin   = ClampPx(Wh_GetIntSetting(L"customMinWidth"));
    g_custom.wMax   = ClampPx(Wh_GetIntSetting(L"customMaxWidth"));
    g_custom.wClose = ClampPx(Wh_GetIntSetting(L"customCloseWidth"));

    PCWSTR m = Wh_GetStringSetting(L"mode");
    if (m) {
        if      (!wcscmp(m, L"vista"))  g_geo = &kVista;
        else if (!wcscmp(m, L"basic"))  g_geo = &kBasic;
        else if (!wcscmp(m, L"xp"))     g_geo = &kXP;
        else if (!wcscmp(m, L"win7"))   g_geo = &kWin7;
        else if (!wcscmp(m, L"custom")) g_geo = &g_custom;
        else                            g_geo = &kWin8;
    }
    Wh_FreeStringSetting(m);

    PCWSTR a = Wh_GetStringSetting(L"titleAlign");
    g_align = ALIGN_LEFT;
    if (a) {
        if      (!wcscmp(a, L"center")) g_align = ALIGN_CENTER;
        else if (!wcscmp(a, L"right"))  g_align = ALIGN_RIGHT;
    }
    Wh_FreeStringSetting(a);

    PCWSTR b = Wh_GetStringSetting(L"borders");
    g_border = BORDER_STOCK;
    if (b) {
        if      (!wcscmp(b, L"thick"))  g_border = BORDER_THICK;
        else if (!wcscmp(b, L"custom")) g_border = BORDER_CUSTOM;
    }
    Wh_FreeStringSetting(b);

    g_borderPx    = ClampPx(Wh_GetIntSetting(L"borderThickness"));
    g_clientEdge  = Wh_GetIntSetting(L"clientEdge") != 0;
    g_topOffset   = Wh_GetIntSetting(L"topOffset");
    g_rightOffset = Wh_GetIntSetting(L"rightOffset");
    g_gap         = Wh_GetIntSetting(L"gap");
}

// ---------------------------------------------------------------------------
// uDWM layout (19044)
// ---------------------------------------------------------------------------

// CTopLevelWindow (pThis)
static const int OFF_STATE    = 240;   // BYTE, bit 4 = maximized
static const int OFF_TEXT     = 520;   // CText* (title)
static const int OFF_BUTTONS  = 488;   // CVisual* [4]: 1 min, 2 max/restore, 3 close
static const int OFF_FLAGS    = 592;   // DWORD, bit 2 = tool window
static const int OFF_MARGINS  = 628;   // MARGINS, client margins (non-maximized)
static const int OFF_WINDATA  = 728;   // CWindowData*
static const int STATE_MAX    = 4;
static const int FLAG_TOOL    = 2;

// CWindowData (pointed to by pThis+728)
static const int WD_CLIENTMARGINS = 64;   // MARGINS
static const int WD_STYLE         = 100;  // DWORD
static const int WD_EXSTYLE       = 104;  // DWORD
static const int WD_DPI           = 324;  // UINT

using UpdateNCAreaButton_t  = long (*)(void*, int, int, int, int*);
using PositionsAndSizes_t   = long (*)(void*);
using UpdatePinnedParts_t   = long (*)(void*);
using UpdateMargins_t       = bool (*)(void*);
using CVisual_SetSize_t     = long (*)(void*, const SIZE*);
using CVisual_SetDirty_t    = void (*)(void*, unsigned long);

static UpdateNCAreaButton_t UpdateNCAreaButton_Orig;
static PositionsAndSizes_t  PositionsAndSizes_Orig;
static UpdateMargins_t      UpdateMargins_Orig;
static UpdatePinnedParts_t  UpdatePinnedParts;
static CVisual_SetSize_t    CVisual_SetSize;
static CVisual_SetDirty_t   CVisual_SetDirtyFlags;
static bool                 g_hooked = false;

static UINT WinDpi(char* base) {
    char* wd = *(char**)(base + OFF_WINDATA);
    UINT dpi = wd ? *(UINT*)(wd + WD_DPI) : 96;
    return dpi ? dpi : 96;
}

// ---------------------------------------------------------------------------
// title alignment
// ---------------------------------------------------------------------------

using DrawTextW_t = int (WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT);
static DrawTextW_t DrawTextW_Orig;

static int WINAPI DrawTextW_Hook(HDC dc, LPCWSTR text, int cch, LPRECT rc, UINT fmt) {
    if (g_align == ALIGN_LEFT || !rc)
        return DrawTextW_Orig(dc, text, cch, rc, fmt);

    bool calc = (fmt & DT_CALCRECT) != 0;
    if (!calc) rc->right -= 2;   // keep the text off the right margin

    UINT f = (fmt & ~(UINT)(DT_CENTER | DT_RIGHT)) |
             (g_align == ALIGN_CENTER ? DT_CENTER : DT_RIGHT);
    int r = DrawTextW_Orig(dc, text, cch, rc, f);

    // make DWM size the title visual to the whole available span, so the
    // alignment applies across it instead of just around the text
    if (calc) rc->right = 10000;
    return r;
}

// ---------------------------------------------------------------------------
// caption buttons
// ---------------------------------------------------------------------------

static SIZE ButtonSize(char* base, int id) {
    UINT dpi = WinDpi(base);
    const Geo& g = *g_geo;
    int w96 = (id == 3) ? g.wClose : (id == 2) ? g.wMax : g.wMin;
    SIZE sz;
    sz.cx = MulDiv(w96, dpi, 96);
    sz.cy = MulDiv(g.h, dpi, 96);
    return sz;
}

// in case uDWM still calls the standalone button function somewhere
static long UpdateNCAreaButton_Hook(void* pThis, int id, int height,
                                    int offsetTop, int* offsetRight) {
    char* base = (char*)pThis;
    long hr = UpdateNCAreaButton_Orig(pThis, id, height, offsetTop, offsetRight);

    if (hr < 0 || id < 0 || id > 3 || !CVisual_SetSize ||
        (*(DWORD*)(base + OFF_FLAGS) & FLAG_TOOL)) {
        return hr;
    }
    DWORD* btn = *(DWORD**)(base + OFF_BUTTONS + 8 * id);
    if (!btn) return hr;

    SIZE sz = ButtonSize(base, id);
    int oldW = (int)btn[30];
    if (CVisual_SetSize(btn, &sz) >= 0) {
        *offsetRight += (int)btn[30] - oldW;
    }
    return hr;
}

// main path: let stock lay everything out, then fix the buttons
static long PositionsAndSizes_Hook(void* pThis) {
    long hr = PositionsAndSizes_Orig(pThis);
    if (hr < 0 || !CVisual_SetSize || !CVisual_SetDirtyFlags) return hr;

    char* base = (char*)pThis;
    if (*(DWORD*)(base + OFF_FLAGS) & FLAG_TOOL) return hr;

    DWORD* btns[4];
    for (int id = 0; id < 4; id++)
        btns[id] = *(DWORD**)(base + OFF_BUTTONS + 8 * id);

    // starting right inset = right inset stock gave the rightmost existing button
    int right = -1;
    for (int id = 3; id >= 0; id--) {
        if (btns[id]) { right = (int)btns[id][33]; break; }
    }
    if (right < 0) return hr;

    UINT dpi = WinDpi(base);
    bool maximized = (*(BYTE*)(base + OFF_STATE) & STATE_MAX) != 0;
    int dy  = (!maximized && g_topOffset) ? MulDiv(g_topOffset, dpi, 96) : 0;
    int dx  = g_rightOffset ? MulDiv(g_rightOffset, dpi, 96) : 0;
    int gap = g_gap ? MulDiv(g_gap, dpi, 96) : 0;

    right += dx;
    if (right < 0) right = 0;

    int placed = 0;
    for (int id = 3; id >= 0; id--) {
        DWORD* btn = btns[id];
        if (!btn) continue;

        SIZE sz = ButtonSize(base, id);
        CVisual_SetSize(btn, &sz);

        if ((int)btn[33] != right) {
            btn[33] = right;
            CVisual_SetDirtyFlags(btn, 2);
        }
        if (dy) {
            // stock re-sets the top inset on every layout, so this never accumulates
            btn[34] = (DWORD)((int)btn[34] + dy);
            CVisual_SetDirtyFlags(btn, 2);
        }
        right += (int)btn[30] + gap;
        placed++;
    }
    if (placed) right -= gap;   // no gap after the leftmost button

    // title text must stop where the buttons start
    DWORD* text = *(DWORD**)(base + OFF_TEXT);
    if (text && (int)text[33] != right) {
        text[33] = right;
        CVisual_SetDirtyFlags(text, 2);
    }

    if (UpdatePinnedParts) UpdatePinnedParts(pThis);
    return hr;
}

// ---------------------------------------------------------------------------
// window borders
// ---------------------------------------------------------------------------

static inline int Max(int a, int b) { return a > b ? a : b; }

// same layout as the Win32 MARGINS struct (which lives in uxtheme.h/dwmapi.h)
struct Margins {
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
};

using AdjustWindowRectExForDpi_t = BOOL (WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
static AdjustWindowRectExForDpi_t g_AdjustForDpi;

static bool UpdateMargins_Hook(void* pThis) {
    bool ret = UpdateMargins_Orig(pThis);
    if (g_border == BORDER_STOCK) return ret;

    char* base = (char*)pThis;
    char* wd = *(char**)(base + OFF_WINDATA);
    if (!wd) return ret;

    Margins cm = *(Margins*)(wd + WD_CLIENTMARGINS);
    // only windows whose frame is extended on all four sides
    if (!(cm.cxLeftWidth > 0 && cm.cxRightWidth > 0 &&
          cm.cyTopHeight > 0 && cm.cyBottomHeight > 0)) {
        return ret;
    }

    DWORD style   = *(DWORD*)(wd + WD_STYLE);
    DWORD exStyle = *(DWORD*)(wd + WD_EXSTYLE);
    UINT dpi = WinDpi(base);

    // frame this window would have with a normal resizable system frame
    RECT fr = {};
    if (g_AdjustForDpi) g_AdjustForDpi(&fr, style | WS_THICKFRAME, FALSE, exStyle, dpi);
    else                AdjustWindowRectEx(&fr, style | WS_THICKFRAME, FALSE, exStyle);

    if (g_border == BORDER_CUSTOM) {
        int f = fr.right;                    // system frame thickness
        int t = MulDiv(g_borderPx, dpi, 96);
        fr.left   = -t;
        fr.right  = t;
        fr.bottom = t;
        fr.top   += f - t;                   // keep the caption part of the top
    }

    Margins out;
    out.cxLeftWidth    = cm.cxLeftWidth    - Max(cm.cxLeftWidth,    -fr.left);
    out.cxRightWidth   = cm.cxRightWidth   - Max(cm.cxRightWidth,    fr.right);
    out.cyTopHeight    = cm.cyTopHeight    - Max(cm.cyTopHeight,    -fr.top);
    out.cyBottomHeight = cm.cyBottomHeight - Max(cm.cyBottomHeight,  fr.bottom);

    if (g_clientEdge && (exStyle & WS_EX_CLIENTEDGE)) {
        out.cxLeftWidth    -= 2;
        out.cxRightWidth   -= 2;
        out.cyBottomHeight -= 2;
    }

    *(Margins*)(base + OFF_MARGINS) = out;
    return ret;
}

// ---------------------------------------------------------------------------
// hooking
// ---------------------------------------------------------------------------

static bool HookUdwm(HMODULE mod) {
    WH_FIND_SYMBOL fs;
    HANDLE h = Wh_FindFirstSymbol(mod, nullptr, &fs);
    if (!h) {
        Wh_Log(L"Wh_FindFirstSymbol failed");
        return false;
    }

    void* upd = nullptr;
    void* pos = nullptr;
    void* mar = nullptr;
    do {
        if (!_wcsicmp(fs.symbol,
                L"private: long __cdecl CTopLevelWindow::UpdateNCAreaButton(enum CTopLevelWindow::ButtonType,int,int,int *)")) {
            upd = fs.address;
        } else if (!_wcsicmp(fs.symbol,
                L"private: long __cdecl CTopLevelWindow::UpdateNCAreaPositionsAndSizes(void)")) {
            pos = fs.address;
        } else if (!_wcsicmp(fs.symbol,
                L"private: bool __cdecl CTopLevelWindow::UpdateMarginsDependentOnStyle(void)")) {
            mar = fs.address;
        } else if (!_wcsicmp(fs.symbol,
                L"private: long __cdecl CTopLevelWindow::UpdatePinnedParts(void)")) {
            UpdatePinnedParts = (UpdatePinnedParts_t)fs.address;
        } else if (!_wcsicmp(fs.symbol,
                L"public: virtual long __cdecl CVisual::SetSize(struct tagSIZE const *)")) {
            CVisual_SetSize = (CVisual_SetSize_t)fs.address;
        } else if (!_wcsicmp(fs.symbol,
                L"public: virtual void __cdecl CVisual::SetDirtyFlags(unsigned long)")) {
            CVisual_SetDirtyFlags = (CVisual_SetDirty_t)fs.address;
        }
    } while (Wh_FindNextSymbol(h, &fs));
    Wh_FindCloseSymbol(h);

    bool any = false;

    if (pos && CVisual_SetSize && CVisual_SetDirtyFlags) {
        Wh_SetFunctionHook(pos, (void*)PositionsAndSizes_Hook,
                           (void**)&PositionsAndSizes_Orig);
        if (upd) {
            Wh_SetFunctionHook(upd, (void*)UpdateNCAreaButton_Hook,
                               (void**)&UpdateNCAreaButton_Orig);
        }
        any = true;
    } else {
        Wh_Log(L"Button layout symbols not found; button settings disabled");
    }

    if (mar) {
        Wh_SetFunctionHook(mar, (void*)UpdateMargins_Hook, (void**)&UpdateMargins_Orig);
        any = true;
    } else {
        Wh_Log(L"UpdateMarginsDependentOnStyle not found; border settings disabled");
    }

    return any;
}

// uDWM may not be loaded yet if we're injected at dwm start
using LoadLibraryExW_t = HMODULE (WINAPI*)(LPCWSTR, HANDLE, DWORD);
static LoadLibraryExW_t LoadLibraryExW_Orig;

static HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE m = LoadLibraryExW_Orig(name, file, flags);
    if (m && !g_hooked && m == GetModuleHandleW(L"uDWM.dll")) {
        g_hooked = HookUdwm(m);
        if (g_hooked) Wh_ApplyHookOperations();
    }
    return m;
}

BOOL Wh_ModInit() {
    LoadSettings();

    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        g_AdjustForDpi = (AdjustWindowRectExForDpi_t)GetProcAddress(u32, "AdjustWindowRectExForDpi");
        void* dt = (void*)GetProcAddress(u32, "DrawTextW");
        if (dt) Wh_SetFunctionHook(dt, (void*)DrawTextW_Hook, (void**)&DrawTextW_Orig);
    }

    HMODULE ud = GetModuleHandleW(L"uDWM.dll");
    if (ud) {
        g_hooked = HookUdwm(ud);
        return g_hooked ? TRUE : FALSE;
    }

    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    void* ll = kb ? (void*)GetProcAddress(kb, "LoadLibraryExW") : nullptr;
    if (!ll) return FALSE;
    Wh_SetFunctionHook(ll, (void*)LoadLibraryExW_Hook, (void**)&LoadLibraryExW_Orig);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}
