# 引き継ぎ: Grep 除外ファイル正規表現（C3 以降）

作成: 2026-10-06 / 対象: C2 完了後の C3〜C5 の作業

## 1. 現在地

| PR | 内容 | 状態 |
|---|---|---|
| C1 | キー振り分け・照合フック（`CGrepEnumKeys` / `CGrepEnumFilterFiles`） | **マージ済み**（#2710、master `7a31678f`） |
| C2 | 照合器 `CGrepExceptFileRegexps`、文字列リソース、テスト 11 件 | **作業中**。ヘッダー・リソース・vcxproj は適用済み。テスト（03c C2-8）の適用と 53 件 PASSED の確認、`git add`・commit・push が残り【要実測】 |
| C3 | `-GOPT=E`（`CCommandLine` / `DoGrep`） | 未着手（文書 04 を作り直してから） |
| C4 | ダイアログのチェックボックス | 未着手 |
| C5 | マクロのフラグ | 未着手 |

- C2 のブランチ: `feature/grep-exclude-regex-matcher`（`7a31678f` から）
- C2 の指示書: `修正指示書_Grep_03c_PR-C2_フルパス照合.md`（正）＋ `修正指示書_Grep_03c-1_C2追補_ヘッダーをフルパス照合へ.md`
- 03b・03b-1 は古い（ファイル名照合）。**使わない**
- PR 本文: `PR本文_C2_除外ファイル正規表現の照合器.md`（「53 件」の【要実測】を実測後に外す）
- 手元の退避: `git stash@{0}`（C2 wip、旧版）は**使わない**。不要なら `git stash drop stash@{0}`（`stash@{1}` は別件なので消さない）。`C:\source\git\test-grep-exclude-regex.C1C2.cpp` は C2 マージ後に削除してよい

## 2. 決定事項（変えない）

1. **照合対象はフルパス**。`CGrepEnumFilterFiles::IsValid()` が「基準フォルダー＋キーのフォルダー部分＋ファイル名」を渡す。`^`・`$` はパス全体に当たる。ファイル名だけに当てるときは `\\[^.\\]+$` のように `\\` を使う。部分一致なので `bak` はフォルダー名にも当たる
2. 大文字小文字は区別しない（`CBregexp::optNothing`）。既存の除外ワイルドカードと同じ
3. 新規ファイルは `sakura_core/grep/` に置く。`sakura.vcxproj` は `sakura_core` にあるので、登録は `grep\...` の相対パス（ルール §8-1 #13）
4. C1 の API: `SetFileKeys( lpKeys, bExceptFileRegex=false )`、`m_vecExceptFileRegexKeys`、`m_fnIsExceptFilePath`、`IsExceptFilePath( std::wstring_view )`
5. C2 の API: `Attach( CGrepEnumKeys&, LPCWSTR pszDllName )`、`IsMatch( std::wstring_view )`、`GetErrorMessage()`。照合器は `CGrepEnumKeys` より先に宣言して後に破棄する（照合関数が `this` を参照するため）
6. 文書 04（C3〜C5）の予定値: `N_SHAREDATA_VERSION` 183、`IDC_CHK_EXCLUDE_FILE_REGEXP` 1742、`HIDC` 12028、マクロのフラグ `0x800000`、`-GOPT=E`。**C4 に着手する前に master で実測し直す**（#2701 で ShareData_IO が変わった）

## 3. 文書 04（C3〜C5）で直すこと

03c の §0 #7 のとおり、04 は C1 マージ前の「ファイル名照合」前提で書かれている。**C3 に着手するときに 04 を作り直す。**

| 直す箇所 | 内容 |
|---|---|
| 「ファイル名（フォルダーを含まない）と照合する」 | フルパスに変える |
| 「`^[^.]+$` で README を除外」などの例 | `\\[^.\\]+$` など、フルパス用に書き直す |
| C3 のテスト `ExceptFileRegexp` の正規表現 | 同上。期待値は実測する |
| ヘルプ（`HLP000109.html`・`HLP000067.html`・`HLP000362.html`） | フルパス照合・部分一致・大文字小文字を区別しないことを書く |
| C3 の PR 説明の「#2459 からの変更点」 | フルパス照合を反映する |
| C3 で `DoGrep()` に入れる | `CGrepExceptFileRegexps` を `CGrepEnumKeys` より先に宣言する。`Attach()` の失敗時は `GetErrorMessage()` を表示して中断する |
| リソース ID | `STR_GREP_ERR_EXCLUDE_REGEXP` = 35059（C2 で使用）、`_APS_NEXT_RESOURCE_VALUE` = 35060。C3 以降で足す ID は master で再確認 |

## 4. 注意（つまずいた点）

- **`.rc` は UTF-16LE BOM / CRLF**。filesystem MCP で編集しない。C++ は UTF-8 BOM / CRLF / タブ。`.vcxproj`・`.filters` は既存のまま
- 正規表現のテスト: `LR"(...)"` の中の `\\` は 2 文字。引用符・カンマを含む生文字列を `EXPECT_*` マクロの引数に直接書かない（変数に入れる）。アサーションは `EXPECT_THAT( 実際, マッチャー )`
- `CBregexp::Match()` は照合結果をインスタンスに持つので、複数スレッドで同時に使わない（D2 の並列化で考慮）。`CGrepEnumFilterFiles::MakeFullPath()` も同様
- `CDllImp::InitDll()` は指定名で失敗しても既定の DLL 名を試すので、DLL 読み込み失敗の分岐（2 行）はテストで通らない（C2 のカバレッジは約 90% の見込み【要実測】）
- テストの期待値は机上で追ったものが多い。失敗したら修正指示書を**追補として別に**作る（ルール §1。元の指示書を書き直さない）
- git: `git switch` の前に作業ツリーをきれいにする。ステージ内容は `git diff --cached --stat`、スタッシュは `git stash list` / `git stash show`（スタッシュはインデックスとは別）。`git add .` は使わない（`Claude outputs/`・`git` が未追跡で残っている）
- SonarCloud: 解析は push・PR 後に CI で走る（Draft PR で可）。認知的複雑度 25 以下、New Code カバレッジ 80% 以上。`Attach()` の複雑度は約 7 の見積もり【要実測】

## 5. C3 に着手する前の手順

1. C2 の PR（Draft 可）を push し、SonarCloud の Quality Gate・New Issues・カバレッジを確認する
2. C2 がマージされたら、master を更新する（`git switch master` → `git fetch upstream` → `git merge --ff-only upstream/master`）
3. C3 のブランチを master から作る
4. 文書 04 を C3 の部分だけ作り直す（§3）。着手前に `CCommandLine`・`DoGrep` の現行コードを master で読み、`-GOPT` のフラグ処理の before を実測する
5. `プロジェクトルール_Grep作業_1.md` の §1（成果物）・§4（テスト）・§9（SonarCloud）に従う

## 6. 繰り越し（C 系の範囲外）

- 手組みのコマンドラインの入力チェック（ルール §8-2 #1）
- `:HWND:` の実行中フラグ、`.skrnew` の残り、絶対パス除外の表記ゆれ（§8-2 #3・#4・#6）
- Issue #2715（使えないフォルダーを除いて検索）の指示書は別件として存在する（`修正指示書_Issue2715_使えないフォルダーを除いて検索.md`）
