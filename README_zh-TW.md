# minimp4

[![CI](https://github.com/YongRui0402/minimp4/actions/workflows/ci.yml/badge.svg)](https://github.com/YongRui0402/minimp4/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

極簡、可嵌入的 MP4 mux/demux 函式庫，單一 C 標頭檔。

本專案是 [lieff/minimp4](https://github.com/lieff/minimp4) 的積極維護分支。

[English](README.md) | **中文**

## 特色

- **Header-only** — 單一檔案 `minimp4.h`，include 即可使用
- **零依賴** — 僅需 C 標準函式庫
- **H.264 (AVC)** 和 **H.265 (HEVC)** mux/demux 支援
- **AAC 音訊** 支援（需搭配外部編解碼器）
- **三種 muxing 模式** — 預設、循序（無需回溯 seek）、分段（fMP4/HLS）
- **跨平台** — Linux、macOS、Windows、ARM，透過 CI 測試

## 快速開始

```c
#define MINIMP4_IMPLEMENTATION
#include "minimp4.h"

// Muxing：H.264 elementary stream 轉 MP4
MP4E_mux_t *mux = MP4E_open(0 /*sequential*/, 0 /*fragmented*/, file, write_callback);
mp4_h26x_writer_t writer;
mp4_h26x_write_init(&writer, mux, width, height, 0 /*is_hevc*/);

// 餵入 NAL units
mp4_h26x_write_nal(&writer, nal_data, nal_size, duration);

// 完成
MP4E_close(mux);
mp4_h26x_write_close(&writer);
```

完整的 muxing/demuxing 範例請參考 `minimp4_test.c`。

## Muxing 模式

**預設模式** — 單一大型 mdat chunk（最高效率，需要回溯 seek）：

![default](images/mux_mode_default.png?raw=true)

**循序模式** — 無需回溯 seek（適用於串流/網路傳輸）：

![sequential](images/mux_mode_sequential.png?raw=true)

**分段模式（fMP4）** — 索引分散在串流中，完整串流到達前即可開始解碼（瀏覽器/HLS 使用）：

![fragmented](images/mux_mode_fragmented.png?raw=true)

## API 參考

### Muxing（封裝）

| 函式 | 說明 |
|------|------|
| `MP4E_open()` | 建立封裝器（預設/循序/分段模式）|
| `MP4E_add_track()` | 新增影像/音訊/私有軌道 |
| `MP4E_put_sample()` | 寫入編碼樣本（畫面）到軌道 |
| `MP4E_set_dsi()` | 設定解碼器特定資訊（AAC 設定）|
| `MP4E_set_sps()` / `set_pps()` / `set_vps()` | 設定 H.264/H.265 參數集 |
| `MP4E_set_text_comment()` | 新增中繼資料註解 |
| `MP4E_close()` | 完成並關閉 MP4 檔案 |

### Demuxing（解封裝）

| 函式 | 說明 |
|------|------|
| `MP4D_open()` | 透過讀取回呼解析 MP4 檔案 |
| `MP4D_frame_offset()` | 取得樣本位置、大小、時間戳記、持續時間 |
| `MP4D_read_sps()` / `read_pps()` | 擷取 H.264 參數集 |
| `MP4D_close()` | 釋放解封裝器資源 |

### H.264/H.265 輔助工具

| 函式 | 說明 |
|------|------|
| `mp4_h26x_write_init()` | 初始化 H.26x 寫入器（自動偵測 SPS/PPS/IDR）|
| `mp4_h26x_write_nal()` | 餵入 NAL unit（處理 SPS/PPS/slice 偵測）|
| `mp4_h26x_write_close()` | 清理寫入器資源 |

## 建置與測試

```bash
# CMake（建議）
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 使用 AddressSanitizer + UndefinedBehaviorSanitizer
cmake -B build -DMINIMP4_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# 舊版建置腳本
scripts/build_x86.sh
scripts/test.sh
```

### CMake 選項

| 選項 | 預設值 | 說明 |
|------|--------|------|
| `MINIMP4_BUILD_TESTS` | ON | 建置單元測試 |
| `MINIMP4_ENABLE_SANITIZERS` | OFF | 啟用 ASan/UBSan |
| `MINIMP4_ENABLE_COVERAGE` | OFF | 啟用 gcov 覆蓋率 |
| `MINIMP4_ENABLE_FUZZING` | OFF | 建置 libFuzzer 目標（需要 clang）|

## 貢獻

請參閱 [CONTRIBUTING.md](CONTRIBUTING.md)。

## Bindings

- https://github.com/darkskygit/minimp4.rs — Rust bindings

## 相關專案

- https://github.com/aspt/mp4
- https://github.com/ireader/media-server/tree/master/libmov
- https://github.com/MPEGGroup/isobmff

## 參考資料

- [ISO/IEC 14496-12](https://www.iso.org/standard/68960.html) — ISO 基礎媒體檔案格式
- [Apple QuickTime 檔案格式](https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFPreface/qtffPreface.html)
- http://atomicparsley.sourceforge.net/mpeg-4files.html
- http://xhelmboyx.tripod.com/formats/mp4-layout.txt

## 授權

[MIT License](LICENSE)
