# Plan Rewrite: CSV Commander untuk Fedora GNOME (GTK4)

Dokumen ini adalah spesifikasi lengkap untuk menulis ulang CSV Commander (Win32, `src/main.cpp`)
sebagai aplikasi GNOME native. Ditulis untuk dieksekusi oleh agent.

## 0. Referensi visual — WAJIB dibuka lebih dulu

**`mockup/ui-mockup.html` adalah sumber kebenaran UI**, bukan kode Win32.
Buka di browser; semuanya interaktif (ganti mode, sort, filter, find & replace, tutup/buka tab).
State tertentu bisa dibuka langsung lewat parameter URL — gunakan ini untuk membanding screenshot:

| URL | State |
|---|---|
| `ui-mockup.html` | Mode View, dokumen dengan baris bermasalah |
| `ui-mockup.html?mode=split` | Split: editor kiri, tabel kanan |
| `ui-mockup.html?mode=edit` | Edit: teks CSV mentah |
| `ui-mockup.html?fr=1` | Dialog Find & Replace dengan hasil preview |
| `ui-mockup.html?state=loading` | Layar loading file besar + progress bar |
| `ui-mockup.html?state=empty` | Tanpa dokumen (empty state) |
| `ui-mockup.html?state=folder` | Sidebar dengan daftar file folder |

Widget "MOCKUP STATE" di pojok kanan-bawah hanyalah alat demo — **jangan diimplementasikan**.

### Palet warna (nilai persis, jangan diganti tema)

| Token | Hex | Pemakaian |
|---|---|---|
| bg | `#FFFFFF` | latar konten, tabel, editor |
| chrome | `#F7F8F7` | headerbar, tab bar, sidebar, dialog |
| border | `#E4E7E4` | garis pemisah area, border input |
| text | `#1F2328` | teks utama |
| muted | `#737A73` | teks sekunder (status, tab non-aktif, EXPLORER) |
| accent | `#0F7A4D` | header tabel, tombol mode aktif, progress, tab underline |
| hot | `#E9EDE9` | hover tombol/tab |
| altrow | `#EBF4EF` | zebra baris genap |
| hdr-sep | `#3A946A` | pemisah kolom di header hijau |
| badrow | `#FCE7E7` | baris dgn jumlah kolom ≠ header |
| warnrow | `#FFF4E0` | baris dgn nilai menyimpang dari tipe kolom |

### Metrik utama
Headerbar 48px; tab bar 38px (tab 32px, radius atas 6, underline aktif 2px accent);
sidebar 220px; tombol ikon 32×32 radius 6; segmented 3×(68×28) radius 6 dalam kontainer putih radius 8;
filter box 180×24; baris tabel padding 4–5px×8px; kolom min 70px maks 340px (ellipsis).

## 1. Stack

- **GTK4 + libadwaita**, bahasa **C++17 dengan gtkmm-4** — supaya logika inti dari `main.cpp`
  bisa di-port hampir apa adanya. (Qt6 juga dapat diterima bila ada alasan kuat, tapi plan ini menganggap GTK4.)
- Build: **Meson**. App ID: **`io.github.s4rt4.commander`** (konvensi Flathub untuk repo GitHub).

## 2. Struktur proyek

```
linux/
  meson.build
  csvcore/            # logika murni, TANPA dependensi GTK — unit-testable
    parser.{h,cc}     # ParseCSV, DetectDelim (port dari main.cpp)
    validate.{h,cc}   # ValidateDoc, LooksDate, aturan 90%
    document.{h,cc}   # Document, visible/filter/sort, undo stack
    replace.{h,cc}    # ComputeReplace (simple/regex, per baris, samples ≤200)
    io.{h,cc}         # baca chunked + deteksi encoding, tulis UTF-8(±BOM)
  ui/
    main.cc  app.{h,cc}  window.{h,cc}  tabpage.{h,cc}
    csvview.{h,cc}    # GtkColumnView + model
    finddialog.{h,cc}
    style.css         # seluruh palet di atas
  data/
    io.github.s4rt4.commander.desktop
    io.github.s4rt4.commander.metainfo.xml
    io.github.s4rt4.commander.svg          # salin dari assets/logo.svg
    icons/hicolor/scalable/mimetypes/text-csv.svg   # salin dari assets/icon-csv-file.svg
    symbolic/*.svg    # ikon toolbar 16px (lihat §5)
  flatpak/io.github.s4rt4.commander.yml
```

## 3. Porting csvcore (dari `src/main.cpp`)

Fungsi yang dipindahkan nyaris verbatim: `DetectDelim`, `ParseCSV`, `LooksDate`, `ValidateDoc`
(acuan lebar = baris header!), `RebuildVisible`, `ComputeReplace`, `QuoteCell`, logika export,
`PushUndo`/undo-redo (maks 20 snapshot / 64MB).

Penyesuaian wajib:
- `std::wstring` (UTF-16) → `std::string` (UTF-8).
- `StrStrIW` (filter case-insensitive) → lipat dengan `g_utf8_casefold` lalu `find`.
- `StrCmpLogicalW` (sort natural) → `g_utf8_collate_key_for_filename` (perilaku setara).
- `std::wregex` → `std::regex` pada UTF-8 (cukup; ECMAScript + icase sama).
- Baca file: chunk 4MB + progress callback; deteksi BOM UTF-8/UTF-16LE; fallback lossy latin1
  bila bukan UTF-8 valid. Simpan selalu UTF-8, BOM dipertahankan bila aslinya ber-BOM.
- File ≥ **1 MB** dimuat di thread (`std::thread` atau `Gio::Task`); progres & hasil dikirim ke
  main loop via `Glib::Dispatcher`/`g_idle_add` (padanan `WM_APP_PROG`/`WM_APP_DONE`).

## 4. Pemetaan widget Win32 → GTK4

| Win32 sekarang | GTK4/libadwaita |
|---|---|
| Toolbar custom-paint + title bar | `AdwHeaderBar` (CSD): tombol ikon flat kiri, segmented di `title-widget`, status+filter di pack_end |
| Segmented View/Split/Edit | `AdwToggleGroup` (atau 3 `GtkToggleButton` linked) + CSS accent |
| Tab bar custom (●, ×, +) | `AdwTabBar` + `AdwTabView` (dirty = `needs-attention`/indikator; tombol + di ujung) |
| Sidebar listbox | `GtkBox` berisi label EXPLORER + `GtkListBox`; toggle via tombol ☰ (bukan `AdwOverlaySplitView` agar identik dgn mockup) |
| ListView virtual (LVS_OWNERDATA) | `GtkColumnView` + `GListModel` custom (lazy, sanggup 400k baris) + `GtkSortListModel` tidak dipakai — sort/filter dikerjakan csvcore agar identik |
| Header hijau + panah sort | CSS `columnview > header > button` (bg accent, teks putih, border kanan hdr-sep); panah ▲▼ ditambahkan manual di judul kolom |
| Zebra + baris bad/warn | cell factory menambah css class `bad`/`warn` per baris; zebra via `:nth-child(even)` di CSS row |
| Kontrol EDIT | `GtkTextView` monospace dalam `GtkScrolledWindow` (opsional `GtkSourceView` utk nomor baris) |
| Split 50:50 | `GtkPaned` (**boleh** bisa digeser — peningkatan yang diizinkan) |
| Dialog Find & Replace | `AdwDialog`/`GtkWindow` modeless; layout persis mockup `?fr=1` |
| Tooltip TTF_SUBCLASS | properti `tooltip-text` bawaan |
| Dialog file/folder | `GtkFileDialog` (portal, otomatis di Flatpak) |
| Menu export (TrackPopupMenu) | `GtkMenuButton` + `GMenu`: "Export semua baris…" / "Export tampilan saat ini (N baris)…" (item kedua disabled bila tanpa filter/sort) |
| Progress loading | overlay di konten: teks "Memuat … NN%" + `GtkProgressBar` lebar 320 |
| Drag & drop file | `GtkDropTarget` (GFile) |

## 5. Ikon & logo

- Logo aplikasi: **`assets/logo.svg`** (path koma = outline glyph asli, identik dgn `app.ico`)
  → install ke `hicolor/scalable/apps/io.github.s4rt4.commander.svg`.
- Ikon file CSV: **`assets/icon-csv-file.svg`** → `hicolor/scalable/mimetypes/text-csv.svg`
  (juga `text-x-csv.svg`, `application-csv.svg` sebagai symlink/salinan — penamaan mimetype icon).
- Ikon toolbar: JANGAN pakai font MDL2. Pakai Adwaita symbolic yang setara:
  `sidebar-show-symbolic`, `folder-open-symbolic`, `document-open-symbolic`,
  `document-save-symbolic`, `document-save-as-symbolic`/`folder-download-symbolic` (export),
  `edit-find-replace-symbolic`, `tab-new-symbolic`, `window-close-symbolic`.
  Bentuk garis-tipis 16px seperti di mockup sudah cukup dekat dengan Adwaita.
- `.desktop`: `MimeType=text/csv;text/tab-separated-values;` `Exec=csvcommander %f` — dengan ini
  GNOME menawarkan "Open With CSV Commander" dan `xdg-mime default` menjadikannya default.

## 6. Perilaku yang harus identik (checklist QA)

- [ ] Sort: klik header → naik → turun → reset; kolom `#` mereset; numerik dibanding numerik, sisanya natural-compare; panah tampil di header.
- [ ] Filter: debounce 250 ms; substring case-insensitive di semua kolom; status `x / y baris · k kolom`.
- [ ] Split: ketikan di editor → debounce 400 ms → reparse → tabel + status + validasi ter-update; tab jadi dirty (●), judul jendela ber-•.
- [ ] Validasi: tipe kolom = angka/tanggal bila ≥90% nilai non-kosong seragam; baris ≠ lebar header → `badrow`; nilai menyimpang → `warnrow`; status `⚠ n bermasalah`.
- [ ] Loading: file ≥1MB di thread; UI menampilkan "Memuat nama… NN%" + progress bar; tab bisa ditutup saat loading tanpa crash.
- [ ] Find & Replace: simple = literal (escape regex, `$` literal); regex mendukung grup `$1`; Match case; Preview menghitung & menampilkan ≤200 sampel `baris N: lama → baru` TANPA mengubah dokumen; Ganti Semua = snapshot undo dulu.
- [ ] Undo/redo: Ctrl+Z/Ctrl+Y; ketikan pakai undo TextView bawaan, operasi Ganti Semua pakai snapshot.
- [ ] Export: quoting RFC 4180 (kutip bila mengandung delimiter/kutip/newline; `""` untuk kutip), delimiter mengikuti sumber, CRLF; "tampilan saat ini" mengikuti filter+sort.
- [ ] Buka: dialog, drag & drop, argumen CLI, klik file di sidebar; file yang sama tidak dibuka dua kali (pindah tab).
- [ ] Tutup tab dirty → dialog Simpan/Buang/Batal (`AdwAlertDialog`).
- [ ] Shortcut: Ctrl+O/N/S/Shift+S/W/F/H/E/Z/Y, Ctrl+Tab / Ctrl+Shift+Tab, Ctrl+1/2/3.

## 7. Packaging

- **Flatpak** (utama): runtime `org.gnome.Platform//48`, sandbox default; file dibuka via portal
  sehingga tidak perlu izin filesystem luas. Target akhir: Flathub.
- RPM (opsional belakangan): spec sederhana, `BuildRequires: gtkmm4-devel libadwaita-devel meson`.
- `metainfo.xml`: sertakan screenshot (bisa diambil dari mockup untuk sementara).

## 8. Milestone

1. **M1 csvcore** — port + unit test (parser kutip/delimiter, validasi 90%, replace, export quoting). Test = spesifikasi dari `README.md` + §6.
2. **M2 kerangka UI** — jendela, headerbar, segmented, tab (AdwTabView), sidebar, style.css dengan palet persis; bandingkan screenshot vs `ui-mockup.html` state normal.
3. **M3 tabel** — GtkColumnView virtual + header hijau + zebra + bad/warn + sort + filter; uji 400k baris.
4. **M4 editor & split** — TextView, live-reparse, dirty; mode switching Ctrl+1/2/3.
5. **M5 fitur** — find&replace + preview + undo, export, loading async + progress, drag&drop, asosiasi file.
6. **M6 packaging** — ikon/mime/desktop/metainfo, Flatpak build, QA checklist §6 penuh + banding screenshot semua state mockup.

## 9. Divergensi yang diizinkan (jangan dilawan)

- Font: Cantarell/Adwaita Sans & mono default sistem (bukan Segoe UI/Consolas).
- Scrollbar, caret, seleksi, dialog file = gaya Adwaita.
- Tombol jendela mengikuti setelan GNOME user (posisi/jumlah).
- `GtkPaned` split boleh digeser (Win32 fixed 50:50).
- Dialog konfirmasi memakai `AdwAlertDialog` (bukan MessageBox 3 tombol klasik).

Selain daftar ini, tampilan harus mengikuti mockup — bila ragu, screenshot mockup-nya.
