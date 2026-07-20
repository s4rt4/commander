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
- **Edit sel langsung di tabel** — double-click sel (atau `F2`) untuk mengedit di tempat; `Enter` simpan, `Esc` batal, `Tab`/`Shift+Tab` pindah sel.
- **Operasi baris & kolom** — klik kanan tabel: sisip/duplikat/hapus baris, sisip/hapus/ganti nama kolom; klik kanan header untuk menu kolom. `Del` menghapus baris terseleksi, `Ctrl+C` menyalin baris (tab-separated, siap paste ke Excel).
- **Statistik kolom** — klik kanan → Statistik: jumlah terisi/kosong, nilai unik, min/max/jumlah/rata-rata untuk kolom numerik.
- **Cari & lompat** — `F3`/`Shift+F3` (atau `Enter` di kotak filter) melompat antar sel yang cocok dan menyorotnya, tanpa menyembunyikan baris lain; `Esc` membersihkan filter.
- **Tema terang & gelap** — tombol ☾/☀ di kanan toolbar; default mengikuti tema Windows, pilihan tersimpan.
- **Status bar** — jumlah baris/kolom, posisi sel aktif, dan indikator **delimiter** & **encoding** yang bisa diklik untuk override manual (koma/titik koma/tab/pipe; UTF-8, UTF-8 BOM, ANSI).
- **Recent files** — layar awal menampilkan file terakhir dibuka; posisi jendela, tema, sidebar, folder, dan pembagian Split tersimpan di `%APPDATA%\CSVCommander\settings.ini`.
- **Sidebar explorer** — buka folder, semua `.csv`/`.tsv` tampil sebagai daftar; klik untuk membuka.
- **Drag & drop** file ke jendela, atau via command line: `CSVCommander.exe data.csv` (bisa dijadikan target "Open with").
- Deteksi otomatis delimiter (`,` `;` tab `|`) dan encoding (UTF-8 ± BOM, UTF-16 LE, ANSI fallback); field ber-kutip sesuai RFC 4180.
- Panel **Split bisa digeser** (drag pembatas tengah), lebar kolom bisa di-autofit (double-click pembatas header), sel yang terpotong tampil utuh sebagai tooltip.

## Pintasan keyboard

| Tombol | Aksi |
|---|---|
| `Ctrl+O` / `Ctrl+N` | Buka file / tab baru |
| `Ctrl+S` / `Ctrl+Shift+S` | Simpan / Simpan sebagai |
| `Ctrl+W` | Tutup tab |
| `Ctrl+F` | Fokus ke filter |
| `F3` / `Shift+F3` | Lompat ke sel cocok berikut / sebelum |
| `Esc` | Batal edit sel / bersihkan filter |
| `F2` | Edit sel terfokus |
| `Del` | Hapus baris terseleksi |
| `Ctrl+C` | Salin baris terseleksi |
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
- Line ending dinormalkan ke CRLF saat menyimpan; file UTF-16 disimpan ulang sebagai UTF-8.
