# minimp4 維護接管計畫

## 背景

minimp4 是一個極簡、header-only 的 C 語言 MP4 mux/demux 函式庫，原始專案為 [lieff/minimp4](https://github.com/lieff/minimp4)，目前已停止維護。本計畫的目標是接手維護這個專案，透過四個階段逐步建立完善的基礎設施，最終成為該專案的活躍維護版本。

### 專案核心理念

所有變更必須遵守以下原則：

- **極簡** — 不加入非必要功能
- **零依賴** — 僅使用 C 標準函式庫
- **可嵌入** — 必須能在嵌入式目標（ARM 等）上編譯
- **Header-only** — 單一標頭檔 include 即可使用

### 現狀評估

| 項目 | 現狀 |
|------|------|
| 程式碼 | `minimp4.h`（3502 行）+ `minimp4_test.c`（367 行）|
| 構建系統 | Shell scripts（`scripts/build_x86.sh`, `build_arm.sh`），無 CMake/Makefile |
| 測試 | 二進位比對測試（`scripts/test.sh` + `vectors/` 參考檔案），無單元測試 |
| CI/CD | 僅 Travis CI（`.travis.yml`），無 GitHub Actions |
| 文件 | 極簡 README（48 行），無貢獻指南 |
| 版本 | 無 tag、無 release，僅 master 分支 |
| 授權 | CC0 1.0 Universal（公共領域）|
| 已知問題 | 6 個 TODO/FIXME、缺少邊界檢查、HEVC 支援不完整 |

---

## Phase 1：自動化測試基礎設施

> 目標：建立完整的測試體系，確保所有功能可驗證、可回歸。

### 1.1 CMake 構建系統

保留現有 `scripts/` 腳本以維持向後相容，新增 CMake 作為主要構建系統。

**新增檔案：**

```
CMakeLists.txt          # 根目錄，定義 INTERFACE library target
tests/CMakeLists.txt    # 測試目標定義
```

**設計要點：**

- `minimp4` 定義為 `INTERFACE` target（header-only library）
- 構建選項：
  - `MINIMP4_BUILD_TESTS` — 構建測試（預設 ON）
  - `MINIMP4_ENABLE_SANITIZERS` — 啟用 ASan/UBSan
  - `MINIMP4_ENABLE_COVERAGE` — 啟用 gcov 覆蓋率
  - `MINIMP4_ENABLE_FUZZING` — 構建 libFuzzer 模糊測試目標
- 同時構建舊的 `minimp4_test.c` 作為 integration test

**基本使用方式：**

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### 1.2 測試框架

採用**自訂極簡測試 harness**，不引入外部測試框架依賴，符合專案零依賴理念。

新增 `tests/test_harness.h`，提供：

- `ASSERT_EQ(a, b)` — 相等斷言
- `ASSERT_NE(a, b)` — 不等斷言
- `ASSERT_NULL(x)` / `ASSERT_NOT_NULL(x)` — 空指標斷言
- `ASSERT_MEM_EQ(a, b, n)` — 記憶體比對
- `ASSERT_TRUE(x)` / `ASSERT_FALSE(x)` — 布林斷言
- `RUN_TEST(fn)` — 測試執行與結果報告
- `TEST_SUMMARY()` — 測試摘要輸出

### 1.3 測試目錄結構

```
tests/
├── test_harness.h          # 極簡斷言/執行巨集
├── test_mux_api.c          # MP4E_* API 單元測試
├── test_demux_api.c        # MP4D_* API 單元測試
├── test_h26x_writer.c      # mp4_h26x_write_* 便利 API 測試
├── test_nal_parsing.c      # NAL unit 查找、escape code 處理
├── test_vector_util.c      # minimp4_vector_* 內部資料結構
├── test_bit_reader.c       # bit_reader_t 和 bs_t 測試
├── test_edge_cases.c       # 空指標、零長度、溢位等邊界情況
├── test_integration.c      # 往返 mux→demux 完整性測試
├── test_hevc.c             # HEVC 專用 mux/demux 路徑
└── fuzz/
    ├── fuzz_demux.c        # libFuzzer: MP4D_open 模糊測試
    └── fuzz_mux_nal.c      # libFuzzer: mp4_h26x_write_nal 模糊測試
```

### 1.4 各模組測試重點

| 測試檔案 | 覆蓋範圍 | 優先級 |
|----------|---------|--------|
| `test_mux_api.c` | `MP4E_open/add_track/put_sample/set_dsi/sps/pps/close`、sequential/fragmented 模式、多軌道 muxing | P0 |
| `test_demux_api.c` | `MP4D_open/frame_offset/read_sps/read_pps/close`、軌道列舉、fMP4 demux | P0 |
| `test_integration.c` | H.264 mux→demux 往返、三種模式各自往返、slices 處理（取代 shell `cmp` 測試）| P0 |
| `test_h26x_writer.c` | `mp4_h26x_write_init/nal/close`、H.264 與 H.265 NAL 寫入 | P1 |
| `test_nal_parsing.c` | `find_nal_unit`（00 00 01 / 00 00 00 01 start codes）、`remove_nal_escapes` 往返、邊界情況 | P1 |
| `test_bit_reader.c` | `init_bits/get_bits/show_bits`、Exp-Golomb 編解碼、position tracking | P1 |
| `test_edge_cases.c` | NULL 指標傳入所有公開 API、零長度資料、無效 track ID、callback 失敗、整數溢位 | P1 |
| `test_vector_util.c` | `minimp4_vector_init/grow/alloc_tail/put/reset` | P2 |
| `test_hevc.c` | HEVC NAL mux/demux、hvcC box 驗證（需先生成 HEVC 測試向量）| P2 |
| `fuzz/fuzz_demux.c` | 任意 bytes 餵入 `MP4D_open`，最高價值模糊測試目標 | P2 |
| `fuzz/fuzz_mux_nal.c` | 任意 bytes 餵入 `mp4_h26x_write_nal` | P2 |

**HEVC 測試向量生成：**

```bash
ffmpeg -f lavfi -i testsrc=duration=1:size=176x144:rate=15 \
  -c:v libx265 -x265-params "keyint=15:min-keyint=15" vectors/foreman.265
```

### 1.5 CI/CD：Travis CI → GitHub Actions

新增 `/.github/workflows/ci.yml`：

| Job | 說明 |
|-----|------|
| `build-and-test` | 矩陣構建：ubuntu/macos/windows x gcc/clang，啟用 ASan/UBSan |
| `coverage` | gcov + lcov → Codecov，目標 **80%+** 行覆蓋率 |
| `legacy-tests` | 執行原有 `scripts/build_x86.sh` + `scripts/test.sh` |
| `fuzz` | clang + libFuzzer，限時 120 秒快速模糊測試 |

**遷移策略：**

1. 先新增 GitHub Actions，與 Travis CI 並行
2. GitHub Actions 穩定通過 2 週後，移除 `.travis.yml`
3. 更新 README 中的 CI badge

### 1.6 已知 Bug（測試階段應發現並修復）

| # | 位置 | 問題描述 | 嚴重度 |
|---|------|---------|--------|
| 1 | `minimp4_test.c:70` | `read_callback` 中 `buf->size - offset - size` 可能整數下溢 | Medium |
| 2 | `minimp4.h:1542` | hvcC box 硬編碼 profile/level，未從 HEVC SPS 解析 | High |
| 3 | `minimp4.h:764` | `minimp4_vector_grow` 的 `capacity*2 + 1024` 可能整數溢位 | Medium |
| 4 | 多處 | `malloc` 後缺少 NULL 檢查 | Medium |
| 5 | `minimp4.h:2486` | switch fallthrough 缺少明確標註，觸發 `-Wimplicit-fallthrough` 警告 | Low |
| 6 | `minimp4.h:3242` | `MP4D_frame_offset` 未驗證 `ntrack` 和 `nsample` 邊界 | High |

### 1.7 執行順序

```
Step 1  ─── CMakeLists.txt（根 + tests/），含 legacy test target
Step 2  ─── tests/test_harness.h
Step 3  ─── .github/workflows/ci.yml（先跑 legacy + CMake build）
Step 4  ─── test_mux_api.c + test_demux_api.c（最高價值）
Step 5  ─── test_integration.c（取代 shell cmp 測試）
Step 6  ─── test_nal_parsing.c + test_bit_reader.c + test_h26x_writer.c
Step 7  ─── test_edge_cases.c + test_vector_util.c
Step 8  ─── Coverage 報告整合
Step 9  ─── Fuzz targets
Step 10 ─── HEVC 測試向量 + test_hevc.c
Step 11 ─── 移除 .travis.yml
```

---

## Phase 2：AI Agent 半自動化管理

> 目標：利用 AI 工具輔助日常維護，降低人工負擔。

### 2.1 CLAUDE.md 專案指南

在根目錄建立 `CLAUDE.md`，讓 Claude Code 了解專案上下文：

```
內容涵蓋：
- 專案概述與架構
- 構建與測試指令
- 專案核心理念
- 程式碼風格規範（C99、4 空格縮排、snake_case、MP4E_/MP4D_/mp4_h26x_ 前綴）
- 已知問題與 TODO 清單
- PR 審查要點
```

### 2.2 AI 輔助 PR 審查

新增 `/.github/workflows/ai-review.yml`：

- **觸發條件：** `pull_request` 事件
- **審查重點：**
  1. 是否符合專案理念（極簡、零依賴、header-only）
  2. C 程式碼品質（邊界檢查、NULL 安全、記憶體洩漏）
  3. 是否有對應測試
  4. 是否引入外部依賴
- **輸出方式：** PR 評論（建議性質，非阻塞 check）

### 2.3 AI 輔助 Issue 分類

- 排程 GitHub Action（每週 cron）
- AI 自動為新 issue 加標籤（`bug` / `enhancement` / `question`）
- 建議優先級（P0-P3）
- **不自動關閉** issue，僅標籤和評論

### 2.4 AI 輔助程式碼品質

在 `.claude/settings.json` 設定 hooks：

- Pre-commit 層級：檢查 malloc 後 NULL 檢查、未檢查返回值、潛在 buffer overflow
- 利用 Claude Code `/review` 功能進行程式碼審查

### 2.5 AI 輔助發布管理

- Git tag 觸發 → 自動生成 changelog
- 草擬 release notes
- 透過 `gh release create` 發布

### 2.6 執行順序

```
Step 1  ─── 建立 CLAUDE.md
Step 2  ─── 擴展 .claude/settings.local.json
Step 3  ─── AI PR 審查 GitHub Action
Step 4  ─── AI Issue 分類排程
Step 5  ─── Pre-commit hooks
Step 6  ─── 發布自動化
```

---

## Phase 3：上游 Issue 與 PR 審查

> 目標：從 lieff/minimp4 篩選有價值的貢獻，整合到維護版本中。

### 3.1 審查流程

1. 新增 upstream remote：
   ```bash
   git remote add upstream https://github.com/lieff/minimp4.git
   git fetch upstream
   ```
2. 使用 `gh` CLI 抓取所有 open/closed issues 和 PRs
3. 建立追蹤文件 `docs/upstream-review.md`

### 3.2 接受/拒絕標準

#### 接受條件（全部滿足）

- 修復真實 bug（crash、資料損壞、規格不符）或改善健壯性
- 不引入外部依賴
- 不大幅增加程式碼量（超過 100 行需強烈理由）
- 維持 header-only、可嵌入特性
- 已測試或我們能為其新增測試

#### 拒絕條件（任一觸發）

- 引入外部函式庫依賴
- 超出核心 mux/demux 範疇
- 破壞向後 API 相容性（無強烈理由）
- 純裝飾性變更（無功能改善）

#### 延後條件

- 規模大，需先建立測試再合併
- 涉及我們計畫重寫的 HEVC 程式碼

### 3.3 Git 工作流程

```
upstream PR #XX
  └─→ cherry-pick 到新分支 upstream/pr-XX
       └─→ 新增對應測試
            └─→ 提 PR 到 master
                 └─→ AI review + 人工審查
                      └─→ merge
```

- 使用 cherry-pick（非 merge）保持歷史乾淨
- commit message 引用原始 issue/PR：`Cherry-pick from lieff/minimp4#XX`

### 3.4 追蹤文件格式

`docs/upstream-review.md`：

```markdown
| 類型 | # | 標題 | 狀態 | 決定 | 理由 | 我們的 PR |
|------|---|------|------|------|------|----------|
| Issue | #25 | ... | open | accept | ... | #3 |
| PR | #19 | ... | merged | already-included | ... | - |
```

### 3.5 執行順序

```
Step 1  ─── 新增 upstream remote 並 fetch
Step 2  ─── 整理所有 issues/PRs 到追蹤文件
Step 3  ─── 逐一依標準審查
Step 4  ─── Cherry-pick 接受的變更，新增測試，透過 PR 合併
Step 5  ─── 在原始 repo 相關 issue 下留言指向我們的修復
```

---

## Phase 4：社群建設與引流

> 目標：建立專業的開源專案形象，吸引用戶從舊 repo 遷移。

### 4.1 README 改版

擴充現有 README（48 行），新結構：

1. **標題 + 徽章** — CI status、Codecov、License badge
2. **Fork 說明** — 明確表示這是 `lieff/minimp4` 的持續維護版本
3. **功能列表** — H.264/H.265 mux/demux、AAC audio、fMP4、sequential mode
4. **快速開始** — `#define MINIMP4_IMPLEMENTATION` + 基本使用範例
5. **API 參考** — 所有公開函式的一行描述表格
6. **構建與測試** — CMake 指令 + 舊腳本說明
7. **Muxing 模式** — 保留現有三張模式圖
8. **Contributing** — 連結到 CONTRIBUTING.md
9. **License** — CC0

### 4.2 CONTRIBUTING.md

新增 `/CONTRIBUTING.md`：

- Bug 回報流程（使用 issue template）
- 程式碼提交流程（fork → branch → PR）
- 程式碼風格指南（C99、4 空格、snake_case）
- 測試要求（所有 PR 必須含測試，CI 必須通過）
- 專案理念聲明（不接受違反核心理念的 PR）
- 審查流程說明（AI 輔助 + 人工審查）

### 4.3 GitHub Issue / PR 模板

```
.github/
├── ISSUE_TEMPLATE/
│   ├── bug_report.md           # 結構化 bug 報告模板
│   └── feature_request.md      # 功能提案模板
└── PULL_REQUEST_TEMPLATE.md    # PR 檢查清單
```

**Bug 報告模板包含：** 版本/commit hash、作業系統/編譯器、重現步驟、預期 vs 實際行為

**PR 模板檢查清單：**
- [ ] 已新增對應測試
- [ ] CI 全部通過
- [ ] 未引入外部依賴
- [ ] 遵循程式碼風格
- [ ] 更新了相關文件

### 4.4 版本策略

| 版本 | 里程碑 | 觸發條件 |
|------|--------|---------|
| `v0.1.0` | 程式碼基線 | Phase 1 開始前，繼承自 lieff/minimp4 |
| `v0.2.0` | 測試基礎設施完成 | Phase 1 完成 |
| `v0.3.0` | 上游整合完成 | Phase 3 完成 |
| `v1.0.0` | 正式穩定版 | 所有已知 bug 修復 + HEVC 完善 |

之後採用 [Semantic Versioning](https://semver.org/)：

- **PATCH**（x.x.1）：bug fix、文件更新
- **MINOR**（x.1.0）：新功能（不破壞 API）
- **MAJOR**（1.0.0）：破壞性 API 變更

### 4.5 引流策略（延後）

> 以下步驟延後至專案更完整時再執行，避免在未準備好的狀態下通知外部人員。

- 在 lieff/minimp4 開 issue 宣佈維護 fork
- 回覆原始 repo 的 open issues
- 聯繫依賴專案更新指向
- 提交至 package managers
- 社群推廣（Reddit、Hacker News 等）

### 4.6 執行順序

```
Step 1  ─── 更新 README.md
Step 2  ─── 建立 CONTRIBUTING.md
Step 3  ─── 建立 issue/PR templates
Step 4  ─── 設定 GitHub topics 與 description
Step 5  ─── 建立初始 release tag v0.1.0
（以下延後）
Step 6  ─── 在 lieff/minimp4 發布宣佈 issue
Step 7  ─── 回覆原始 repo 相關 open issues
Step 8  ─── 聯繫依賴專案維護者
Step 9  ─── 社群推廣文章
```

---

## 總體時程表

```
Week 1  ── Phase 1a ── CMake + test harness + GitHub Actions CI（基本）
Week 2  ── Phase 1b ── test_mux_api + test_demux_api + test_integration
Week 3  ── Phase 1c ── test_nal_parsing + test_bit_reader + test_edge_cases
Week 4  ── Phase 1d ── Coverage + fuzz + HEVC vectors
Week 5  ── Phase 2  ── CLAUDE.md + AI review + AI triage + release automation
Week 6  ── Phase 3  ── Upstream review + cherry-pick + 測試
Week 7  ── Phase 4a ── README + CONTRIBUTING + templates + v0.1.0
Week 8+ ── 持續維護 ── Bug 修復、HEVC 改善、定期 release
（引流推廣延後至專案完整後執行）
```

---

## 驗證方式

### Phase 1 驗證

```bash
# 構建 + 測試（含 sanitizers）
cmake -B build -DMINIMP4_ENABLE_SANITIZERS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# 覆蓋率報告
cmake -B build-cov -DMINIMP4_ENABLE_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
lcov --capture --directory build-cov --output-file coverage.info
# 目標：80%+ 行覆蓋率
```

- GitHub Actions CI 所有 job 綠燈
- Codecov 報告顯示 80%+ 覆蓋率
- Legacy tests（`scripts/test.sh`）仍然通過

### Phase 2 驗證

- 提交測試 PR → AI review bot 自動留下評論
- 排程 action 按時執行 issue triage
- 建立 tag → 自動生成 release notes

### Phase 3 驗證

- `docs/upstream-review.md` 所有 issue/PR 已審查
- 每個接受的變更有對應測試
- Cherry-pick 的 PR 通過 CI

### Phase 4 驗證

- README 包含所有預定章節
- Issue/PR templates 可正常使用
- `v0.1.0` release 已建立
- GitHub topics 已設定
