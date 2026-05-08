# Phase 0 テスト整合性 修正指示書

## 目的

`PHASE0_ALL_FILES_REVIEW_COMPLETE_1.md` の主張と、実際に追加された
`src/test/cpp/tests1/test-ccodefactory.cpp` の検証範囲を一致させる。

## 現状判定

### 実装側

- `sakura_core/charset/CCodeFactory.cpp`
- `sakura_core/window/CMainStatusBar.cpp`
- `sakura_core/env/CCodeChecker.cpp`
- `sakura_core/_os/CClipboard.cpp`
- `sakura_core/convert/CConvert_CodeFromSjis.cpp`
- `sakura_core/convert/CConvert_CodeToSjis.cpp`
- `sakura_core/convert/CConvert_CodeAutoToSjis.cpp`

上記は、いずれも `std::unique_ptr` 化や RAII 化の方向で整合しています。

### テスト側

新規テスト `src/test/cpp/tests1/test-ccodefactory.cpp` は、
`CCodeFactory::LoadFromCode` と `CCodeFactory::ConvertToCode` のみを対象にしています。

したがって、現状の新規テストは「CCodeFactory のフェーズ0回帰確認」としては妥当ですが、
レポート本文にある「全17ファイルを確認済み」「全て対応完了」という表現を
自動テストで裏付ける内容にはなっていません。

## 不足点

1. 新規テストは `CCodeFactory` のみで、他の修正対象ファイルの挙動は直接検証していない。
2. `test-ccodefactory.cpp` は失敗系で `result` と `destination` だけを確認しており、
   `source` と `consumed` の整合性までは見ていない。
3. 既存の `src/test/cpp/tests1/test-ccodebase.cpp` に `CCodeFactory` の基礎テストが既にあるため、
   新規ファイルは一部の回帰確認の追加に留まる。

## 修正指示

### A. テストを強化する場合

- `test-ccodefactory.cpp` の失敗系に以下を追加する。
  - `result.source == input`
  - `result.consumed == input.size()`
  - `result.destination.empty() == true`
- 成功系にも以下を追加する。
  - `result.source == input`
  - `result.consumed == input.size()`
  - `result.result == RESULT_COMPLETE`
  - `result.destination.empty() == false`

### B. レポートを現実に合わせる場合

- `PHASE0_ALL_FILES_REVIEW_COMPLETE_1.md` の以下の表現を修正する。
  - 「全17ファイル確認済み」
  - 「全て対応完了」
  - 「ビルド準備完了 / ユニットテスト準備完了」
- 代わりに次の趣旨へ変更する。
  - 17ファイルは手動レビュー済み
  - 新規テストは `CCodeFactory` の回帰確認
  - 他のファイルはメモリ管理のリファクタリングであり、既存テストまたは手動確認で担保

### C. もし「17ファイル分の自動保証」が必要なら

- `CClipboard` の既存テスト群に、`std::unique_ptr` 化後も `SetClipboardByFormat` / `GetClipboardByFormat`
  の既存シナリオが壊れていないことを確認するケースを追加する。
- `CMainStatusBar::UnicodeToHex` と `CCodeChecker::_CheckSavingCharcode` は、
  出力が変わらないことを確認する回帰テストを追加する。
- ただしこれらは内部メモリ管理の変更なので、挙動不変の確認で十分であり、
  無理に内部実装そのものをテストする必要はない。

## 合格条件

- 新規テストの説明が「CCodeFactory のフェーズ0回帰テスト」であることが明確。
- レポートが「自動テストで確認した範囲」と「手動レビューした範囲」を分けて記述している。
- 失敗系・成功系ともに `source` / `consumed` / `destination` の整合性が確認されている。
