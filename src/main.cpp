// CSV Commander — CSV viewer/editor ringan, pure Win32 API, tanpa dependency.
// Mode: View (tabel) / Split / Edit (teks mentah). Multi-tab, sidebar explorer,
// sort per kolom, filter, drag & drop.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <thread>
#include <regex>
#include <unordered_set>
#include <cstdio>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ------------------------------------------------------------------ konstanta

enum {
    IDC_LIST = 101, IDC_EDIT = 102, IDC_SEARCH = 103, IDC_SIDELIST = 104, IDC_CELLEDIT = 105,

    // tombol custom-paint (bukan child window)
    BTN_SIDEBAR = 110, BTN_OPENFOLDER = 111, BTN_OPENFILE = 112, BTN_SAVE = 113,
    BTN_MODE_VIEW = 114, BTN_MODE_SPLIT = 115, BTN_MODE_EDIT = 116,
    BTN_NEWTAB = 117, BTN_OPENFOLDER_BIG = 118, BTN_EXPORT = 119, BTN_REPLACE = 120,
    BTN_THEME = 121, BTN_DELIM = 122, BTN_ENC = 123,

    HIT_TAB_BASE = 1000,      // + index
    HIT_CLOSE_BASE = 2000,    // + index
    HIT_MRU_BASE = 3000,      // + index (daftar recent di layar kosong)

    IDM_OPEN = 300, IDM_SAVE, IDM_SAVEAS, IDM_NEW, IDM_CLOSETAB,
    IDM_FIND, IDM_NEXTTAB, IDM_PREVTAB, IDM_SELALL,
    IDM_MODE_VIEW, IDM_MODE_SPLIT, IDM_MODE_EDIT,
    IDM_REPLACE, IDM_UNDO, IDM_REDO, IDM_EXPORT_ALL, IDM_EXPORT_FILTERED,
    IDM_COPY, IDM_FINDNEXT, IDM_FINDPREV, IDM_ESCAPE, IDM_EDITCELL, IDM_DELKEY,

    // context menu tabel
    IDM_ROW_INS_ABOVE = 340, IDM_ROW_INS_BELOW, IDM_ROW_DUP, IDM_ROW_DEL,
    IDM_COL_INS_LEFT, IDM_COL_INS_RIGHT, IDM_COL_RENAME, IDM_COL_DEL,
    IDM_COL_STATS, IDM_COL_AUTOFIT,

    // status bar: delimiter & encoding
    IDM_DELIM_COMMA = 360, IDM_DELIM_SEMI, IDM_DELIM_TAB, IDM_DELIM_PIPE,
    IDM_ENC_UTF8 = 370, IDM_ENC_UTF8BOM, IDM_ENC_ANSI,

    // dialog find & replace
    FR_FIND = 401, FR_REP, FR_REGEX, FR_CASE, FR_PREVIEW, FR_APPLY, FR_INFO, FR_LIST,

    TIMER_REPARSE = 1, TIMER_FILTER = 2,
};

// pesan dari thread loader + dari subclass header (klik kanan kolom / autofit divider)
enum { WM_APP_PROG = WM_APP + 1, WM_APP_DONE = WM_APP + 2,
       WM_APP_HDRMENU = WM_APP + 3, WM_APP_HDRFIT = WM_APP + 4 };

enum Mode { MODE_VIEW = 0, MODE_SPLIT = 1, MODE_EDIT = 2 };

// ------------------------------------------------------------------ tema

struct Theme {
    COLORREF bg, chrome, border, text, muted, accent, hot, altrow, hdrSep,
             badrow, warnrow, match, disabled, closeHot;
};

static const Theme THEME_LIGHT = {
    RGB(255, 255, 255),   // bg
    RGB(247, 248, 247),   // chrome
    RGB(228, 231, 228),   // border
    RGB(31, 35, 40),      // text
    RGB(115, 122, 115),   // muted
    RGB(15, 122, 77),     // accent
    RGB(233, 237, 233),   // hot
    RGB(235, 244, 239),   // altrow: belang hijau muda
    RGB(58, 148, 106),    // hdrSep: pemisah kolom di header hijau
    RGB(252, 231, 231),   // badrow: baris rusak (jumlah kolom salah)
    RGB(255, 244, 224),   // warnrow: tipe kolom tidak cocok
    RGB(196, 230, 213),   // match: sel hasil pencarian (F3)
    RGB(180, 185, 180),   // disabled
    RGB(220, 224, 220),   // closeHot
};

static const Theme THEME_DARK = {
    RGB(24, 26, 27),      // bg
    RGB(33, 36, 37),      // chrome
    RGB(56, 61, 62),      // border
    RGB(226, 229, 226),   // text
    RGB(148, 155, 148),   // muted
    RGB(46, 160, 108),    // accent
    RGB(50, 55, 56),      // hot
    RGB(31, 37, 34),      // altrow
    RGB(82, 175, 130),    // hdrSep
    RGB(76, 40, 40),      // badrow
    RGB(74, 62, 34),      // warnrow
    RGB(28, 82, 56),      // match
    RGB(95, 100, 95),     // disabled
    RGB(70, 76, 72),      // closeHot
};

// palet aktif; nama CLR_* dipertahankan agar seluruh kode paint tetap sama
static COLORREF CLR_BG, CLR_CHROME, CLR_BORDER, CLR_TEXT, CLR_MUTED, CLR_ACCENT,
                CLR_HOT, CLR_ALTROW, CLR_HDR_SEP, CLR_BADROW, CLR_WARNROW,
                CLR_MATCH, CLR_DISABLED, CLR_CLOSEHOT;

// ------------------------------------------------------------------ dokumen

enum Enc { ENC_UTF8 = 0, ENC_UTF8BOM = 1, ENC_ANSI = 2 };

struct Document {
    std::wstring path;                 // kosong = belum tersimpan
    std::wstring title;
    std::wstring rawText;
    int enc = ENC_UTF8;
    bool dirty = false;
    bool editTouched = false;          // teks di kontrol EDIT lebih baru dari rawText
    bool parsedStale = true;

    wchar_t delim = L',';
    bool delimManual = false;          // user override → jangan deteksi ulang
    std::vector<std::vector<std::wstring>> rows;   // rows[0] = header
    int numCols = 0;

    std::vector<int> visible;          // indeks baris data (>=1), sudah difilter+sort
    int sortCol = -1;                  // -1 = urutan asli
    bool sortAsc = true;
    std::wstring filter;

    // validasi skema
    std::vector<int> colTypes;         // per kolom: 0 teks, 1 angka, 2 tanggal
    std::vector<uint8_t> rowFlags;     // per baris data: 0 ok, 1 kolom salah, 2 tipe tak cocok
    int badRows = 0;

    // background loading
    int id = 0;
    bool loading = false;
    bool loadFailed = false;
    int loadPercent = 0;

    // undo/redo (untuk operasi Ganti Semua)
    std::vector<std::wstring> undoStack, redoStack;
};

// ------------------------------------------------------------------ global

static HINSTANCE g_hInst;
static HWND g_hMain, g_hList, g_hEdit, g_hSearch, g_hSideList, g_hFind, g_hTip, g_hCellEdit;
static int g_docIdSeq = 0;
static HFONT g_fontUI, g_fontUIBold, g_fontMono, g_fontIcon, g_fontIconSm, g_fontIconBig;
static int g_dpi = 96;

static std::vector<Document*> g_docs;
static int g_cur = -1;
static int g_mode = MODE_VIEW;
static bool g_sidebarVisible = true;
static bool g_suppressEditNotify = false;

static std::wstring g_folder;
static std::vector<std::wstring> g_folderFiles;   // nama file saja

static RECT g_rcToolbar, g_rcTabbar, g_rcSidebar, g_rcContent, g_rcStatus, g_rcSearchBox, g_rcSplit;
struct HitBtn { int id; RECT rc; };
static std::vector<HitBtn> g_hits;
static int g_hotId = -1;
static bool g_trackingMouse = false;

// tema
static bool g_dark = false;
static int g_themePref = -1;                       // -1 ikuti sistem, 0 terang, 1 gelap
static HBRUSH g_brChrome = nullptr, g_brBg = nullptr;

// splitter mode Split
static double g_split = 0.5;
static bool g_dragSplit = false;

// cell edit in-place
static int g_editItem = -1, g_editSub = -1;        // item = indeks visible, sub >= 1
static int g_editHdrCol = -1;                      // >= 0 → sedang rename header kolom
static bool g_cellEditActive = false;

// sel terpilih (untuk status bar) + hasil pencarian F3
static int g_selSub = 1;
static int g_matchItem = -1, g_matchSub = -1;

// recent files (MRU)
static std::vector<std::wstring> g_mru;
static const int MRU_MAX = 8;

static int S(int v) { return MulDiv(v, g_dpi, 96); }
static Document* Cur() { return (g_cur >= 0 && g_cur < (int)g_docs.size()) ? g_docs[g_cur] : nullptr; }

// ------------------------------------------------------------------ util

static bool PtInRc(const RECT& rc, POINT pt) { return PtInRect(&rc, pt) != 0; }

static std::wstring GetWndText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n, 0);
    if (n) GetWindowTextW(h, &s[0], n + 1);
    return s;
}

// kontrol EDIT butuh CRLF; normalkan LF tunggal → CRLF
static void NormalizeCRLF(std::wstring& s) {
    size_t extra = 0;
    for (size_t i = 0; i < s.size(); i++)
        if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r')) extra++;
    if (!extra) return;
    std::wstring out;
    out.reserve(s.size() + extra);
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r')) out += L'\r';
        out += s[i];
    }
    s.swap(out);
}

static std::wstring FileNameOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

// baca file → wstring, deteksi encoding (UTF-8/UTF-16 BOM/ANSI fallback).
// Pembacaan per 4MB agar progress bisa dilaporkan untuk file besar.
static bool ReadFileText(const std::wstring& path, std::wstring& out, int& enc,
                         const std::function<void(int)>& prog = {}) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
    if (sz.QuadPart > 512LL * 1024 * 1024) { CloseHandle(h); return false; }
    size_t total = (size_t)sz.QuadPart, got = 0;
    std::string bytes(total, 0);
    while (got < total) {
        DWORD chunk = (DWORD)std::min<size_t>(4 * 1024 * 1024, total - got);
        DWORD rd = 0;
        if (!ReadFile(h, &bytes[got], chunk, &rd, nullptr) || rd == 0) break;
        got += rd;
        if (prog && total) prog((int)(got * 50 / total));
    }
    CloseHandle(h);
    bytes.resize(got);
    if (prog) prog(55);

    enc = ENC_UTF8;
    const unsigned char* b = (const unsigned char*)bytes.data();
    size_t n = bytes.size();
    if (n >= 2 && b[0] == 0xFF && b[1] == 0xFE) {           // UTF-16 LE → disimpan ulang sebagai UTF-8
        out.assign((const wchar_t*)(bytes.data() + 2), (n - 2) / 2);
        return true;
    }
    size_t off = 0;
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) { enc = ENC_UTF8BOM; off = 3; }
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + off, (int)(n - off), nullptr, 0);
    UINT cp = CP_UTF8;
    if (wlen == 0 && n > off) {
        cp = CP_ACP; enc = ENC_ANSI;
        wlen = MultiByteToWideChar(cp, 0, bytes.data() + off, (int)(n - off), nullptr, 0);
    }
    out.resize(wlen);
    if (wlen) MultiByteToWideChar(cp, 0, bytes.data() + off, (int)(n - off), &out[0], wlen);
    return true;
}

static bool WriteFileText(const std::wstring& path, const std::wstring& text, int enc) {
    UINT cp = (enc == ENC_ANSI) ? CP_ACP : CP_UTF8;
    int blen = WideCharToMultiByte(cp, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string bytes(blen, 0);
    if (blen) WideCharToMultiByte(cp, 0, text.c_str(), (int)text.size(), &bytes[0], blen, nullptr, nullptr);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr;
    if (enc == ENC_UTF8BOM) { const unsigned char bom[3] = {0xEF, 0xBB, 0xBF}; WriteFile(h, bom, 3, &wr, nullptr); }
    BOOL ok = WriteFile(h, bytes.data(), (DWORD)bytes.size(), &wr, nullptr);
    CloseHandle(h);
    return ok != 0;
}

// ------------------------------------------------------------------ parsing CSV

static wchar_t DetectDelim(const std::wstring& t) {
    int counts[4] = {0, 0, 0, 0};                     // , ; \t |
    bool q = false;
    size_t lines = 0;
    for (size_t i = 0; i < t.size() && lines < 5; i++) {
        wchar_t c = t[i];
        if (q) { if (c == L'"') q = false; continue; }
        if (c == L'"') q = true;
        else if (c == L',') counts[0]++;
        else if (c == L';') counts[1]++;
        else if (c == L'\t') counts[2]++;
        else if (c == L'|') counts[3]++;
        else if (c == L'\n') lines++;
    }
    const wchar_t d[4] = {L',', L';', L'\t', L'|'};
    int best = 0;
    for (int i = 1; i < 4; i++) if (counts[i] > counts[best]) best = i;
    return counts[best] > 0 ? d[best] : L',';
}

static void ValidateDoc(Document* d);

static void ParseCSV(Document* d) {
    d->rows.clear();
    d->numCols = 0;
    const std::wstring& t = d->rawText;
    size_t start = (!t.empty() && t[0] == 0xFEFF) ? 1 : 0;   // buang BOM nyasar
    if (!d->delimManual) d->delim = DetectDelim(t);

    std::vector<std::wstring> cur;
    std::wstring cell;
    bool q = false;
    for (size_t i = start; i < t.size(); i++) {
        wchar_t c = t[i];
        if (q) {
            if (c == L'"') {
                if (i + 1 < t.size() && t[i + 1] == L'"') { cell += L'"'; i++; }
                else q = false;
            } else cell += c;
        } else {
            if (c == L'"') q = true;
            else if (c == d->delim) { cur.push_back(std::move(cell)); cell.clear(); }
            else if (c == L'\r') { /* skip */ }
            else if (c == L'\n') {
                cur.push_back(std::move(cell)); cell.clear();
                d->rows.push_back(std::move(cur)); cur.clear();
            }
            else cell += c;
        }
    }
    if (!cell.empty() || !cur.empty()) { cur.push_back(std::move(cell)); d->rows.push_back(std::move(cur)); }
    // buang baris kosong murni di akhir
    while (!d->rows.empty() && d->rows.back().size() == 1 && d->rows.back()[0].empty())
        d->rows.pop_back();
    for (auto& r : d->rows) d->numCols = std::max(d->numCols, (int)r.size());
    ValidateDoc(d);
    d->parsedStale = false;
}

static const std::wstring& CellAt(const Document* d, int row, int col) {
    static const std::wstring empty;
    if (row < 0 || row >= (int)d->rows.size()) return empty;
    const auto& r = d->rows[row];
    return col < (int)r.size() ? r[col] : empty;
}

static bool NumericVal(const std::wstring& s, double& v) {
    if (s.empty()) return false;
    wchar_t* end = nullptr;
    v = wcstod(s.c_str(), &end);
    return end && *end == 0;
}

// tanggal sederhana: 3 bagian angka dipisah - / . , salah satu bagian 4 digit (tahun)
static bool LooksDate(const std::wstring& s) {
    int lens[3] = {0, 0, 0};
    int pi = 0;
    wchar_t sep = 0;
    for (wchar_t c : s) {
        if (c >= L'0' && c <= L'9') { if (++lens[pi] > 4) return false; }
        else if (c == L'-' || c == L'/' || c == L'.') {
            if (sep && c != sep) return false;
            sep = c;
            if (++pi > 2) return false;
        }
        else return false;
    }
    if (pi != 2 || !lens[0] || !lens[1] || !lens[2]) return false;
    return lens[0] == 4 || lens[2] == 4;
}

// inferensi tipe kolom (>=90% nilai non-kosong seragam) + tandai baris menyimpang
static void ValidateDoc(Document* d) {
    int nc = d->numCols, nd = (int)d->rows.size() - 1;
    d->colTypes.assign(nc > 0 ? nc : 0, 0);
    d->rowFlags.assign(nd > 0 ? nd : 0, 0);
    d->badRows = 0;
    if (nc <= 0 || nd <= 0) return;
    for (int c = 0; c < nc; c++) {
        int num = 0, dat = 0, nonEmpty = 0;
        for (int r = 1; r <= nd; r++) {
            const std::wstring& s = CellAt(d, r, c);
            if (s.empty()) continue;
            nonEmpty++;
            double v;
            if (NumericVal(s, v)) num++;
            else if (LooksDate(s)) dat++;
        }
        if (nonEmpty > 0) {
            if (num * 10 >= nonEmpty * 9) d->colTypes[c] = 1;
            else if (dat * 10 >= nonEmpty * 9) d->colTypes[c] = 2;
        }
    }
    int expect = (int)d->rows[0].size();   // skema mengikuti lebar header, bukan baris terpanjang
    for (int r = 1; r <= nd; r++) {
        const auto& row = d->rows[r];
        uint8_t fl = 0;
        if ((int)row.size() != expect) fl = 1;
        else {
            for (int c = 0; c < nc && !fl; c++) {
                if (!d->colTypes[c]) continue;
                const std::wstring& s = row[c];
                if (s.empty()) continue;
                double v;
                if (d->colTypes[c] == 1) { if (!NumericVal(s, v)) fl = 2; }
                else                     { if (!LooksDate(s))    fl = 2; }
            }
        }
        if (fl) d->badRows++;
        d->rowFlags[r - 1] = fl;
    }
}

static void RebuildVisible(Document* d) {
    d->visible.clear();
    int n = (int)d->rows.size();
    d->visible.reserve(n > 1 ? n - 1 : 0);
    if (d->filter.empty()) {
        for (int i = 1; i < n; i++) d->visible.push_back(i);
    } else {
        for (int i = 1; i < n; i++) {
            for (const auto& c : d->rows[i]) {
                if (StrStrIW(c.c_str(), d->filter.c_str())) { d->visible.push_back(i); break; }
            }
        }
    }
    if (d->sortCol >= 0) {
        int col = d->sortCol;
        bool asc = d->sortAsc;
        std::stable_sort(d->visible.begin(), d->visible.end(), [d, col, asc](int a, int b) {
            const std::wstring& sa = CellAt(d, a, col);
            const std::wstring& sb = CellAt(d, b, col);
            double va, vb;
            int r;
            if (NumericVal(sa, va) && NumericVal(sb, vb)) r = (va < vb) ? -1 : (va > vb ? 1 : 0);
            else r = StrCmpLogicalW(sa.c_str(), sb.c_str());
            return asc ? r < 0 : r > 0;
        });
    }
}

// ------------------------------------------------------------------ list view

static void UpdateHeaderArrows(Document* d) {
    HWND hdr = ListView_GetHeader(g_hList);
    int cnt = Header_GetItemCount(hdr);
    for (int i = 0; i < cnt; i++) {
        HDITEMW hi = {}; hi.mask = HDI_FORMAT;
        Header_GetItem(hdr, i, &hi);
        hi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (d && d->sortCol == i - 1 && d->sortCol >= 0)
            hi.fmt |= d->sortAsc ? HDF_SORTUP : HDF_SORTDOWN;
        Header_SetItem(hdr, i, &hi);
    }
}

static void RefreshListColumns(Document* d) {
    SendMessageW(g_hList, WM_SETREDRAW, FALSE, 0);
    while (ListView_DeleteColumn(g_hList, 0)) {}
    ListView_SetItemCountEx(g_hList, 0, 0);
    if (d && !d->rows.empty()) {
        LVCOLUMNW c = {};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        c.fmt = LVCFMT_RIGHT;
        c.pszText = (LPWSTR)L"#";
        c.cx = S(52);
        ListView_InsertColumn(g_hList, 0, &c);
        for (int i = 0; i < d->numCols; i++) {
            std::wstring name = CellAt(d, 0, i);
            if (name.empty()) { name = L"Kolom " + std::to_wstring(i + 1); }
            // perkiraan lebar: header vs sampel 100 baris pertama
            size_t chars = name.size();
            int sample = std::min((int)d->rows.size(), 101);
            for (int r = 1; r < sample; r++) chars = std::max(chars, CellAt(d, r, i).size());
            int w = std::min(std::max((int)chars * S(8) + S(24), S(70)), S(340));
            c.fmt = LVCFMT_LEFT;
            c.pszText = (LPWSTR)name.c_str();
            c.cx = w;
            ListView_InsertColumn(g_hList, i + 1, &c);
        }
        ListView_SetItemCountEx(g_hList, (int)d->visible.size(), 0);
        UpdateHeaderArrows(d);
    }
    SendMessageW(g_hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hList, nullptr, TRUE);
}

static void UpdateListCount(Document* d) {
    ListView_SetItemCountEx(g_hList, d ? (int)d->visible.size() : 0, 0);
    InvalidateRect(g_hList, nullptr, TRUE);
}

// ------------------------------------------------------------------ status & judul

static void InvalidateChrome() {
    RECT rc;
    GetClientRect(g_hMain, &rc);
    rc.bottom = g_rcTabbar.bottom;
    InvalidateRect(g_hMain, &rc, FALSE);
    if (g_sidebarVisible) InvalidateRect(g_hMain, &g_rcSidebar, FALSE);
    InvalidateRect(g_hMain, &g_rcStatus, FALSE);
}

static void UpdateTitle() {
    Document* d = Cur();
    std::wstring t = d ? (d->title + (d->dirty ? L" •" : L"") + L" — CSV Commander") : L"CSV Commander";
    SetWindowTextW(g_hMain, t.c_str());
}

// ------------------------------------------------------------------ tema

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static bool SystemPrefersDark() {
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return v == 0;
    return false;
}

static void ApplyTheme() {
    g_dark = (g_themePref < 0) ? SystemPrefersDark() : (g_themePref == 1);
    const Theme& t = g_dark ? THEME_DARK : THEME_LIGHT;
    CLR_BG = t.bg;         CLR_CHROME = t.chrome;   CLR_BORDER = t.border;
    CLR_TEXT = t.text;     CLR_MUTED = t.muted;     CLR_ACCENT = t.accent;
    CLR_HOT = t.hot;       CLR_ALTROW = t.altrow;   CLR_HDR_SEP = t.hdrSep;
    CLR_BADROW = t.badrow; CLR_WARNROW = t.warnrow; CLR_MATCH = t.match;
    CLR_DISABLED = t.disabled; CLR_CLOSEHOT = t.closeHot;

    if (g_brChrome) DeleteObject(g_brChrome);
    if (g_brBg) DeleteObject(g_brBg);
    g_brChrome = CreateSolidBrush(CLR_CHROME);
    g_brBg = CreateSolidBrush(CLR_BG);

    BOOL darkAttr = g_dark ? TRUE : FALSE;
    const wchar_t* sub = g_dark ? L"DarkMode_Explorer" : L"Explorer";
    if (g_hMain) DwmSetWindowAttribute(g_hMain, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkAttr, sizeof(darkAttr));
    if (g_hFind) DwmSetWindowAttribute(g_hFind, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkAttr, sizeof(darkAttr));
    if (g_hList) {
        SetWindowTheme(g_hList, sub, nullptr);
        ListView_SetBkColor(g_hList, CLR_BG);
        ListView_SetTextBkColor(g_hList, CLR_BG);
        ListView_SetTextColor(g_hList, CLR_TEXT);
    }
    if (g_hEdit) SetWindowTheme(g_hEdit, sub, nullptr);
    if (g_hSideList) SetWindowTheme(g_hSideList, sub, nullptr);
    if (g_hMain) {
        RedrawWindow(g_hMain, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    if (g_hFind) InvalidateRect(g_hFind, nullptr, TRUE);
}

// ------------------------------------------------------------------ pengaturan tersimpan (INI + MRU)

static std::wstring g_iniPath;

static void InitSettingsPath() {
    wchar_t dir[MAX_PATH] = {};
    ExpandEnvironmentStringsW(L"%APPDATA%\\CSVCommander", dir, MAX_PATH);
    CreateDirectoryW(dir, nullptr);
    g_iniPath = std::wstring(dir) + L"\\settings.ini";
}

static int IniInt(const wchar_t* k, int def) {
    return (int)GetPrivateProfileIntW(L"app", k, def, g_iniPath.c_str());
}
static void IniSetInt(const wchar_t* k, int v) {
    WritePrivateProfileStringW(L"app", k, std::to_wstring(v).c_str(), g_iniPath.c_str());
}
static std::wstring IniStr(const wchar_t* k) {
    wchar_t b[1024] = {};
    GetPrivateProfileStringW(L"app", k, L"", b, _countof(b), g_iniPath.c_str());
    return b;
}
static void IniSetStr(const wchar_t* k, const std::wstring& v) {
    WritePrivateProfileStringW(L"app", k, v.c_str(), g_iniPath.c_str());
}

static void AddMru(const std::wstring& path) {
    for (auto it = g_mru.begin(); it != g_mru.end();)
        if (StrCmpIW(it->c_str(), path.c_str()) == 0) it = g_mru.erase(it); else ++it;
    g_mru.insert(g_mru.begin(), path);
    if ((int)g_mru.size() > MRU_MAX) g_mru.resize(MRU_MAX);
}

static void LoadSettings() {
    InitSettingsPath();
    g_themePref = IniInt(L"theme", -1);
    g_sidebarVisible = IniInt(L"sidebar", 1) != 0;
    g_split = std::min(85, std::max(15, IniInt(L"split", 50))) / 100.0;
    g_folder = IniStr(L"folder");
    if (!g_folder.empty() && !PathFileExistsW(g_folder.c_str())) g_folder.clear();
    for (int i = 0; i < MRU_MAX; i++) {
        std::wstring f = IniStr((L"mru" + std::to_wstring(i)).c_str());
        if (!f.empty() && PathFileExistsW(f.c_str())) g_mru.push_back(f);
    }
}

static void SaveSettings() {
    if (g_iniPath.empty()) return;
    IniSetInt(L"theme", g_themePref);
    IniSetInt(L"sidebar", g_sidebarVisible ? 1 : 0);
    IniSetInt(L"split", (int)(g_split * 100));
    IniSetStr(L"folder", g_folder);
    WINDOWPLACEMENT wp = {sizeof(wp)};
    if (g_hMain && GetWindowPlacement(g_hMain, &wp)) {
        IniSetInt(L"wx", wp.rcNormalPosition.left);
        IniSetInt(L"wy", wp.rcNormalPosition.top);
        IniSetInt(L"ww", wp.rcNormalPosition.right - wp.rcNormalPosition.left);
        IniSetInt(L"wh", wp.rcNormalPosition.bottom - wp.rcNormalPosition.top);
        IniSetInt(L"wmax", wp.showCmd == SW_SHOWMAXIMIZED ? 1 : 0);
    }
    for (int i = 0; i < MRU_MAX; i++)
        IniSetStr((L"mru" + std::to_wstring(i)).c_str(),
                  i < (int)g_mru.size() ? g_mru[i] : L"");
}

// ------------------------------------------------------------------ sinkronisasi edit ⇄ parse

static void PullEditText(Document* d) {
    if (!d || !d->editTouched) return;
    int len = GetWindowTextLengthW(g_hEdit);
    d->rawText.resize(len);
    if (len) GetWindowTextW(g_hEdit, &d->rawText[0], len + 1);
    d->editTouched = false;
}

static void ReparseAndRefresh(Document* d) {
    if (!d) return;
    PullEditText(d);
    ParseCSV(d);
    RebuildVisible(d);
    RefreshListColumns(d);
    InvalidateChrome();
}

// ------------------------------------------------------------------ undo/redo

// snapshot sebelum operasi besar (Ganti Semua); dibatasi 20 langkah / ±64MB
static void PushUndo(Document* d) {
    PullEditText(d);
    d->undoStack.push_back(d->rawText);
    d->redoStack.clear();
    size_t total = 0;
    for (auto& s : d->undoStack) total += s.size();
    while (d->undoStack.size() > 1 &&
           (d->undoStack.size() > 20 || total * sizeof(wchar_t) > 64u * 1024 * 1024)) {
        total -= d->undoStack.front().size();
        d->undoStack.erase(d->undoStack.begin());
    }
}

// ganti seluruh teks dokumen + refresh UI bila dokumen aktif
static void ApplyRawText(Document* d, const std::wstring& t) {
    d->rawText = t;
    d->editTouched = false;
    d->parsedStale = true;
    d->dirty = true;
    if (d == Cur()) {
        g_suppressEditNotify = true;
        SetWindowTextW(g_hEdit, d->rawText.c_str());
        g_suppressEditNotify = false;
        ParseCSV(d);
        RebuildVisible(d);
        RefreshListColumns(d);
        InvalidateChrome();
        UpdateTitle();
    }
}

static void DoUndo() {
    Document* d = Cur();
    if (!d || d->loading) return;
    // ketikan biasa: pakai undo bawaan kontrol EDIT dulu
    if (GetFocus() == g_hEdit && SendMessageW(g_hEdit, EM_CANUNDO, 0, 0)) {
        SendMessageW(g_hEdit, EM_UNDO, 0, 0);
        return;
    }
    if (d->undoStack.empty()) return;
    PullEditText(d);
    d->redoStack.push_back(d->rawText);
    std::wstring t = std::move(d->undoStack.back());
    d->undoStack.pop_back();
    ApplyRawText(d, t);
}

static void DoRedo() {
    Document* d = Cur();
    if (!d || d->loading || d->redoStack.empty()) return;
    PullEditText(d);
    d->undoStack.push_back(d->rawText);
    std::wstring t = std::move(d->redoStack.back());
    d->redoStack.pop_back();
    ApplyRawText(d, t);
}

// ------------------------------------------------------------------ mutasi tabel
// Edit sel / operasi baris-kolom mengubah d->rows, lalu rawText diregenerasi
// dari rows (kebalikan arah ParseCSV). Panggil PushUndo() SEBELUM mutasi.

static std::wstring QuoteCell(const std::wstring& c, wchar_t delim);

static void RegenRawText(Document* d) {
    std::wstring out;
    out.reserve(d->rawText.size() + 64);
    for (const auto& row : d->rows) {
        int nc = std::max(d->numCols, (int)row.size());
        for (int i = 0; i < nc; i++) {
            if (i) out += d->delim;
            out += QuoteCell(i < (int)row.size() ? row[i] : std::wstring(), d->delim);
        }
        out += L"\r\n";
    }
    d->rawText = std::move(out);
    d->editTouched = false;
    d->parsedStale = false;
    d->dirty = true;
    if (d == Cur()) {
        g_suppressEditNotify = true;
        SetWindowTextW(g_hEdit, d->rawText.c_str());
        g_suppressEditNotify = false;
    }
}

// dipanggil setelah rows diubah; colsChanged=true bila struktur kolom berubah
static void FinishRowsMutation(Document* d, bool colsChanged) {
    d->numCols = 0;
    for (auto& r : d->rows) d->numCols = std::max(d->numCols, (int)r.size());
    RegenRawText(d);
    ValidateDoc(d);
    g_matchItem = g_matchSub = -1;
    if (colsChanged) {
        RebuildVisible(d);
        if (d == Cur()) RefreshListColumns(d);
    } else {
        RebuildVisible(d);
        if (d == Cur()) UpdateListCount(d);
    }
    if (d == Cur()) { UpdateTitle(); InvalidateChrome(); }
}

// ------------------------------------------------------------------ layout

// daftarkan ulang area tooltip mengikuti posisi tombol terkini
static void UpdateTooltips() {
    if (!g_hTip) return;
    static const struct { int id; const wchar_t* txt; } tips[] = {
        {BTN_SIDEBAR,    L"Tampilkan/sembunyikan sidebar"},
        {BTN_OPENFOLDER, L"Buka folder"},
        {BTN_OPENFILE,   L"Buka file (Ctrl+O)"},
        {BTN_SAVE,       L"Simpan (Ctrl+S)"},
        {BTN_EXPORT,     L"Export (Ctrl+E)"},
        {BTN_REPLACE,    L"Find && Replace (Ctrl+H)"},
        {BTN_NEWTAB,     L"Tab baru (Ctrl+N)"},
        {BTN_MODE_VIEW,  L"Tabel (Ctrl+1)"},
        {BTN_MODE_SPLIT, L"Edit + tabel berdampingan (Ctrl+2)"},
        {BTN_MODE_EDIT,  L"Teks CSV mentah (Ctrl+3)"},
        {BTN_THEME,      L"Ganti tema terang/gelap"},
        {BTN_DELIM,      L"Delimiter — klik untuk mengganti"},
        {BTN_ENC,        L"Encoding penyimpanan — klik untuk mengganti"},
    };
    for (auto& t : tips) {
        TOOLINFOW ti = {};
        ti.cbSize = sizeof(TOOLINFOW);
        ti.hwnd = g_hMain;
        ti.uId = (UINT_PTR)t.id;
        SendMessageW(g_hTip, TTM_DELTOOLW, 0, (LPARAM)&ti);
        const RECT* rc = nullptr;
        for (auto& h : g_hits) if (h.id == t.id) { rc = &h.rc; break; }
        if (!rc) continue;
        ti.uFlags = TTF_SUBCLASS;
        ti.rect = *rc;
        ti.lpszText = (LPWSTR)t.txt;
        SendMessageW(g_hTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    }
}

static void EndCellEdit(bool commit);   // didefinisikan di bagian cell edit

static void Layout() {
    if (g_cellEditActive) EndCellEdit(true);
    RECT rc;
    GetClientRect(g_hMain, &rc);
    int W = rc.right, H = rc.bottom;
    int tbH = S(48), tabH = S(38), stH = S(26);
    int side = g_sidebarVisible ? S(220) : 0;
    int Hc = H - stH;                                 // batas bawah konten (di atas status bar)

    g_rcToolbar = {0, 0, W, tbH};
    g_rcSidebar = {0, tbH, side, Hc};
    g_rcTabbar  = {side, tbH, W, tbH + tabH};
    g_rcContent = {side, tbH + tabH, W, Hc};
    g_rcStatus  = {0, Hc, W, H};
    g_rcSplit   = {0, 0, 0, 0};

    // kanan toolbar: tombol tema, lalu search box custom rounded
    int bs = S(32), by = (tbH - bs) / 2;
    RECT thRc = {W - S(10) - bs, by, W - S(10), by + bs};
    int sw = S(190), sbH = S(28);
    g_rcSearchBox = {thRc.left - S(8) - sw, (tbH - sbH) / 2, thRc.left - S(8), (tbH - sbH) / 2 + sbH};
    MoveWindow(g_hSearch, g_rcSearchBox.left + S(26), g_rcSearchBox.top + (sbH - S(16)) / 2,
               sw - S(34), S(16), TRUE);

    // sidebar list
    if (g_sidebarVisible) {
        MoveWindow(g_hSideList, S(6), tbH + S(34), side - S(12), Hc - tbH - S(40), TRUE);
        ShowWindow(g_hSideList, g_folder.empty() ? SW_HIDE : SW_SHOW);
    } else ShowWindow(g_hSideList, SW_HIDE);

    // konten (dokumen yang masih loading ditampilkan sebagai layar progres)
    bool haveDoc = Cur() != nullptr && !Cur()->loading;
    int cx = g_rcContent.left, cy = g_rcContent.top;
    int cw = g_rcContent.right - cx, ch = g_rcContent.bottom - cy;
    if (!haveDoc) {
        ShowWindow(g_hList, SW_HIDE);
        ShowWindow(g_hEdit, SW_HIDE);
    } else if (g_mode == MODE_VIEW) {
        MoveWindow(g_hList, cx, cy, cw, ch, TRUE);
        ShowWindow(g_hList, SW_SHOW);
        ShowWindow(g_hEdit, SW_HIDE);
    } else if (g_mode == MODE_EDIT) {
        MoveWindow(g_hEdit, cx, cy, cw, ch, TRUE);
        ShowWindow(g_hEdit, SW_SHOW);
        ShowWindow(g_hList, SW_HIDE);
    } else {
        // Split: sisakan celah S(6) milik jendela utama sebagai pegangan splitter
        int half = (int)(cw * g_split);
        half = std::min(std::max(half, S(120)), cw - S(120));
        MoveWindow(g_hEdit, cx, cy, half - S(3), ch, TRUE);
        MoveWindow(g_hList, cx + half + S(3), cy, cw - half - S(3), ch, TRUE);
        ShowWindow(g_hEdit, SW_SHOW);
        ShowWindow(g_hList, SW_SHOW);
        g_rcSplit = {cx + half - S(3), cy, cx + half + S(3), cy + ch};
    }

    // ---- hit rects
    g_hits.clear();
    int x = S(10);
    int ids[6] = {BTN_SIDEBAR, BTN_OPENFOLDER, BTN_OPENFILE, BTN_SAVE, BTN_EXPORT, BTN_REPLACE};
    for (int id : ids) {
        g_hits.push_back({id, {x, by, x + bs, by + bs}});
        x += bs + S(2);
    }
    g_hits.push_back({BTN_THEME, thRc});
    // segmented tengah
    int segW = S(68), segH = S(28), segX = (W - segW * 3) / 2, segY = (tbH - segH) / 2;
    g_hits.push_back({BTN_MODE_VIEW,  {segX,            segY, segX + segW,     segY + segH}});
    g_hits.push_back({BTN_MODE_SPLIT, {segX + segW,     segY, segX + segW * 2, segY + segH}});
    g_hits.push_back({BTN_MODE_EDIT,  {segX + segW * 2, segY, segX + segW * 3, segY + segH}});

    // status bar kanan: encoding lalu delimiter (zona teks klik-able)
    {
        Document* d = Cur();
        if (d && !d->loading) {
            int zy0 = g_rcStatus.top + S(2), zy1 = H - S(2);
            int ex1 = W - S(10), ex0 = ex1 - S(80);
            g_hits.push_back({BTN_ENC, {ex0, zy0, ex1, zy1}});
            int dx1 = ex0 - S(8), dx0 = dx1 - S(84);
            g_hits.push_back({BTN_DELIM, {dx0, zy0, dx1, zy1}});
        }
    }

    // tab
    int n = (int)g_docs.size();
    int avail = (g_rcTabbar.right - g_rcTabbar.left) - S(60);
    int tw = n ? std::min(std::max(avail / n, S(96)), S(190)) : 0;
    int tx = g_rcTabbar.left + S(8);
    for (int i = 0; i < n; i++) {
        RECT trc = {tx, g_rcTabbar.top + S(6), tx + tw, g_rcTabbar.bottom};
        int cs = S(18);
        RECT crc = {trc.right - S(24), (trc.top + trc.bottom - cs) / 2, trc.right - S(24) + cs, (trc.top + trc.bottom + cs) / 2};
        g_hits.push_back({HIT_CLOSE_BASE + i, crc});
        g_hits.push_back({HIT_TAB_BASE + i, trc});
        tx += tw + S(2);
    }
    RECT prc = {tx + S(4), g_rcTabbar.top + S(9), tx + S(4) + S(26), g_rcTabbar.top + S(9) + S(26)};
    g_hits.push_back({BTN_NEWTAB, prc});

    if (g_sidebarVisible && g_folder.empty()) {
        int bw = S(150), bh = S(32);
        RECT orc = {(side - bw) / 2, tbH + S(140), (side + bw) / 2, tbH + S(140) + bh};
        g_hits.push_back({BTN_OPENFOLDER_BIG, orc});
    }

    // layar kosong: daftar recent files bisa diklik
    if (!Cur() && !g_mru.empty()) {
        int cxm = (g_rcContent.left + g_rcContent.right) / 2;
        int cym = (g_rcContent.top + g_rcContent.bottom) / 2;
        int lw = std::min(S(420), cw - S(80));
        int ly = cym - S(10), lh = S(26);
        for (int i = 0; i < (int)g_mru.size(); i++) {
            RECT mrc = {cxm - lw / 2, ly, cxm + lw / 2, ly + lh};
            g_hits.push_back({HIT_MRU_BASE + i, mrc});
            ly += lh + S(2);
        }
    }
    UpdateTooltips();
    InvalidateRect(g_hMain, nullptr, FALSE);
}

// ------------------------------------------------------------------ painting

static void DrawGlyph(HDC dc, const RECT& rc, wchar_t glyph, COLORREF clr, HFONT font) {
    wchar_t s[2] = {glyph, 0};
    HFONT old = (HFONT)SelectObject(dc, font);
    SetTextColor(dc, clr);
    RECT r = rc;
    DrawTextW(dc, s, 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

static void FillRound(HDC dc, const RECT& rc, COLORREF fill, int rad) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, fill);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(br); DeleteObject(pen);
}

// header ListView digambar sendiri: latar hijau aksen + teks putih tebal.
// Header report-view memang selalu menempel di atas saat scroll (sticky).
static LRESULT CALLBACK HeaderProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_RBUTTONUP: {
        HDHITTESTINFO ht = {};
        ht.pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int i = (int)SendMessageW(h, HDM_HITTEST, 0, (LPARAM)&ht);
        if (i >= 1) PostMessageW(g_hMain, WM_APP_HDRMENU, (WPARAM)i, 0);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        HDHITTESTINFO ht = {};
        ht.pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int i = (int)SendMessageW(h, HDM_HITTEST, 0, (LPARAM)&ht);
        if (i >= 0 && (ht.flags & (HHT_ONDIVIDER | HHT_ONDIVOPEN))) {
            PostMessageW(g_hMain, WM_APP_HDRFIT, (WPARAM)i, 0);
            return 0;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc0 = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HDC dc = CreateCompatibleDC(dc0);
        HBITMAP bmp = CreateCompatibleBitmap(dc0, rc.right, rc.bottom);
        HGDIOBJ ob = SelectObject(dc, bmp);
        HBRUSH bg = CreateSolidBrush(CLR_ACCENT);
        FillRect(dc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);
        HPEN sep = CreatePen(PS_SOLID, 1, CLR_HDR_SEP);
        HGDIOBJ op = SelectObject(dc, sep);
        HFONT of = (HFONT)SelectObject(dc, g_fontUIBold);
        int n = Header_GetItemCount(h);
        wchar_t buf[256];
        for (int i = 0; i < n; i++) {
            RECT ir;
            if (!Header_GetItemRect(h, i, &ir)) continue;
            HDITEMW hi = {};
            hi.mask = HDI_TEXT | HDI_FORMAT;
            hi.pszText = buf;
            hi.cchTextMax = _countof(buf);
            buf[0] = 0;
            Header_GetItem(h, i, &hi);
            MoveToEx(dc, ir.right - 1, ir.top + S(5), nullptr);
            LineTo(dc, ir.right - 1, ir.bottom - S(5));
            RECT tr = {ir.left + S(8), ir.top, ir.right - S(8), ir.bottom};
            bool up = (hi.fmt & HDF_SORTUP) != 0, down = (hi.fmt & HDF_SORTDOWN) != 0;
            if (up || down) tr.right -= S(14);
            SetTextColor(dc, RGB(255, 255, 255));
            UINT align = (hi.fmt & HDF_RIGHT) ? DT_RIGHT : DT_LEFT;
            DrawTextW(dc, buf, -1, &tr, align | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (up || down) {
                RECT ar = {tr.right, ir.top, ir.right - S(4), ir.bottom};
                DrawGlyph(dc, ar, up ? 0xE70E : 0xE70D, RGB(255, 255, 255), g_fontIconSm);
            }
        }
        SelectObject(dc, of);
        SelectObject(dc, op);
        DeleteObject(sep);
        BitBlt(dc0, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

static const RECT* HitRect(int id) {
    for (auto& h : g_hits) if (h.id == id) return &h.rc;
    return nullptr;
}

static void PaintMain(HDC dc) {
    RECT rc;
    GetClientRect(g_hMain, &rc);
    SetBkMode(dc, TRANSPARENT);

    HBRUSH brChrome = CreateSolidBrush(CLR_CHROME);
    HBRUSH brBg = CreateSolidBrush(CLR_BG);
    HPEN penBorder = CreatePen(PS_SOLID, 1, CLR_BORDER);

    // toolbar + tabbar + sidebar + status bar bg
    FillRect(dc, &g_rcToolbar, brChrome);
    RECT tb = g_rcTabbar;
    FillRect(dc, &tb, brChrome);
    if (g_sidebarVisible) FillRect(dc, &g_rcSidebar, brChrome);
    FillRect(dc, &g_rcStatus, brChrome);

    // area konten (tampak saat tidak ada dokumen)
    RECT cnt = g_rcContent;
    FillRect(dc, &cnt, brBg);

    HGDIOBJ op = SelectObject(dc, penBorder);
    // garis bawah toolbar & tabbar, garis kanan sidebar, garis atas status bar
    MoveToEx(dc, 0, g_rcToolbar.bottom - 1, nullptr); LineTo(dc, rc.right, g_rcToolbar.bottom - 1);
    MoveToEx(dc, g_rcTabbar.left, g_rcTabbar.bottom - 1, nullptr); LineTo(dc, rc.right, g_rcTabbar.bottom - 1);
    if (g_sidebarVisible) {
        MoveToEx(dc, g_rcSidebar.right - 1, g_rcSidebar.top, nullptr);
        LineTo(dc, g_rcSidebar.right - 1, g_rcSidebar.bottom);
    }
    MoveToEx(dc, 0, g_rcStatus.top, nullptr); LineTo(dc, rc.right, g_rcStatus.top);
    SelectObject(dc, op);

    // pegangan splitter di mode Split (tiga titik di tengah celah)
    if (g_mode == MODE_SPLIT && Cur() && !Cur()->loading && g_rcSplit.right > g_rcSplit.left) {
        int mx = (g_rcSplit.left + g_rcSplit.right) / 2;
        int my = (g_rcSplit.top + g_rcSplit.bottom) / 2;
        HBRUSH grip = CreateSolidBrush(CLR_MUTED);
        for (int i = -1; i <= 1; i++) {
            RECT dot = {mx - S(1), my + i * S(8) - S(1), mx + S(1), my + i * S(8) + S(1)};
            FillRect(dc, &dot, grip);
        }
        DeleteObject(grip);
    }

    // ---- tombol ikon toolbar
    struct { int id; wchar_t g; } icons[] = {
        {BTN_SIDEBAR, 0xE700}, {BTN_OPENFOLDER, 0xE838}, {BTN_OPENFILE, 0xE8E5},
        {BTN_SAVE, 0xE74E}, {BTN_EXPORT, 0xE896}, {BTN_REPLACE, 0xE8AC},
    };
    for (auto& ic : icons) {
        const RECT* r = HitRect(ic.id);
        if (!r) continue;
        if (g_hotId == ic.id) FillRound(dc, *r, CLR_HOT, S(6));
        bool enabled = true;
        if (ic.id == BTN_SAVE) enabled = Cur() && Cur()->dirty && !Cur()->loading;
        else if (ic.id == BTN_EXPORT) enabled = Cur() && !Cur()->loading && !Cur()->rows.empty();
        else if (ic.id == BTN_REPLACE) enabled = Cur() && !Cur()->loading;
        DrawGlyph(dc, *r, ic.g, enabled ? CLR_TEXT : CLR_DISABLED, g_fontIcon);
    }

    // ---- tombol tema (kanan toolbar)
    if (const RECT* r = HitRect(BTN_THEME)) {
        if (g_hotId == BTN_THEME) FillRound(dc, *r, CLR_HOT, S(6));
        DrawGlyph(dc, *r, g_dark ? 0xE706 : 0xE708, CLR_TEXT, g_fontIcon);   // sun / moon
    }

    // ---- kontainer search box (rounded + ikon kaca pembesar)
    {
        FillRound(dc, g_rcSearchBox, CLR_BG, S(8));
        HPEN pb = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HGDIOBJ o = SelectObject(dc, pb);
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, g_rcSearchBox.left, g_rcSearchBox.top, g_rcSearchBox.right, g_rcSearchBox.bottom, S(8), S(8));
        SelectObject(dc, o);
        DeleteObject(pb);
        RECT gr = {g_rcSearchBox.left, g_rcSearchBox.top, g_rcSearchBox.left + S(26), g_rcSearchBox.bottom};
        DrawGlyph(dc, gr, 0xE721, CLR_MUTED, g_fontIconSm);
    }

    // ---- segmented View/Split/Edit
    {
        const RECT* r0 = HitRect(BTN_MODE_VIEW);
        const RECT* r2 = HitRect(BTN_MODE_EDIT);
        if (r0 && r2) {
            RECT box = {r0->left - S(4), r0->top - S(4), r2->right + S(4), r2->bottom + S(4)};
            FillRound(dc, box, CLR_BG, S(8));
            HPEN pb = CreatePen(PS_SOLID, 1, CLR_BORDER);
            HGDIOBJ o = SelectObject(dc, pb);
            SelectObject(dc, GetStockObject(NULL_BRUSH));
            RoundRect(dc, box.left, box.top, box.right, box.bottom, S(8), S(8));
            SelectObject(dc, o);
            DeleteObject(pb);
        }
        const wchar_t* labels[3] = {L"View", L"Split", L"Edit"};
        int ids2[3] = {BTN_MODE_VIEW, BTN_MODE_SPLIT, BTN_MODE_EDIT};
        for (int i = 0; i < 3; i++) {
            const RECT* r = HitRect(ids2[i]);
            if (!r) continue;
            bool sel = (g_mode == i);
            if (sel) FillRound(dc, *r, CLR_ACCENT, S(6));
            else if (g_hotId == ids2[i]) FillRound(dc, *r, CLR_HOT, S(6));
            HFONT of = (HFONT)SelectObject(dc, sel ? g_fontUIBold : g_fontUI);
            SetTextColor(dc, sel ? RGB(255, 255, 255) : CLR_TEXT);
            RECT tr = *r;
            DrawTextW(dc, labels[i], -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, of);
        }
    }

    // ---- status bar bawah
    {
        Document* d = Cur();
        HFONT of = (HFONT)SelectObject(dc, g_fontUI);
        if (d && !d->loading && !d->rows.empty()) {
            int total = (int)d->rows.size() - 1;
            int vis = (int)d->visible.size();
            std::wstring st;
            if (!d->filter.empty() && vis != total)
                st = std::to_wstring(vis) + L" / " + std::to_wstring(total) + L" baris · " + std::to_wstring(d->numCols) + L" kolom";
            else
                st = std::to_wstring(total) + L" baris · " + std::to_wstring(d->numCols) + L" kolom";
            int focused = ListView_GetNextItem(g_hList, -1, LVNI_FOCUSED);
            if (focused >= 0 && focused < (int)d->visible.size()) {
                st += L"  ·  Baris " + std::to_wstring(d->visible[focused]);
                if (g_selSub >= 1 && g_selSub <= d->numCols) {
                    std::wstring cn = CellAt(d, 0, g_selSub - 1);
                    if (cn.empty()) cn = L"Kolom " + std::to_wstring(g_selSub);
                    st += L", " + cn;
                }
            }
            if (d->badRows > 0)
                st += L"  ·  ⚠ " + std::to_wstring(d->badRows) + L" bermasalah";
            RECT lr = {S(12), g_rcStatus.top, g_rcStatus.right - S(200), g_rcStatus.bottom};
            SetTextColor(dc, CLR_MUTED);
            DrawTextW(dc, st.c_str(), -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // kanan: delimiter & encoding (klik untuk mengganti)
            const wchar_t* dn = L"koma";
            if (d->delim == L';') dn = L"titik koma";
            else if (d->delim == L'\t') dn = L"tab";
            else if (d->delim == L'|') dn = L"pipe";
            const wchar_t* en = d->enc == ENC_UTF8BOM ? L"UTF-8 BOM" : (d->enc == ENC_ANSI ? L"ANSI" : L"UTF-8");
            struct { int id; const wchar_t* txt; } zones[] = {{BTN_DELIM, dn}, {BTN_ENC, en}};
            for (auto& z : zones) {
                const RECT* r = HitRect(z.id);
                if (!r) continue;
                if (g_hotId == z.id) FillRound(dc, *r, CLR_HOT, S(4));
                SetTextColor(dc, g_hotId == z.id ? CLR_TEXT : CLR_MUTED);
                RECT zr = *r;
                DrawTextW(dc, z.txt, -1, &zr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        } else {
            RECT lr = {S(12), g_rcStatus.top, g_rcStatus.right - S(12), g_rcStatus.bottom};
            SetTextColor(dc, CLR_MUTED);
            DrawTextW(dc, L"Siap", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(dc, of);
    }

    // ---- tab
    for (int i = 0; i < (int)g_docs.size(); i++) {
        const RECT* r = HitRect(HIT_TAB_BASE + i);
        if (!r) continue;
        bool act = (i == g_cur);
        RECT trc = *r;
        if (act) {
            FillRound(dc, trc, CLR_BG, S(6));
            // indikator di ATAS tab — kalau di bawah menyatu dengan header tabel yang juga hijau
            RECT top = {trc.left + S(4), trc.top, trc.right - S(4), trc.top + S(2)};
            HBRUSH ab = CreateSolidBrush(CLR_ACCENT);
            FillRect(dc, &top, ab);
            DeleteObject(ab);
        } else if (g_hotId == HIT_TAB_BASE + i || g_hotId == HIT_CLOSE_BASE + i) {
            FillRound(dc, trc, CLR_HOT, S(6));
        }
        RECT lr = {trc.left + S(10), trc.top, trc.right - S(26), trc.bottom};
        HFONT of = (HFONT)SelectObject(dc, act ? g_fontUIBold : g_fontUI);
        SetTextColor(dc, act ? CLR_TEXT : CLR_MUTED);
        DrawTextW(dc, g_docs[i]->title.c_str(), -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, of);
        const RECT* cr = HitRect(HIT_CLOSE_BASE + i);
        if (cr) {
            // konvensi VS Code: tab dirty menampilkan titik; berubah jadi ✕ saat di-hover
            bool hotClose = (g_hotId == HIT_CLOSE_BASE + i);
            if (hotClose) FillRound(dc, *cr, CLR_CLOSEHOT, S(4));
            if (g_docs[i]->dirty && !hotClose) {
                int mx = (cr->left + cr->right) / 2, my = (cr->top + cr->bottom) / 2, rr = S(3);
                HBRUSH db = CreateSolidBrush(CLR_ACCENT);
                HPEN dp = CreatePen(PS_SOLID, 1, CLR_ACCENT);
                HGDIOBJ o1 = SelectObject(dc, db), o2 = SelectObject(dc, dp);
                Ellipse(dc, mx - rr, my - rr, mx + rr, my + rr);
                SelectObject(dc, o1); SelectObject(dc, o2);
                DeleteObject(db); DeleteObject(dp);
            } else {
                DrawGlyph(dc, *cr, 0xE711, CLR_MUTED, g_fontIconSm);
            }
        }
    }
    // tombol + tab baru
    if (const RECT* r = HitRect(BTN_NEWTAB)) {
        if (g_hotId == BTN_NEWTAB) FillRound(dc, *r, CLR_HOT, S(6));
        DrawGlyph(dc, *r, 0xE710, CLR_MUTED, g_fontIconSm);
    }

    // ---- sidebar
    if (g_sidebarVisible) {
        HFONT of = (HFONT)SelectObject(dc, g_fontUIBold);
        SetTextColor(dc, CLR_MUTED);
        RECT hr = {S(12), g_rcSidebar.top + S(8), g_rcSidebar.right - S(8), g_rcSidebar.top + S(30)};
        std::wstring head = g_folder.empty() ? L"EXPLORER" : L"EXPLORER — " + FileNameOf(g_folder);
        DrawTextW(dc, head.c_str(), -1, &hr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, of);
        if (g_folder.empty()) {
            RECT mr = {S(10), g_rcSidebar.top + S(80), g_rcSidebar.right - S(10), g_rcSidebar.top + S(130)};
            of = (HFONT)SelectObject(dc, g_fontUI);
            SetTextColor(dc, CLR_MUTED);
            DrawTextW(dc, L"Belum ada folder dibuka", -1, &mr, DT_CENTER | DT_WORDBREAK);
            SelectObject(dc, of);
            if (const RECT* r = HitRect(BTN_OPENFOLDER_BIG)) {
                FillRound(dc, *r, g_hotId == BTN_OPENFOLDER_BIG ? CLR_HOT : CLR_BG, S(6));
                HPEN pb = CreatePen(PS_SOLID, 1, CLR_BORDER);
                HGDIOBJ o = SelectObject(dc, pb);
                SelectObject(dc, GetStockObject(NULL_BRUSH));
                RoundRect(dc, r->left, r->top, r->right, r->bottom, S(6), S(6));
                SelectObject(dc, o);
                DeleteObject(pb);
                of = (HFONT)SelectObject(dc, g_fontUI);
                SetTextColor(dc, CLR_TEXT);
                RECT br2 = *r;
                DrawTextW(dc, L"Buka folder…", -1, &br2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(dc, of);
            }
        }
    }

    // ---- empty state / layar loading konten
    Document* dcur = Cur();
    if (!dcur) {
        int cxm = (g_rcContent.left + g_rcContent.right) / 2;
        int cym = (g_rcContent.top + g_rcContent.bottom) / 2;
        RECT gr = {g_rcContent.left, cym - S(150), g_rcContent.right, cym - S(80)};
        DrawGlyph(dc, gr, 0xE9F9, CLR_BORDER, g_fontIconBig);   // glyph tabel besar
        RECT mr = {g_rcContent.left, cym - S(72), g_rcContent.right, cym - S(44)};
        HFONT of = (HFONT)SelectObject(dc, g_fontUI);
        SetTextColor(dc, CLR_MUTED);
        DrawTextW(dc, L"Tarik file CSV ke sini, atau tekan Ctrl+O untuk membuka file",
                  -1, &mr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (!g_mru.empty()) {
            RECT hr2 = {g_rcContent.left, cym - S(34), g_rcContent.right, cym - S(12)};
            SelectObject(dc, g_fontUIBold);
            DrawTextW(dc, L"TERAKHIR DIBUKA", -1, &hr2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, g_fontUI);
            for (int i = 0; i < (int)g_mru.size(); i++) {
                const RECT* r = HitRect(HIT_MRU_BASE + i);
                if (!r) continue;
                if (g_hotId == HIT_MRU_BASE + i) FillRound(dc, *r, CLR_HOT, S(6));
                RECT ir = {r->left + S(8), r->top, r->left + S(28), r->bottom};
                DrawGlyph(dc, ir, 0xE7C3, CLR_ACCENT, g_fontIconSm);
                std::wstring nm = FileNameOf(g_mru[i]);
                RECT nr = {r->left + S(32), r->top, r->left + S(32) + S(160), r->bottom};
                SetTextColor(dc, CLR_TEXT);
                DrawTextW(dc, nm.c_str(), -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                RECT pr2 = {nr.right + S(10), r->top, r->right - S(8), r->bottom};
                SetTextColor(dc, CLR_MUTED);
                DrawTextW(dc, g_mru[i].c_str(), -1, &pr2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX);
            }
        }
        SelectObject(dc, of);
    } else if (dcur->loading) {
        int cxm = (g_rcContent.left + g_rcContent.right) / 2;
        int cym = (g_rcContent.top + g_rcContent.bottom) / 2;
        std::wstring msg = L"Memuat " + dcur->title + L"…  " + std::to_wstring(dcur->loadPercent) + L"%";
        RECT mr = {g_rcContent.left, cym - S(40), g_rcContent.right, cym - S(10)};
        HFONT of = (HFONT)SelectObject(dc, g_fontUI);
        SetTextColor(dc, CLR_MUTED);
        DrawTextW(dc, msg.c_str(), -1, &mr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, of);
        int bw = S(320), bh = S(6);
        RECT track = {cxm - bw / 2, cym, cxm + bw / 2, cym + bh};
        FillRound(dc, track, CLR_BORDER, S(3));
        int fw = bw * std::max(0, std::min(100, dcur->loadPercent)) / 100;
        if (fw > S(6)) {
            RECT fill = {track.left, track.top, track.left + fw, track.bottom};
            FillRound(dc, fill, CLR_ACCENT, S(3));
        }
    }

    DeleteObject(brChrome);
    DeleteObject(brBg);
    DeleteObject(penBorder);
}

// ------------------------------------------------------------------ dokumen: buka/tutup/simpan

static void UpdateSaveEnabled() { InvalidateChrome(); }

static void SwitchToDoc(int i) {
    if (g_cellEditActive) EndCellEdit(true);   // commit ke dokumen lama sebelum pindah
    Document* prev = Cur();
    if (prev) PullEditText(prev);
    g_cur = i;
    g_matchItem = g_matchSub = -1;
    g_selSub = 1;
    Document* d = Cur();
    g_suppressEditNotify = true;
    SetWindowTextW(g_hEdit, d ? d->rawText.c_str() : L"");
    g_suppressEditNotify = false;
    SetWindowTextW(g_hSearch, d ? d->filter.c_str() : L"");
    if (d) {
        if (d->parsedStale) { ParseCSV(d); RebuildVisible(d); }
        RefreshListColumns(d);
    } else {
        RefreshListColumns(nullptr);
    }
    Layout();
    UpdateTitle();
}

static void AddDoc(Document* d) {
    g_docs.push_back(d);
    SwitchToDoc((int)g_docs.size() - 1);
}

static void OpenPath(const std::wstring& path) {
    // sudah terbuka? pindah tab saja
    for (int i = 0; i < (int)g_docs.size(); i++) {
        if (StrCmpIW(g_docs[i]->path.c_str(), path.c_str()) == 0) { SwitchToDoc(i); return; }
    }
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
        MessageBoxW(g_hMain, (L"Gagal membuka file:\n" + path).c_str(), L"CSV Commander", MB_ICONERROR);
        return;
    }
    ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    AddMru(path);

    Document* d = new Document();
    d->id = ++g_docIdSeq;
    d->path = path;
    d->title = FileNameOf(path);

    if (sz < 1000000) {
        // file kecil: sinkron, tanpa layar loading
        if (!ReadFileText(path, d->rawText, d->enc)) {
            delete d;
            MessageBoxW(g_hMain, (L"Gagal membuka file:\n" + path).c_str(), L"CSV Commander", MB_ICONERROR);
            return;
        }
        NormalizeCRLF(d->rawText);
        AddDoc(d);
        return;
    }

    // file besar: baca + parse di background thread, UI menampilkan progres
    d->loading = true;
    d->parsedStale = false;
    AddDoc(d);
    HWND hwnd = g_hMain;
    int docId = d->id;
    std::wstring p = path;
    std::thread([hwnd, docId, p]() {
        Document* tmp = new Document();
        auto prog = [&](int pct) { PostMessageW(hwnd, WM_APP_PROG, (WPARAM)docId, (LPARAM)pct); };
        if (!ReadFileText(p, tmp->rawText, tmp->enc, prog)) {
            tmp->loadFailed = true;
        } else {
            prog(60); NormalizeCRLF(tmp->rawText);
            prog(68); ParseCSV(tmp);
            prog(96); RebuildVisible(tmp);
            prog(100);
        }
        PostMessageW(hwnd, WM_APP_DONE, (WPARAM)docId, (LPARAM)tmp);
    }).detach();
}

static void NewDoc() {
    Document* d = new Document();
    d->id = ++g_docIdSeq;
    d->title = L"tanpa-judul.csv";
    d->rawText = L"kolom1,kolom2,kolom3\r\n";
    AddDoc(d);
}

static bool SaveDoc(Document* d, bool saveAs) {
    if (!d || d->loading) return false;
    PullEditText(d);
    std::wstring path = d->path;
    if (saveAs || path.empty()) {
        wchar_t buf[MAX_PATH] = {};
        lstrcpynW(buf, d->title.c_str(), MAX_PATH);
        OPENFILENAMEW ofn = {sizeof(ofn)};
        ofn.hwndOwner = g_hMain;
        ofn.lpstrFilter = L"File CSV (*.csv)\0*.csv\0Semua file (*.*)\0*.*\0";
        ofn.lpstrFile = buf;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"csv";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return false;
        path = buf;
    }
    if (!WriteFileText(path, d->rawText, d->enc)) {
        MessageBoxW(g_hMain, L"Gagal menyimpan file.", L"CSV Commander", MB_ICONERROR);
        return false;
    }
    d->path = path;
    d->title = FileNameOf(path);
    d->dirty = false;
    AddMru(path);
    UpdateTitle();
    Layout();
    return true;
}

// ------------------------------------------------------------------ export

static std::wstring QuoteCell(const std::wstring& c, wchar_t delim) {
    bool need = false;
    for (wchar_t ch : c)
        if (ch == delim || ch == L'"' || ch == L'\n' || ch == L'\r') { need = true; break; }
    if (!need) return c;
    std::wstring r = L"\"";
    for (wchar_t ch : c) { if (ch == L'"') r += L"\"\""; else r += ch; }
    r += L"\"";
    return r;
}

// viewOnly = hanya baris yang tampil (terfilter + tersortir), selain itu semua baris
static void ExportDoc(Document* d, bool viewOnly) {
    if (!d || d->loading) return;
    PullEditText(d);
    if (d->parsedStale) { ParseCSV(d); RebuildVisible(d); RefreshListColumns(d); }
    if (d->rows.empty()) return;

    wchar_t buf[MAX_PATH] = {};
    std::wstring base = d->title;
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base = base.substr(0, dot);
    base += viewOnly ? L"-filtered.csv" : L"-export.csv";
    lstrcpynW(buf, base.c_str(), MAX_PATH);
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = L"File CSV (*.csv)\0*.csv\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"csv";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    std::wstring out;
    auto appendRow = [&](const std::vector<std::wstring>& row) {
        for (int i = 0; i < d->numCols; i++) {
            if (i) out += d->delim;
            out += QuoteCell(i < (int)row.size() ? row[i] : std::wstring(), d->delim);
        }
        out += L"\r\n";
    };
    appendRow(d->rows[0]);
    int n = 0;
    if (viewOnly) { for (int idx : d->visible) { appendRow(d->rows[idx]); n++; } }
    else          { for (size_t r = 1; r < d->rows.size(); r++) { appendRow(d->rows[r]); n++; } }

    if (!WriteFileText(buf, out, d->enc)) {
        MessageBoxW(g_hMain, L"Gagal menulis file export.", L"CSV Commander", MB_ICONERROR);
        return;
    }
    std::wstring m = std::to_wstring(n) + L" baris data diekspor ke:\n" + buf;
    MessageBoxW(g_hMain, m.c_str(), L"CSV Commander", MB_ICONINFORMATION);
}

// return false = batal
static bool CloseDoc(int i) {
    if (i < 0 || i >= (int)g_docs.size()) return true;
    Document* d = g_docs[i];
    if (i == g_cur) PullEditText(d);
    if (d->dirty) {
        std::wstring msg = L"Simpan perubahan pada \"" + d->title + L"\"?";
        int r = MessageBoxW(g_hMain, msg.c_str(), L"CSV Commander", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return false;
        if (r == IDYES && !SaveDoc(d, false)) return false;
    }
    delete d;
    g_docs.erase(g_docs.begin() + i);
    int next = g_cur;
    if (i < g_cur) next = g_cur - 1;
    else if (i == g_cur) next = std::min(i, (int)g_docs.size() - 1);
    g_cur = -1;                      // paksa refresh penuh
    SwitchToDoc(next);
    return true;
}

// ------------------------------------------------------------------ folder sidebar

static void ScanFolder() {
    g_folderFiles.clear();
    if (g_folder.empty()) return;
    const wchar_t* pats[] = {L"\\*.csv", L"\\*.tsv"};
    for (auto p : pats) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((g_folder + p).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                g_folderFiles.push_back(fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(g_folderFiles.begin(), g_folderFiles.end(),
              [](const std::wstring& a, const std::wstring& b) { return StrCmpLogicalW(a.c_str(), b.c_str()) < 0; });
    SendMessageW(g_hSideList, LB_RESETCONTENT, 0, 0);
    for (auto& f : g_folderFiles)
        SendMessageW(g_hSideList, LB_ADDSTRING, 0, (LPARAM)f.c_str());
}

static bool PickFolder(std::wstring& out) {
    IFileDialog* pfd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
        return false;
    DWORD opts = 0;
    pfd->GetOptions(&opts);
    pfd->SetOptions(opts | FOS_PICKFOLDERS);
    bool ok = false;
    if (SUCCEEDED(pfd->Show(g_hMain))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(pfd->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; CoTaskMemFree(p); ok = true; }
            it->Release();
        }
    }
    pfd->Release();
    return ok;
}

static void DoOpenFolder() {
    std::wstring f;
    if (PickFolder(f)) {
        g_folder = f;
        ScanFolder();
        if (!g_sidebarVisible) g_sidebarVisible = true;
        Layout();
    }
}

static void DoOpenFileDialog() {
    wchar_t buf[MAX_PATH * 8] = {};
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = L"File CSV/TSV (*.csv;*.tsv)\0*.csv;*.tsv\0Semua file (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = _countof(buf);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return;
    // multi-select: dir\0file1\0file2\0\0 — atau path tunggal
    std::wstring dir = buf;
    const wchar_t* p = buf + dir.size() + 1;
    if (*p == 0) OpenPath(dir);
    else {
        while (*p) {
            std::wstring f = p;
            OpenPath(dir + L"\\" + f);
            p += f.size() + 1;
        }
    }
}

// ------------------------------------------------------------------ mode & aksi

static void SetMode(int m) {
    if (m == g_mode) return;
    Document* d = Cur();
    if (d && g_mode != MODE_VIEW && m != MODE_EDIT) {
        // keluar dari edit → pastikan parse terbaru
        PullEditText(d);
        if (d->parsedStale) { ParseCSV(d); RebuildVisible(d); RefreshListColumns(d); }
    }
    g_mode = m;
    Layout();
    if (d) {
        if (m == MODE_VIEW) SetFocus(g_hList);
        else SetFocus(g_hEdit);
    }
}

// ------------------------------------------------------------------ edit sel in-place

static LRESULT CALLBACK CellEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR);
static void BeginCellEdit(int item, int sub);

static void EnsureCellEditCtl() {
    if (g_hCellEdit) return;
    g_hCellEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL,
                                  0, 0, 0, 0, g_hList, (HMENU)IDC_CELLEDIT, g_hInst, nullptr);
    SetWindowSubclass(g_hCellEdit, CellEditProc, 1, 0);
    SendMessageW(g_hCellEdit, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
}

static void EndCellEdit(bool commit) {
    if (!g_cellEditActive) return;
    g_cellEditActive = false;
    bool hadFocus = (GetFocus() == g_hCellEdit);
    std::wstring txt = GetWndText(g_hCellEdit);
    ShowWindow(g_hCellEdit, SW_HIDE);
    Document* d = Cur();
    if (d && commit) {
        if (g_editHdrCol >= 0) {
            int c = g_editHdrCol;
            if (c >= 0 && c < d->numCols && txt != CellAt(d, 0, c)) {
                PushUndo(d);
                if (d->rows.empty()) d->rows.push_back({});
                auto& hdr = d->rows[0];
                while ((int)hdr.size() <= c) hdr.push_back(L"");
                hdr[c] = txt;
                FinishRowsMutation(d, true);
            }
        } else if (g_editItem >= 0 && g_editItem < (int)d->visible.size() && g_editSub >= 1) {
            int row = d->visible[g_editItem];
            int c = g_editSub - 1;
            if (txt != CellAt(d, row, c)) {
                PushUndo(d);
                auto& r = d->rows[row];
                while ((int)r.size() <= c) r.push_back(L"");
                r[c] = txt;
                // sengaja tanpa RebuildVisible agar baris tidak melompat saat mengedit
                d->numCols = std::max(d->numCols, (int)r.size());
                RegenRawText(d);
                ValidateDoc(d);
                UpdateTitle();
                InvalidateChrome();
                InvalidateRect(g_hList, nullptr, FALSE);
            }
        }
    }
    g_editItem = g_editSub = g_editHdrCol = -1;
    if (hadFocus) SetFocus(g_hList);
}

static void BeginCellEdit(int item, int sub) {
    Document* d = Cur();
    if (!d || d->loading || sub < 1 || sub > d->numCols ||
        item < 0 || item >= (int)d->visible.size()) return;
    if (g_cellEditActive) EndCellEdit(true);
    EnsureCellEditCtl();
    ListView_EnsureVisible(g_hList, item, FALSE);
    RECT rc;
    ListView_GetSubItemRect(g_hList, item, sub, LVIR_LABEL, &rc);
    g_editItem = item; g_editSub = sub; g_editHdrCol = -1;
    g_selSub = sub;
    g_cellEditActive = true;
    SetWindowTextW(g_hCellEdit, CellAt(d, d->visible[item], sub - 1).c_str());
    MoveWindow(g_hCellEdit, rc.left + S(2), rc.top + 1,
               std::max((int)(rc.right - rc.left) - S(4), S(40)), rc.bottom - rc.top - 1, TRUE);
    ShowWindow(g_hCellEdit, SW_SHOW);
    SetFocus(g_hCellEdit);
    SendMessageW(g_hCellEdit, EM_SETSEL, 0, -1);
}

// rename header kolom: pakai editor yang sama, diposisikan di atas item header
static void BeginHeaderEdit(int col) {
    Document* d = Cur();
    if (!d || d->loading || col < 0 || col >= d->numCols) return;
    if (g_cellEditActive) EndCellEdit(true);
    EnsureCellEditCtl();
    HWND hdr = ListView_GetHeader(g_hList);
    RECT rc;
    if (!Header_GetItemRect(hdr, col + 1, &rc)) return;
    MapWindowPoints(hdr, g_hList, (POINT*)&rc, 2);
    g_editHdrCol = col; g_editItem = g_editSub = -1;
    g_cellEditActive = true;
    SetWindowTextW(g_hCellEdit, CellAt(d, 0, col).c_str());
    MoveWindow(g_hCellEdit, rc.left + S(2), rc.top + S(2),
               std::max((int)(rc.right - rc.left) - S(4), S(40)), rc.bottom - rc.top - S(4), TRUE);
    ShowWindow(g_hCellEdit, SW_SHOW);
    SetFocus(g_hCellEdit);
    SendMessageW(g_hCellEdit, EM_SETSEL, 0, -1);
}

static LRESULT CALLBACK CellEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (wp == VK_RETURN) { EndCellEdit(true); return 0; }
        if (wp == VK_ESCAPE) { EndCellEdit(false); return 0; }
        if (wp == VK_TAB) {
            bool shift = GetKeyState(VK_SHIFT) < 0;
            bool wasHdr = g_editHdrCol >= 0;
            int item = g_editItem, sub = g_editSub;
            EndCellEdit(true);
            Document* d = Cur();
            if (!d || wasHdr) return 0;
            int nc = d->numCols;
            if (!shift) { if (++sub > nc) { sub = 1; item++; } }
            else        { if (--sub < 1)  { sub = nc; item--; } }
            if (item >= 0 && item < (int)d->visible.size()) BeginCellEdit(item, sub);
            return 0;
        }
        break;
    case WM_CHAR:
        if (wp == L'\r' || wp == L'\n' || wp == L'\t' || wp == 27) return 0;   // cegah beep
        break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

// subclass ListView: warna editor sel + notifikasi kill-focus editor
static LRESULT CALLBACK ListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_THEMECHANGED) {
        // SetWindowTheme (ganti tema) bisa mereset warna listview → set ulang sesuai palet
        LRESULT r = DefSubclassProc(h, msg, wp, lp);
        ListView_SetBkColor(h, CLR_BG);
        ListView_SetTextBkColor(h, CLR_BG);
        ListView_SetTextColor(h, CLR_TEXT);
        InvalidateRect(h, nullptr, TRUE);
        return r;
    }
    if (msg == WM_CTLCOLOREDIT) {
        HDC dcx = (HDC)wp;
        SetBkColor(dcx, CLR_BG);
        SetTextColor(dcx, CLR_TEXT);
        return (LRESULT)g_brBg;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDC_CELLEDIT && HIWORD(wp) == EN_KILLFOCUS) {
        if (g_cellEditActive) EndCellEdit(true);
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

// ------------------------------------------------------------------ operasi baris & kolom

static int g_menuCol = -1;   // kolom target context menu (indeks data, 0-based)

static int FocusedDataRow(Document* d) {
    int f = ListView_GetNextItem(g_hList, -1, LVNI_FOCUSED);
    if (f >= 0 && f < (int)d->visible.size()) return d->visible[f];
    return -1;
}

static std::vector<int> SelectedDataRows(Document* d) {
    std::vector<int> out;
    int i = -1;
    while ((i = ListView_GetNextItem(g_hList, i, LVNI_SELECTED)) >= 0)
        if (i < (int)d->visible.size()) out.push_back(d->visible[i]);
    std::sort(out.begin(), out.end());
    return out;
}

static void RowInsert(Document* d, bool below) {
    if (!d || d->loading || d->rows.empty()) return;
    int at = FocusedDataRow(d);
    int pos = (at < 0) ? (int)d->rows.size() : (below ? at + 1 : at);
    pos = std::min(std::max(pos, 1), (int)d->rows.size());
    PushUndo(d);
    d->rows.insert(d->rows.begin() + pos, std::vector<std::wstring>(std::max(d->numCols, 1)));
    FinishRowsMutation(d, false);
}

static void RowDuplicate(Document* d) {
    if (!d || d->loading) return;
    std::vector<int> sel = SelectedDataRows(d);
    if (sel.empty()) return;
    PushUndo(d);
    for (int i = (int)sel.size() - 1; i >= 0; i--) {
        std::vector<std::wstring> copy = d->rows[sel[i]];
        d->rows.insert(d->rows.begin() + sel[i] + 1, std::move(copy));
    }
    FinishRowsMutation(d, false);
}

static void RowDelete(Document* d) {
    if (!d || d->loading) return;
    std::vector<int> sel = SelectedDataRows(d);
    if (sel.empty()) return;
    PushUndo(d);
    for (int i = (int)sel.size() - 1; i >= 0; i--)
        d->rows.erase(d->rows.begin() + sel[i]);
    FinishRowsMutation(d, false);
}

static void ColInsert(Document* d, int col, bool right) {
    if (!d || d->loading || d->rows.empty()) return;
    int pos = right ? col + 1 : col;
    pos = std::min(std::max(pos, 0), d->numCols);
    PushUndo(d);
    for (size_t r = 0; r < d->rows.size(); r++) {
        auto& row = d->rows[r];
        while ((int)row.size() < pos) row.push_back(L"");
        row.insert(row.begin() + pos,
                   r == 0 ? L"Kolom " + std::to_wstring(pos + 1) : std::wstring());
    }
    FinishRowsMutation(d, true);
}

static void ColDelete(Document* d, int col) {
    if (!d || d->loading || col < 0 || col >= d->numCols) return;
    if (d->numCols <= 1) return;   // jangan sisakan tabel tanpa kolom
    PushUndo(d);
    for (auto& row : d->rows)
        if (col < (int)row.size()) row.erase(row.begin() + col);
    FinishRowsMutation(d, true);
}

// salin baris terseleksi ke clipboard (tab-separated, ramah paste ke Excel)
static void CopySelection(Document* d) {
    if (!d || d->loading) return;
    std::vector<int> sel = SelectedDataRows(d);
    if (sel.empty()) return;
    std::wstring out;
    for (int r : sel) {
        const auto& row = d->rows[r];
        for (int c = 0; c < d->numCols; c++) {
            if (c) out += L'\t';
            if (c < (int)row.size()) out += row[c];
        }
        out += L"\r\n";
    }
    if (!OpenClipboard(g_hMain)) return;
    EmptyClipboard();
    size_t bytes = (out.size() + 1) * sizeof(wchar_t);
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hg) {
        void* p = GlobalLock(hg);
        memcpy(p, out.c_str(), bytes);
        GlobalUnlock(hg);
        SetClipboardData(CF_UNICODETEXT, hg);
    }
    CloseClipboard();
}

// ------------------------------------------------------------------ statistik kolom

static void ShowColStats(Document* d, int col) {
    if (!d || d->loading || col < 0 || col >= d->numCols) return;
    std::wstring name = CellAt(d, 0, col);
    if (name.empty()) name = L"Kolom " + std::to_wstring(col + 1);

    int total = (int)d->visible.size(), nonEmpty = 0, numCount = 0;
    double mn = 0, mx = 0, sum = 0;
    std::unordered_set<std::wstring> uniq;
    bool trackUniq = total <= 200000;   // batasi memori untuk file raksasa
    for (int idx : d->visible) {
        const std::wstring& s = CellAt(d, idx, col);
        if (s.empty()) continue;
        nonEmpty++;
        if (trackUniq) uniq.insert(s);
        double v;
        if (NumericVal(s, v)) {
            if (!numCount) { mn = mx = v; } else { mn = std::min(mn, v); mx = std::max(mx, v); }
            sum += v;
            numCount++;
        }
    }
    auto fmt = [](double v) {
        wchar_t b[64];
        if (v == (long long)v && v > -1e15 && v < 1e15)
            swprintf_s(b, L"%lld", (long long)v);
        else
            swprintf_s(b, L"%.6g", v);
        return std::wstring(b);
    };
    std::wstring m = L"Kolom: " + name + L"\n";
    if (!d->filter.empty()) m += L"(dihitung dari " + std::to_wstring(total) + L" baris yang tampil)\n";
    m += L"\nTerisi: " + std::to_wstring(nonEmpty) + L" dari " + std::to_wstring(total) + L" baris";
    m += L"\nKosong: " + std::to_wstring(total - nonEmpty);
    m += L"\nNilai unik: " + (trackUniq ? std::to_wstring(uniq.size()) : std::wstring(L"— (terlalu banyak baris)"));
    if (numCount > 0) {
        m += L"\n\nNumerik: " + std::to_wstring(numCount) + L" nilai";
        m += L"\nMin: " + fmt(mn) + L"\nMax: " + fmt(mx);
        m += L"\nJumlah: " + fmt(sum);
        m += L"\nRata-rata: " + fmt(sum / numCount);
    }
    MessageBoxW(g_hMain, m.c_str(), (L"Statistik — " + name).c_str(), MB_ICONINFORMATION);
}

// ------------------------------------------------------------------ cari sel (F3)

static void FindNextCell(int dir) {
    Document* d = Cur();
    if (!d || d->loading || d->visible.empty() || d->numCols <= 0) return;
    std::wstring q = GetWndText(g_hSearch);
    if (q.empty()) { SetFocus(g_hSearch); return; }
    if (g_mode == MODE_EDIT) SetMode(MODE_SPLIT);   // hasil disorot di tabel
    int n = (int)d->visible.size(), nc = d->numCols;
    int ci = g_matchItem, cs = g_matchSub;
    if (ci < 0) {
        ci = ListView_GetNextItem(g_hList, -1, LVNI_FOCUSED);
        if (ci < 0) ci = 0;
        cs = dir > 0 ? 0 : nc + 1;
    }
    long long totalCells = (long long)n * nc;
    for (long long step = 0; step < totalCells; step++) {
        cs += dir;
        if (cs > nc) { cs = 1; if (++ci >= n) ci = 0; }
        if (cs < 1)  { cs = nc; if (--ci < 0) ci = n - 1; }
        const std::wstring& cell = CellAt(d, d->visible[ci], cs - 1);
        if (!cell.empty() && StrStrIW(cell.c_str(), q.c_str())) {
            int old = g_matchItem;
            g_matchItem = ci; g_matchSub = cs; g_selSub = cs;
            ListView_SetItemState(g_hList, -1, 0, LVIS_SELECTED);
            ListView_SetItemState(g_hList, ci, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_hList, ci, FALSE);
            if (old >= 0) ListView_RedrawItems(g_hList, old, old);
            ListView_RedrawItems(g_hList, ci, ci);
            SetFocus(g_hList);
            InvalidateChrome();
            return;
        }
    }
    MessageBeep(MB_OK);
}

// ------------------------------------------------------------------ autofit kolom

static void AutofitColumn(int hi) {
    Document* d = Cur();
    if (!d || d->loading) return;
    if (hi == 0) { ListView_SetColumnWidth(g_hList, 0, S(52)); return; }
    int c = hi - 1;
    if (c < 0 || c >= d->numCols) return;
    HDC dc = GetDC(g_hList);
    HFONT of = (HFONT)SelectObject(dc, g_fontUI);
    SIZE sz;
    std::wstring name = CellAt(d, 0, c);
    if (name.empty()) name = L"Kolom " + std::to_wstring(c + 1);
    GetTextExtentPoint32W(dc, name.c_str(), (int)name.size(), &sz);
    int w = sz.cx + S(30);
    int sample = std::min((int)d->visible.size(), 500);   // sampling agar tetap cepat di file besar
    for (int i = 0; i < sample; i++) {
        const std::wstring& s = CellAt(d, d->visible[i], c);
        if (s.empty()) continue;
        GetTextExtentPoint32W(dc, s.c_str(), (int)std::min<size_t>(s.size(), 200), &sz);
        w = std::max(w, (int)sz.cx + S(18));
    }
    SelectObject(dc, of);
    ReleaseDC(g_hList, dc);
    ListView_SetColumnWidth(g_hList, hi, std::min(w, S(520)));
}

// ------------------------------------------------------------------ context menu tabel

static void AppendColMenu(HMENU m, Document* d, int col) {
    std::wstring cn = CellAt(d, 0, col);
    if (cn.empty()) cn = L"Kolom " + std::to_wstring(col + 1);
    AppendMenuW(m, MF_STRING, IDM_COL_STATS,  (L"Statistik \"" + cn + L"\"…").c_str());
    AppendMenuW(m, MF_STRING, IDM_COL_RENAME, (L"Ganti nama \"" + cn + L"\"").c_str());
    AppendMenuW(m, MF_STRING, IDM_COL_AUTOFIT, L"Sesuaikan lebar kolom");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_COL_INS_LEFT,  L"Sisip kolom di kiri");
    AppendMenuW(m, MF_STRING, IDM_COL_INS_RIGHT, L"Sisip kolom di kanan");
    AppendMenuW(m, MF_STRING | (d->numCols > 1 ? 0 : MF_GRAYED), IDM_COL_DEL,
                (L"Hapus kolom \"" + cn + L"\"").c_str());
}

static void ShowListContextMenu(POINT scr) {
    Document* d = Cur();
    if (!d || d->loading || d->rows.empty()) return;
    POINT pt = scr;
    ScreenToClient(g_hList, &pt);
    LVHITTESTINFO ht = {};
    ht.pt = pt;
    ListView_SubItemHitTest(g_hList, &ht);
    bool onRow = ht.iItem >= 0;
    int col = ht.iSubItem - 1;
    if (onRow) {
        if (!(ListView_GetItemState(g_hList, ht.iItem, LVIS_SELECTED) & LVIS_SELECTED)) {
            ListView_SetItemState(g_hList, -1, 0, LVIS_SELECTED);
            ListView_SetItemState(g_hList, ht.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
        if (ht.iSubItem >= 1) g_selSub = ht.iSubItem;
    }
    int selCount = ListView_GetSelectedCount(g_hList);
    HMENU m = CreatePopupMenu();
    if (onRow) {
        std::wstring del = selCount > 1 ? L"Hapus " + std::to_wstring(selCount) + L" baris\tDel"
                                        : L"Hapus baris\tDel";
        std::wstring dup = selCount > 1 ? L"Duplikat " + std::to_wstring(selCount) + L" baris"
                                        : L"Duplikat baris";
        AppendMenuW(m, MF_STRING, IDM_ROW_INS_ABOVE, L"Sisip baris di atas");
        AppendMenuW(m, MF_STRING, IDM_ROW_INS_BELOW, L"Sisip baris di bawah");
        AppendMenuW(m, MF_STRING, IDM_ROW_DUP, dup.c_str());
        AppendMenuW(m, MF_STRING, IDM_ROW_DEL, del.c_str());
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, IDM_COPY, L"Salin baris\tCtrl+C");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    }
    if (col >= 0 && col < d->numCols) {
        g_menuCol = col;
        AppendColMenu(m, d, col);
    }
    if (GetMenuItemCount(m) == 0) { DestroyMenu(m); return; }
    TrackPopupMenu(m, TPM_RIGHTBUTTON, scr.x, scr.y, 0, g_hMain, nullptr);
    DestroyMenu(m);
}

static void ShowHeaderContextMenu(int headerItem) {
    Document* d = Cur();
    if (!d || d->loading || headerItem < 1) return;
    g_menuCol = headerItem - 1;
    if (g_menuCol >= d->numCols) return;
    HMENU m = CreatePopupMenu();
    AppendColMenu(m, d, g_menuCol);
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hMain, nullptr);
    DestroyMenu(m);
}

// ------------------------------------------------------------------ find & replace

static std::wstring EscapeRegex(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) {
        if (wcschr(L"\\^$.|?*+()[]{}", c)) r += L'\\';
        r += c;
    }
    return r;
}

static std::wstring EscapeRepl(const std::wstring& s) {   // $ literal di mode simple
    std::wstring r;
    for (wchar_t c : s) { if (c == L'$') r += L"$$"; else r += c; }
    return r;
}

struct ReplaceOutcome { int repl = 0; int lines = 0; std::wstring text; };

// proses per baris teks agar preview bisa menunjukkan nomor baris; regex tidak lintas baris
static bool ComputeReplace(const std::wstring& src, const std::wstring& pat, const std::wstring& rep,
                           bool useRegex, bool matchCase, ReplaceOutcome& out,
                           std::vector<std::wstring>* samples, std::wstring& err) {
    if (pat.empty()) { err = L"Pola pencarian masih kosong."; return false; }
    std::wstring rxs = useRegex ? pat : EscapeRegex(pat);
    std::wstring rps = useRegex ? rep : EscapeRepl(rep);
    auto flags = std::regex_constants::ECMAScript;
    if (!matchCase) flags |= std::regex_constants::icase;
    std::wregex re;
    try { re.assign(rxs, flags); }
    catch (const std::regex_error&) { err = L"Pola regex tidak valid."; return false; }

    out.text.reserve(src.size());
    size_t pos = 0;
    int lineNo = 1;
    while (pos <= src.size()) {
        size_t nl = src.find(L'\n', pos);
        std::wstring line = src.substr(pos, nl == std::wstring::npos ? src.size() - pos : nl - pos);
        bool hadCR = !line.empty() && line.back() == L'\r';
        if (hadCR) line.pop_back();
        int cnt = 0;
        for (auto it = std::wsregex_iterator(line.begin(), line.end(), re);
             it != std::wsregex_iterator(); ++it) cnt++;
        if (cnt) {
            std::wstring rl = std::regex_replace(line, re, rps);
            out.repl += cnt;
            out.lines++;
            if (samples && samples->size() < 200) {
                std::wstring a = line, b = rl;
                if (a.size() > 70) a = a.substr(0, 70) + L"…";
                if (b.size() > 70) b = b.substr(0, 70) + L"…";
                samples->push_back(L"baris " + std::to_wstring(lineNo) + L":   " + a + L"   →   " + b);
            }
            line = rl;
        }
        out.text += line;
        if (hadCR) out.text += L'\r';
        if (nl == std::wstring::npos) break;
        out.text += L'\n';
        pos = nl + 1;
        lineNo++;
    }
    return true;
}

static LRESULT CALLBACK FindProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(h, &rc);
        FillRect((HDC)wp, &rc, g_brChrome);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dcx = (HDC)wp;
        SetBkMode(dcx, TRANSPARENT);
        SetTextColor(dcx, CLR_TEXT);
        return (LRESULT)g_brChrome;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dcx = (HDC)wp;
        SetBkColor(dcx, CLR_BG);
        SetTextColor(dcx, CLR_TEXT);
        return (LRESULT)g_brBg;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDCANCEL) { ShowWindow(h, SW_HIDE); return 0; }
        if (id == IDOK || id == FR_PREVIEW || id == FR_APPLY) {
            Document* d = Cur();
            if (!d || d->loading) return 0;
            PullEditText(d);
            ReplaceOutcome ro;
            std::vector<std::wstring> samples;
            std::wstring err;
            bool useRx = IsDlgButtonChecked(h, FR_REGEX) == BST_CHECKED;
            bool mc = IsDlgButtonChecked(h, FR_CASE) == BST_CHECKED;
            if (!ComputeReplace(d->rawText, GetWndText(GetDlgItem(h, FR_FIND)),
                                GetWndText(GetDlgItem(h, FR_REP)), useRx, mc, ro, &samples, err)) {
                SetWindowTextW(GetDlgItem(h, FR_INFO), err.c_str());
                return 0;
            }
            HWND lb = GetDlgItem(h, FR_LIST);
            SendMessageW(lb, LB_RESETCONTENT, 0, 0);
            for (auto& s : samples) SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)s.c_str());
            std::wstring info;
            if (id == FR_APPLY) {
                if (ro.repl) {
                    PushUndo(d);
                    ApplyRawText(d, ro.text);
                    info = std::to_wstring(ro.repl) + L" penggantian diterapkan di " +
                           std::to_wstring(ro.lines) + L" baris.  (Ctrl+Z untuk membatalkan)";
                } else info = L"Tidak ada yang cocok — tidak ada perubahan.";
            } else {
                info = L"Preview: " + std::to_wstring(ro.repl) + L" kecocokan di " +
                       std::to_wstring(ro.lines) + L" baris. Belum diterapkan.";
            }
            SetWindowTextW(GetDlgItem(h, FR_INFO), info.c_str());
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void AddFindCtl(int id, const wchar_t* cls, const wchar_t* text, DWORD style, DWORD ex,
                       int x, int y, int w, int hh) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, hh, g_hFind, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
}

static void ShowFindDialog() {
    Document* d = Cur();
    if (!d || d->loading) return;
    if (!g_hFind) {
        WNDCLASSW wcf = {};
        wcf.lpfnWndProc = FindProc;
        wcf.hInstance = g_hInst;
        wcf.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcf.hbrBackground = nullptr;   // latar diisi WM_ERASEBKGND sesuai tema aktif
        wcf.lpszClassName = L"CSVFindReplace";
        RegisterClassW(&wcf);
        int cw = S(620), chh = S(430);
        DWORD st = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        RECT wr = {0, 0, cw, chh};
        AdjustWindowRectEx(&wr, st, FALSE, 0);
        g_hFind = CreateWindowExW(0, L"CSVFindReplace", L"Find & Replace",
                                  st, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                                  g_hMain, nullptr, g_hInst, nullptr);
        BOOL darkAttr = g_dark ? TRUE : FALSE;
        DwmSetWindowAttribute(g_hFind, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkAttr, sizeof(darkAttr));
        HICON icoSm = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        SendMessageW(g_hFind, WM_SETICON, ICON_SMALL, (LPARAM)icoSm);
        SendMessageW(g_hFind, WM_SETICON, ICON_BIG,
                     (LPARAM)LoadImageW(g_hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
        AddFindCtl(0, L"STATIC", L"Cari:", 0, 0, S(14), S(16), S(92), S(18));
        AddFindCtl(FR_FIND, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
                   S(112), S(12), cw - S(126), S(24));
        AddFindCtl(0, L"STATIC", L"Ganti dengan:", 0, 0, S(14), S(48), S(92), S(18));
        AddFindCtl(FR_REP, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
                   S(112), S(44), cw - S(126), S(24));
        AddFindCtl(FR_REGEX, L"BUTTON", L"Regex", BS_AUTOCHECKBOX | WS_TABSTOP, 0,
                   S(112), S(78), S(70), S(20));
        AddFindCtl(FR_CASE, L"BUTTON", L"Match case", BS_AUTOCHECKBOX | WS_TABSTOP, 0,
                   S(192), S(78), S(110), S(20));
        AddFindCtl(FR_PREVIEW, L"BUTTON", L"Preview", BS_DEFPUSHBUTTON | WS_TABSTOP, 0,
                   S(112), S(106), S(92), S(28));
        AddFindCtl(FR_APPLY, L"BUTTON", L"Ganti Semua", BS_PUSHBUTTON | WS_TABSTOP, 0,
                   S(212), S(106), S(112), S(28));
        AddFindCtl(FR_INFO, L"STATIC", L"", 0, 0, S(14), S(142), cw - S(28), S(18));
        AddFindCtl(FR_LIST, L"LISTBOX", L"", LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
                   WS_EX_CLIENTEDGE, S(14), S(166), cw - S(28), chh - S(180));
        SendMessageW(GetDlgItem(g_hFind, FR_LIST), LB_SETHORIZONTALEXTENT, 3000, 0);
    }
    RECT mr, fr2;
    GetWindowRect(g_hMain, &mr);
    GetWindowRect(g_hFind, &fr2);
    int x = mr.left + ((mr.right - mr.left) - (fr2.right - fr2.left)) / 2;
    int y = mr.top + ((mr.bottom - mr.top) - (fr2.bottom - fr2.top)) / 3;
    SetWindowPos(g_hFind, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(g_hFind, SW_SHOW);
    HWND hf = GetDlgItem(g_hFind, FR_FIND);
    SetFocus(hf);
    SendMessageW(hf, EM_SETSEL, 0, -1);
}

static void OnButton(int id) {
    switch (id) {
    case BTN_SIDEBAR: g_sidebarVisible = !g_sidebarVisible; Layout(); break;
    case BTN_OPENFOLDER: case BTN_OPENFOLDER_BIG: DoOpenFolder(); break;
    case BTN_OPENFILE: DoOpenFileDialog(); break;
    case BTN_SAVE: SaveDoc(Cur(), false); break;
    case BTN_EXPORT: {
        Document* d = Cur();
        if (!d || d->loading || d->rows.empty()) break;
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, IDM_EXPORT_ALL, L"Export semua baris…");
        std::wstring lbl = L"Export tampilan saat ini (" + std::to_wstring(d->visible.size()) + L" baris)…";
        bool viewDiff = !d->filter.empty() || d->sortCol >= 0;
        AppendMenuW(m, MF_STRING | (viewDiff ? 0 : MF_GRAYED), IDM_EXPORT_FILTERED, lbl.c_str());
        const RECT* r = HitRect(BTN_EXPORT);
        POINT pt = {r ? r->left : 0, r ? r->bottom : 0};
        ClientToScreen(g_hMain, &pt);
        TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_hMain, nullptr);
        DestroyMenu(m);
        break;
    }
    case BTN_REPLACE: ShowFindDialog(); break;
    case BTN_MODE_VIEW: SetMode(MODE_VIEW); break;
    case BTN_MODE_SPLIT: SetMode(MODE_SPLIT); break;
    case BTN_MODE_EDIT: SetMode(MODE_EDIT); break;
    case BTN_NEWTAB: NewDoc(); break;
    case BTN_THEME:
        g_themePref = g_dark ? 0 : 1;
        ApplyTheme();
        break;
    case BTN_DELIM: {
        Document* d = Cur();
        if (!d || d->loading) break;
        HMENU m = CreatePopupMenu();
        struct { int id; const wchar_t* txt; wchar_t ch; } items[] = {
            {IDM_DELIM_COMMA, L"Koma  ( , )", L','},
            {IDM_DELIM_SEMI,  L"Titik koma  ( ; )", L';'},
            {IDM_DELIM_TAB,   L"Tab", L'\t'},
            {IDM_DELIM_PIPE,  L"Pipe  ( | )", L'|'},
        };
        for (auto& it : items)
            AppendMenuW(m, MF_STRING | (d->delim == it.ch ? MF_CHECKED : 0), it.id, it.txt);
        const RECT* r = HitRect(BTN_DELIM);
        POINT pt = {r ? r->left : 0, r ? r->top : 0};
        ClientToScreen(g_hMain, &pt);
        TrackPopupMenu(m, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hMain, nullptr);
        DestroyMenu(m);
        break;
    }
    case BTN_ENC: {
        Document* d = Cur();
        if (!d || d->loading) break;
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING | (d->enc == ENC_UTF8 ? MF_CHECKED : 0), IDM_ENC_UTF8, L"UTF-8");
        AppendMenuW(m, MF_STRING | (d->enc == ENC_UTF8BOM ? MF_CHECKED : 0), IDM_ENC_UTF8BOM, L"UTF-8 dengan BOM");
        AppendMenuW(m, MF_STRING | (d->enc == ENC_ANSI ? MF_CHECKED : 0), IDM_ENC_ANSI, L"ANSI (codepage sistem)");
        const RECT* r = HitRect(BTN_ENC);
        POINT pt = {r ? r->left : 0, r ? r->top : 0};
        ClientToScreen(g_hMain, &pt);
        TrackPopupMenu(m, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hMain, nullptr);
        DestroyMenu(m);
        break;
    }
    default:
        if (id >= HIT_CLOSE_BASE && id < HIT_CLOSE_BASE + (int)g_docs.size()) CloseDoc(id - HIT_CLOSE_BASE);
        else if (id >= HIT_TAB_BASE && id < HIT_TAB_BASE + (int)g_docs.size()) SwitchToDoc(id - HIT_TAB_BASE);
        else if (id >= HIT_MRU_BASE && id < HIT_MRU_BASE + (int)g_mru.size()) OpenPath(g_mru[id - HIT_MRU_BASE]);
        break;
    }
}

static int HitTest(POINT pt) {
    for (auto& h : g_hits) if (PtInRc(h.rc, pt)) return h.id;
    return -1;
}

static void ApplyFilterFromSearch() {
    Document* d = Cur();
    if (!d) return;
    int len = GetWindowTextLengthW(g_hSearch);
    std::wstring f(len, 0);
    if (len) GetWindowTextW(g_hSearch, &f[0], len + 1);
    if (f == d->filter) return;
    d->filter = f;
    g_matchItem = g_matchSub = -1;
    RebuildVisible(d);
    UpdateListCount(d);
    InvalidateChrome();
}

// ------------------------------------------------------------------ font & dpi

static void CreateFonts() {
    if (g_fontUI) {
        DeleteObject(g_fontUI); DeleteObject(g_fontUIBold); DeleteObject(g_fontMono);
        DeleteObject(g_fontIcon); DeleteObject(g_fontIconSm); DeleteObject(g_fontIconBig);
    }
    g_fontUI      = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontUIBold  = CreateFontW(-S(13), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontMono    = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");
    g_fontIcon    = CreateFontW(-S(15), 0, 0, 0, FW_NORMAL,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
    g_fontIconSm  = CreateFontW(-S(11), 0, 0, 0, FW_NORMAL,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
    g_fontIconBig = CreateFontW(-S(56), 0, 0, 0, FW_NORMAL,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
}

static void ApplyFonts() {
    SendMessageW(g_hList, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
    SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_fontMono, TRUE);
    SendMessageW(g_hSearch, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
    SendMessageW(g_hSideList, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
    if (g_hCellEdit) SendMessageW(g_hCellEdit, WM_SETFONT, (WPARAM)g_fontUI, TRUE);
}

// subclass search box: Enter lompat ke kecocokan berikut, Escape bersihkan filter
static LRESULT CALLBACK SearchProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) { FindNextCell(1); return 0; }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        SetWindowTextW(h, L"");
        ApplyFilterFromSearch();
        if (Cur()) SetFocus(g_hList);
        return 0;
    }
    if (msg == WM_CHAR && (wp == L'\r' || wp == 27)) return 0;   // cegah beep
    return DefSubclassProc(h, msg, wp, lp);
}

// ------------------------------------------------------------------ window proc

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hMain = hwnd;
        g_hList = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)IDC_LIST, g_hInst, nullptr);
        ListView_SetExtendedListViewStyle(g_hList,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES |
            LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP);
        SetWindowTheme(g_hList, L"Explorer", nullptr);
        SetWindowSubclass(ListView_GetHeader(g_hList), HeaderProc, 1, 0);
        SetWindowSubclass(g_hList, ListProc, 1, 0);

        g_hEdit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_EDIT, g_hInst, nullptr);
        SendMessageW(g_hEdit, EM_SETLIMITTEXT, 0x7FFFFFFE, 0);

        g_hSearch = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_SEARCH, g_hInst, nullptr);
        SendMessageW(g_hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Filter…  Ctrl+F · F3 lompat");
        SetWindowSubclass(g_hSearch, SearchProc, 1, 0);

        g_hSideList = CreateWindowExW(0, L"LISTBOX", L"",
            WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            0, 0, 0, 0, hwnd, (HMENU)IDC_SIDELIST, g_hInst, nullptr);

        g_hTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            0, 0, 0, 0, hwnd, nullptr, g_hInst, nullptr);

        CreateFonts();
        ApplyFonts();
        ApplyTheme();
        DragAcceptFiles(hwnd, TRUE);
        return 0;
    }
    case WM_SIZE:
        Layout();
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mi = (MINMAXINFO*)lp;
        mi->ptMinTrackSize = {S(560), S(320)};
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETFOCUS:
        if (Cur()) SetFocus(g_mode == MODE_VIEW ? g_hList : g_hEdit);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ ob = SelectObject(mem, bmp);
        PaintMain(mem);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (g_dragSplit) {
            int cx = g_rcContent.left, cw = g_rcContent.right - cx;
            if (cw > 0) {
                double sp = (double)(pt.x - cx) / cw;
                g_split = std::min(0.85, std::max(0.15, sp));
                Layout();
            }
            return 0;
        }
        int id = HitTest(pt);
        if (id != g_hotId) { g_hotId = id; InvalidateChrome(); }
        if (!g_trackingMouse) {
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            g_trackingMouse = true;
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_trackingMouse = false;
        if (g_hotId != -1) { g_hotId = -1; InvalidateChrome(); }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT cp;
            GetCursorPos(&cp);
            ScreenToClient(hwnd, &cp);
            if (g_mode == MODE_SPLIT && g_rcSplit.right > g_rcSplit.left && PtInRc(g_rcSplit, cp)) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
            if (g_hotId != -1) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
        }
        break;
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (g_mode == MODE_SPLIT && g_rcSplit.right > g_rcSplit.left && PtInRc(g_rcSplit, pt)) {
            g_dragSplit = true;
            SetCapture(hwnd);
            return 0;
        }
        int id = HitTest(pt);
        if (id != -1) OnButton(id);
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_dragSplit) { g_dragSplit = false; ReleaseCapture(); }
        return 0;
    case WM_MBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int id = HitTest(pt);
        if (id >= HIT_TAB_BASE && id < HIT_TAB_BASE + (int)g_docs.size()) CloseDoc(id - HIT_TAB_BASE);
        return 0;
    }
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wp;
        UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; i++) {
            wchar_t buf[MAX_PATH];
            if (DragQueryFileW(drop, i, buf, MAX_PATH)) OpenPath(buf);
        }
        DragFinish(drop);
        return 0;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, CLR_CHROME);
        SetTextColor(dc, CLR_TEXT);
        return (LRESULT)g_brChrome;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, CLR_BG);
        SetTextColor(dc, CLR_TEXT);
        return (LRESULT)g_brBg;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)lp;
        if (mi->CtlID == IDC_SIDELIST) { mi->itemHeight = (UINT)S(26); return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
        if (di->CtlID != IDC_SIDELIST) break;
        HDC dcd = di->hDC;
        RECT r = di->rcItem;
        FillRect(dcd, &r, g_brChrome);
        if ((int)di->itemID == -1) return TRUE;
        bool sel = (di->itemState & ODS_SELECTED) != 0;
        SetBkMode(dcd, TRANSPARENT);
        if (sel) {
            RECT rr = {r.left + S(2), r.top + S(1), r.right - S(2), r.bottom - S(1)};
            FillRound(dcd, rr, CLR_HOT, S(5));
            RECT bar = {r.left + S(2), r.top + S(4), r.left + S(4), r.bottom - S(4)};
            HBRUSH ab = CreateSolidBrush(CLR_ACCENT);
            FillRect(dcd, &bar, ab);
            DeleteObject(ab);
        }
        int len = (int)SendMessageW(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0);
        std::wstring txt(len > 0 ? len : 0, 0);
        if (len > 0) SendMessageW(di->hwndItem, LB_GETTEXT, di->itemID, (LPARAM)&txt[0]);
        RECT ir = {r.left + S(8), r.top, r.left + S(28), r.bottom};
        DrawGlyph(dcd, ir, 0xE7C3, sel ? CLR_ACCENT : CLR_MUTED, g_fontIconSm);
        RECT tr = {r.left + S(32), r.top, r.right - S(6), r.bottom};
        HFONT of = (HFONT)SelectObject(dcd, sel ? g_fontUIBold : g_fontUI);
        SetTextColor(dcd, CLR_TEXT);
        DrawTextW(dcd, txt.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dcd, of);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == IDC_EDIT && code == EN_CHANGE && !g_suppressEditNotify) {
            Document* d = Cur();
            if (d) {
                d->editTouched = true;
                d->parsedStale = true;
                if (!d->dirty) { d->dirty = true; UpdateTitle(); InvalidateChrome(); }
                if (g_mode == MODE_SPLIT) SetTimer(hwnd, TIMER_REPARSE, 400, nullptr);
            }
            return 0;
        }
        if (id == IDC_SEARCH && code == EN_CHANGE) {
            SetTimer(hwnd, TIMER_FILTER, 250, nullptr);
            return 0;
        }
        if (id == IDC_SIDELIST && code == LBN_SELCHANGE) {
            int sel = (int)SendMessageW(g_hSideList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_folderFiles.size())
                OpenPath(g_folder + L"\\" + g_folderFiles[sel]);
            return 0;
        }
        switch (id) {
        case IDM_OPEN: DoOpenFileDialog(); return 0;
        case IDM_NEW: NewDoc(); return 0;
        case IDM_SAVE: SaveDoc(Cur(), false); return 0;
        case IDM_SAVEAS: SaveDoc(Cur(), true); return 0;
        case IDM_CLOSETAB: if (g_cur >= 0) CloseDoc(g_cur); return 0;
        case IDM_FIND: SetFocus(g_hSearch); SendMessageW(g_hSearch, EM_SETSEL, 0, -1); return 0;
        case IDM_MODE_VIEW: SetMode(MODE_VIEW); return 0;
        case IDM_MODE_SPLIT: SetMode(MODE_SPLIT); return 0;
        case IDM_MODE_EDIT: SetMode(MODE_EDIT); return 0;
        case IDM_REPLACE: ShowFindDialog(); return 0;
        case IDM_UNDO: DoUndo(); return 0;
        case IDM_REDO: DoRedo(); return 0;
        case IDM_EXPORT_ALL: ExportDoc(Cur(), false); return 0;
        case IDM_EXPORT_FILTERED: ExportDoc(Cur(), true); return 0;
        case IDM_FINDNEXT: FindNextCell(1); return 0;
        case IDM_FINDPREV: FindNextCell(-1); return 0;
        case IDM_EDITCELL: {
            int f = ListView_GetNextItem(g_hList, -1, LVNI_FOCUSED);
            if (f >= 0) BeginCellEdit(f, std::max(1, g_selSub));
            return 0;
        }
        case IDM_ESCAPE:
            if (g_cellEditActive) { EndCellEdit(false); return 0; }
            if (GetWindowTextLengthW(g_hSearch) > 0) {
                SetWindowTextW(g_hSearch, L"");
                ApplyFilterFromSearch();
                if (Cur()) SetFocus(g_mode == MODE_VIEW ? g_hList : g_hEdit);
            }
            return 0;
        case IDM_COPY: {
            HWND f = GetFocus();
            wchar_t cls[16] = {};
            if (f) GetClassNameW(f, cls, 16);
            if (StrCmpIW(cls, L"EDIT") == 0) SendMessageW(f, WM_COPY, 0, 0);
            else CopySelection(Cur());
            return 0;
        }
        case IDM_ROW_INS_ABOVE: RowInsert(Cur(), false); return 0;
        case IDM_ROW_INS_BELOW: RowInsert(Cur(), true); return 0;
        case IDM_ROW_DUP: RowDuplicate(Cur()); return 0;
        case IDM_ROW_DEL: RowDelete(Cur()); return 0;
        case IDM_COL_INS_LEFT: ColInsert(Cur(), g_menuCol, false); return 0;
        case IDM_COL_INS_RIGHT: ColInsert(Cur(), g_menuCol, true); return 0;
        case IDM_COL_DEL: ColDelete(Cur(), g_menuCol); return 0;
        case IDM_COL_RENAME: BeginHeaderEdit(g_menuCol); return 0;
        case IDM_COL_STATS: ShowColStats(Cur(), g_menuCol); return 0;
        case IDM_COL_AUTOFIT: AutofitColumn(g_menuCol + 1); return 0;
        case IDM_DELIM_COMMA: case IDM_DELIM_SEMI: case IDM_DELIM_TAB: case IDM_DELIM_PIPE: {
            Document* d = Cur();
            if (!d || d->loading) return 0;
            wchar_t nd = id == IDM_DELIM_COMMA ? L',' : id == IDM_DELIM_SEMI ? L';'
                       : id == IDM_DELIM_TAB ? L'\t' : L'|';
            if (nd != d->delim || !d->delimManual) {
                PullEditText(d);
                d->delim = nd;
                d->delimManual = true;
                g_matchItem = g_matchSub = -1;
                ParseCSV(d);
                RebuildVisible(d);
                RefreshListColumns(d);
                InvalidateChrome();
            }
            return 0;
        }
        case IDM_ENC_UTF8: case IDM_ENC_UTF8BOM: case IDM_ENC_ANSI: {
            Document* d = Cur();
            if (!d || d->loading) return 0;
            int ne = id == IDM_ENC_UTF8 ? ENC_UTF8 : id == IDM_ENC_UTF8BOM ? ENC_UTF8BOM : ENC_ANSI;
            if (ne != d->enc) {
                d->enc = ne;
                d->dirty = true;
                UpdateTitle();
                InvalidateChrome();
            }
            return 0;
        }
        case IDM_SELALL: {
            HWND f = GetFocus();
            wchar_t cls[16] = {};
            if (f) GetClassNameW(f, cls, 16);
            if (StrCmpIW(cls, L"EDIT") == 0) SendMessageW(f, EM_SETSEL, 0, -1);
            return 0;
        }
        case IDM_NEXTTAB:
            if (!g_docs.empty()) SwitchToDoc((g_cur + 1) % (int)g_docs.size());
            return 0;
        case IDM_PREVTAB:
            if (!g_docs.empty()) SwitchToDoc((g_cur - 1 + (int)g_docs.size()) % (int)g_docs.size());
            return 0;
        }
        break;
    }
    case WM_APP_PROG: {
        for (auto* d : g_docs)
            if (d->id == (int)wp) {
                d->loadPercent = (int)lp;
                if (d == Cur()) InvalidateRect(hwnd, &g_rcContent, FALSE);
                break;
            }
        return 0;
    }
    case WM_APP_DONE: {
        Document* tmp = (Document*)lp;
        Document* d = nullptr;
        int idx = -1;
        for (int i = 0; i < (int)g_docs.size(); i++)
            if (g_docs[i]->id == (int)wp) { d = g_docs[i]; idx = i; break; }
        if (!d) { delete tmp; return 0; }   // tab sudah ditutup saat masih loading
        if (tmp->loadFailed) {
            d->loading = false;
            MessageBoxW(hwnd, (L"Gagal membuka file:\n" + d->path).c_str(), L"CSV Commander", MB_ICONERROR);
            CloseDoc(idx);
            delete tmp;
            return 0;
        }
        d->rawText  = std::move(tmp->rawText);
        d->enc  = tmp->enc;
        d->delim    = tmp->delim;
        d->numCols  = tmp->numCols;
        d->rows     = std::move(tmp->rows);
        d->visible  = std::move(tmp->visible);
        d->colTypes = std::move(tmp->colTypes);
        d->rowFlags = std::move(tmp->rowFlags);
        d->badRows  = tmp->badRows;
        d->loading = false;
        d->parsedStale = false;
        delete tmp;
        if (d == Cur()) {
            g_suppressEditNotify = true;
            SetWindowTextW(g_hEdit, d->rawText.c_str());
            g_suppressEditNotify = false;
            RefreshListColumns(d);
            Layout();
            UpdateTitle();
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == TIMER_REPARSE) {
            KillTimer(hwnd, TIMER_REPARSE);
            ReparseAndRefresh(Cur());
        } else if (wp == TIMER_FILTER) {
            KillTimer(hwnd, TIMER_FILTER);
            ApplyFilterFromSearch();
        }
        return 0;
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lp;
        if (nm->hwndFrom == g_hList) {
            Document* d = Cur();
            if (nm->code == LVN_GETDISPINFOW && d) {
                NMLVDISPINFOW* di = (NMLVDISPINFOW*)lp;
                int item = di->item.iItem;
                if (!(di->item.mask & LVIF_TEXT) || item < 0 || item >= (int)d->visible.size()) return 0;
                int row = d->visible[item];
                if (di->item.iSubItem == 0) {
                    wchar_t buf[16];
                    _itow_s(row, buf, 10);
                    lstrcpynW(di->item.pszText, buf, di->item.cchTextMax);
                } else {
                    lstrcpynW(di->item.pszText, CellAt(d, row, di->item.iSubItem - 1).c_str(), di->item.cchTextMax);
                }
                return 0;
            }
            if (nm->code == LVN_COLUMNCLICK && d) {
                if (g_cellEditActive) EndCellEdit(true);
                int sub = ((NMLISTVIEW*)lp)->iSubItem;
                if (sub == 0) { d->sortCol = -1; d->sortAsc = true; }
                else {
                    int col = sub - 1;
                    if (d->sortCol == col) {
                        if (d->sortAsc) d->sortAsc = false;
                        else { d->sortCol = -1; d->sortAsc = true; }   // klik ke-3: reset
                    } else { d->sortCol = col; d->sortAsc = true; }
                }
                g_matchItem = g_matchSub = -1;
                RebuildVisible(d);
                UpdateListCount(d);
                UpdateHeaderArrows(d);
                return 0;
            }
            if (nm->code == NM_CLICK && d) {
                NMITEMACTIVATE* ia = (NMITEMACTIVATE*)lp;
                if (ia->iSubItem >= 1) g_selSub = ia->iSubItem;
                InvalidateRect(hwnd, &g_rcStatus, FALSE);
                return 0;
            }
            if (nm->code == NM_DBLCLK && d && !d->loading) {
                NMITEMACTIVATE* ia = (NMITEMACTIVATE*)lp;
                if (ia->iItem >= 0 && ia->iSubItem >= 1) BeginCellEdit(ia->iItem, ia->iSubItem);
                return 0;
            }
            if (nm->code == LVN_ITEMCHANGED) {
                InvalidateRect(hwnd, &g_rcStatus, FALSE);
                return 0;
            }
            if (nm->code == LVN_KEYDOWN && d && !d->loading) {
                WORD vk = ((NMLVKEYDOWN*)lp)->wVKey;
                bool ctrl = GetKeyState(VK_CONTROL) < 0;
                if (vk == VK_F2) {
                    int f = ListView_GetNextItem(g_hList, -1, LVNI_FOCUSED);
                    if (f >= 0) BeginCellEdit(f, std::max(1, g_selSub));
                } else if (vk == VK_DELETE) {
                    RowDelete(d);
                } else if (vk == 'C' && ctrl) {
                    CopySelection(d);
                }
                return 0;
            }
            if (nm->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lp;
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) return CDRF_NOTIFYSUBITEMDRAW;
                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                    int item = (int)cd->nmcd.dwItemSpec, sub = cd->iSubItem;
                    uint8_t fl = 0;
                    if (d && item >= 0 && item < (int)d->visible.size()) {
                        int row = d->visible[item];
                        if (row - 1 >= 0 && row - 1 < (int)d->rowFlags.size()) fl = d->rowFlags[row - 1];
                    }
                    cd->clrText = (sub == 0) ? CLR_MUTED : CLR_TEXT;
                    if (item == g_matchItem && sub == g_matchSub) cd->clrTextBk = CLR_MATCH;
                    else if (fl == 1) cd->clrTextBk = CLR_BADROW;
                    else if (fl == 2) cd->clrTextBk = CLR_WARNROW;
                    else if (item % 2 == 1) cd->clrTextBk = CLR_ALTROW;
                    else cd->clrTextBk = CLR_BG;
                    return CDRF_DODEFAULT;
                }
                return CDRF_DODEFAULT;
            }
        }
        break;
    }
    case WM_CONTEXTMENU: {
        if ((HWND)wp == g_hList) {
            POINT scr = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (scr.x == -1 && scr.y == -1) {   // via tombol Menu keyboard
                RECT r;
                GetWindowRect(g_hList, &r);
                scr = {(r.left + r.right) / 2, (r.top + r.bottom) / 2};
            }
            ShowListContextMenu(scr);
            return 0;
        }
        break;
    }
    case WM_APP_HDRMENU:
        ShowHeaderContextMenu((int)wp);
        return 0;
    case WM_APP_HDRFIT:
        AutofitColumn((int)wp);
        return 0;
    case WM_SETTINGCHANGE:
        // ikuti perubahan tema sistem bila tidak ada override manual
        if (g_themePref < 0 && lp && StrCmpIW((const wchar_t*)lp, L"ImmersiveColorSet") == 0)
            ApplyTheme();
        break;
    case WM_DPICHANGED: {
        g_dpi = HIWORD(wp);
        CreateFonts();
        ApplyFonts();
        RECT* r = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        return 0;
    }
    case WM_CLOSE:
        while (!g_docs.empty()) {
            if (!CloseDoc((int)g_docs.size() - 1)) return 0;   // user batal
        }
        SaveSettings();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------------ entry

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    g_hInst = hInst;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    LoadSettings();

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.lpszClassName = L"CSVCommanderWnd";
    wc.style = 0;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"CSV Commander",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 760,
        nullptr, nullptr, hInst, nullptr);
    g_dpi = GetDpiForWindow(hwnd);
    CreateFonts();
    ApplyFonts();

    // pulihkan posisi & ukuran jendela dari sesi sebelumnya
    {
        int ww = IniInt(L"ww", 0), wh = IniInt(L"wh", 0);
        if (ww >= 400 && wh >= 300) {
            WINDOWPLACEMENT wp = {sizeof(wp)};
            GetWindowPlacement(hwnd, &wp);
            wp.rcNormalPosition.left = IniInt(L"wx", 100);
            wp.rcNormalPosition.top = IniInt(L"wy", 100);
            wp.rcNormalPosition.right = wp.rcNormalPosition.left + ww;
            wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + wh;
            wp.showCmd = IniInt(L"wmax", 0) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
            SetWindowPlacement(hwnd, &wp);   // sekaligus menampilkan jendela
        } else ShowWindow(hwnd, nShow);
    }
    if (!g_folder.empty()) ScanFolder();
    Layout();

    // file dari command line (asosiasi file / "Open with")
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++)
            if (PathFileExistsW(argv[i])) OpenPath(argv[i]);
        LocalFree(argv);
    }

    ACCEL acc[] = {
        {FCONTROL | FVIRTKEY, 'O', IDM_OPEN},
        {FCONTROL | FVIRTKEY, 'N', IDM_NEW},
        {FCONTROL | FVIRTKEY, 'S', IDM_SAVE},
        {FCONTROL | FSHIFT | FVIRTKEY, 'S', IDM_SAVEAS},
        {FCONTROL | FVIRTKEY, 'W', IDM_CLOSETAB},
        {FCONTROL | FVIRTKEY, 'F', IDM_FIND},
        {FCONTROL | FVIRTKEY, 'A', IDM_SELALL},
        {FCONTROL | FVIRTKEY, VK_TAB, IDM_NEXTTAB},
        {FCONTROL | FSHIFT | FVIRTKEY, VK_TAB, IDM_PREVTAB},
        {FCONTROL | FVIRTKEY, '1', IDM_MODE_VIEW},
        {FCONTROL | FVIRTKEY, '2', IDM_MODE_SPLIT},
        {FCONTROL | FVIRTKEY, '3', IDM_MODE_EDIT},
        {FCONTROL | FVIRTKEY, 'H', IDM_REPLACE},
        {FCONTROL | FVIRTKEY, 'Z', IDM_UNDO},
        {FCONTROL | FVIRTKEY, 'Y', IDM_REDO},
        {FCONTROL | FVIRTKEY, 'E', IDM_EXPORT_ALL},
        {FVIRTKEY, VK_F3, IDM_FINDNEXT},
        {FSHIFT | FVIRTKEY, VK_F3, IDM_FINDPREV},
        {FVIRTKEY, VK_ESCAPE, IDM_ESCAPE},
    };
    HACCEL hAccel = CreateAcceleratorTableW(acc, _countof(acc));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (g_hFind && IsWindowVisible(g_hFind) && IsDialogMessageW(g_hFind, &msg)) continue;
        if (!TranslateAcceleratorW(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    CoUninitialize();
    return (int)msg.wParam;
}
