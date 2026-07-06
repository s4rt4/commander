# CSV Commander

Viewer/editor CSV ringan untuk Windows — pure Win32 API, satu exe ±230 KB, tanpa dependency, startup instan.

## Fitur

- **Multi-tab** — buka banyak file sekaligus; klik tengah atau `Ctrl+W` untuk menutup tab.
- **Tiga mode** (segmented control di toolbar, atau `Ctrl+1/2/3`):
  - **View** — tabel virtual (sanggup ratusan ribu baris), header hijau *sticky*, baris belang hijau muda, sort per kolom (klik header: naik → turun → reset).
  - **Edit** — teks CSV mentah (font monospace), `Ctrl+S` untuk menyimpan.
  - **Split** — edit kiri, tabel kanan; tabel ter-update otomatis saat mengetik.
- **File besar** — dibaca chunked di background thread dengan progress bar; UI tidak pernah beku saat memuat.
- **Validasi skema otomatis** — tipe kolom (angka/tanggal/teks) dideteksi dari isinya; baris dengan jumlah kolom salah disorot **merah**, nilai yang menyimpang dari tipe kolom disorot **oranye**; jumlah baris bermasalah tampil di status (⚠).
- **Filter baris** — kotak kanan atas (`Ctrl+F`), mencari di semua kolom; status menampilkan `cocok / total`.
- **Find & Replace** (`Ctrl+H`) — mode simple atau **regex** (dengan `$1` dst. untuk grup), match case opsional, dan **Preview** yang menampilkan daftar before→after sebelum diterapkan.
- **Undo/redo** (`Ctrl+Z` / `Ctrl+Y`) — membatalkan operasi Ganti Semua (snapshot, maks 20 langkah); undo ketikan biasa memakai undo bawaan editor.
- **Export** (tombol ⭳ di toolbar, atau `Ctrl+E`) — semua baris, atau hanya tampilan saat ini (terfilter + tersortir). Sel di-quote sesuai standar CSV.
- **Sidebar explorer** — buka folder, semua `.csv`/`.tsv` tampil sebagai daftar; klik untuk membuka.
- **Drag & drop** file ke jendela, atau via command line: `CSVCommander.exe data.csv` (bisa dijadikan target "Open with").
- Deteksi otomatis delimiter (`,` `;` tab `|`) dan encoding (UTF-8 ± BOM, UTF-16 LE, ANSI fallback); field ber-kutip sesuai RFC 4180.

## Pintasan keyboard

| Tombol | Aksi |
|---|---|
| `Ctrl+O` / `Ctrl+N` | Buka file / tab baru |
| `Ctrl+S` / `Ctrl+Shift+S` | Simpan / Simpan sebagai |
| `Ctrl+W` | Tutup tab |
| `Ctrl+F` | Fokus ke filter |
| `Ctrl+H` | Find & Replace |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+E` | Export semua baris |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Pindah tab |
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | Mode View / Split / Edit |

## Build

Butuh Visual Studio Build Tools 2022 (MSVC). Jalankan:

```
build.bat
```

Hasil: `CSVCommander.exe` di root proyek (manifest & icon sudah ter-embed).
Logo (`src\app.ico`) di-generate ulang dengan `powershell -File src\gen_icon.ps1`.

## Batasan saat ini

- Ganti Semua dan undo pada file ratusan ribu baris memblokir UI beberapa detik (regex berjalan di thread UI).
- Mode Edit melambat untuk file puluhan MB (batasan kontrol EDIT Windows); mode View tetap cepat karena tabel virtual.
- Pembagian panel Split tetap 50:50 (belum bisa digeser).
- Penyimpanan selalu UTF-8 (BOM dipertahankan bila aslinya ber-BOM); line ending dinormalkan ke CRLF.
