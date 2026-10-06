# プロジェクトルール（Grep の作業で決めたこと）

`claude_sakura.md` の補足。2026-09 の T1・T2・T3・#2686 の作業と、2026-10 の C1 の作業で決めたこと、つまずいたことをまとめる。

---

## 1. 成果物

| 項目 | ルール |
|---|---|
| 修正指示書 | 実装レベルの **before / after** 形式。before はファイル内で 1 か所だけ一致するものにする。確認コマンドと期待値を書く |
| 修正指示書の範囲 | **今回の作業の修正箇所だけ**を書く。適用済みの修正・作業全体の再掲・過去の経緯は含めない。作業が増えたときは、追補を別の指示書にする |
| 修正指示書の渡し方 | ファイルとして添付し、**ダウンロードできる**ようにする。プロジェクトにも `claude/` で保存する |
| 修正指示書のチャット表示 | 指示書の内容（before / after・手順の全文）は**チャットに表示しない**。チャットには、ファイル名と、何を直すか・どう確かめるかの要点だけを書く |
| 形式 | Markdown ファイル。プロジェクトに `claude/` で保存する |
| 既存ファイルへの追記 | アップロードされたファイルに追記するときは、全体を 1 回で書き出す（手間をかけない） |
| Google ドライブ | 「Markdown で」と言われたら変換せず `.md` のまま保存する |
| 未確認の事項 | 【要実測】と書き、初回の実行結果で確定する。確認していないことを「確認済み」と書かない |
| 回答 | 簡潔な日本語。要点を先に |

## 2. 禁止事項（再掲）

- ビルド・git 操作（add / commit / push / checkout / merge）は AI が行わない。コマンドは案内だけ
- 製品コードを変えるときは、テストのための変更か不具合の修正かを明記する

## 3. ファイル形式

| ファイル | 形式 |
|---|---|
| C++ ソース・テスト・`.hpp` | UTF-8 BOM、CRLF、タブインデント |
| `.rc` | UTF-16LE BOM、CRLF |
| `.vcxproj`・`.filters` | ASCII |

- filesystem MCP で編集すると LF・BOM 無しになるので、指示書に戻し方を書く
- テスト基盤のヘッダーはフォルダーごとに置く（`window/`・`dlg/`・`env/`・`grep/`）。`.vcxproj` に `ClInclude`、`.filters` に `Other Files\<フォルダー>` で登録する

## 4. テストを書くときの注意（MSVC・gtest）

| 項目 | ルール |
|---|---|
| アサーションの書き方 | upstream に合わせて `EXPECT_THAT(実際の値, マッチャー)` で書く（`Eq`・`StrEq`・`IsTrue`・`IsFalse`・`IsNull`・`ElementsAre` など）。`EXPECT_EQ(期待値, 実際の値)` とは引数の順番が逆になるので注意する。出典は #2641 の berryzplus さんのコメント（失敗時に expected 側の名前が出るため）。適用は T3 以降の新しいテストから。T1（#2687）・T2（#2691）の既存テストは、マルチスレッド対応（D1・D2）で該当のテストを触るときに `EXPECT_THAT` に書き換える |
| 引用符を含む文字列 | マクロ（`EXPECT_THAT`・`EXPECT_EQ` など）の引数に `"` を含む生文字列を直接書かない。変数に入れてから渡す（C2017・引数の分割を防ぐ） |
| 生文字列の `\` | `LR"(...)"` の中の `\\` は 2 文字になる。正規表現の `\d` などは `\` 1 つで書く |
| 不具合を検出するテスト | `DISABLED_` を付けて Issue 番号をコメントに書く。修正の PR で外す。`GTEST_SKIP()` は後ろのコードで C4702 が出るので、使うときは `#else` で分ける |
| Debug ビルドでだけ止まるもの | `assert_warning` は `DebugBreak()` を呼ぶ（例: `DoGrep()` の再入）。`#ifdef _DEBUG` で `GTEST_SKIP()` する |
| メッセージボックス | `CheckRegexpSyntax()` の `::MessageBox` も含め、テストでは `MockUser32::MessageBoxExW()` に届く。`EXPECT_CALL` で受ける。期待が満たされないと、次のテストの `SetUp()` で失敗として出る |
| ダイアログのテスト | OK が受け付けられない入力（エラー）では、OK の後にキャンセルを送る。送らないと止まる |
| ウィンドウ数の上限 | `IsFuncEnable()` は上限で `F_GREP_DIALOG` を無効にする。上限にするのはダイアログが開いた後 |
| Grep の出力の確かめ方 | エディターに表示されたテキストは使わない（berryzplus さんの指摘）。`-GOPT=U`（標準出力）＋`H` で受けて、文書の文字コードで戻して比べる。ヒット数は戻り値、置換は置換後のファイル |
| 出力のパス | `CreateFolders()` で長い名前になる。期待値は `GetLongPathNameW()` で作る |
| CI の一時フォルダー | CI の `%TEMP%` は 8.3 形式の短い名前（`C:\Users\RUNNER~1\...`）。`folder.Path()` と製品コードが組み立てたパス（履歴・出力など）を比べるときは、両方を `GetLongPathNameW()` で長い名前に直してから比べる（#2692 `History`）。手元はユーザー名が短いと再現しないので、長い名前のフォルダーの短い名前を `TEMP`・`TMP` に入れて確かめる |
| 出力の形 | 形式 1 でも、B（ベースフォルダー表示）のときは結果の行の先頭に `・` が付く |

## 5. git の手順（案内するとき）

| 場面 | 手順 |
|---|---|
| PR のブランチで作業を始める前 | `git pull --rebase origin <ブランチ>`。レビューする人が「Update branch」で master を取り込むことがある |
| push が「fetch first」で拒否された | `git pull --rebase origin <ブランチ>` してから push。`--force` は使わない |
| master の取り込み | `git fetch upstream` → `git merge upstream/master`（PR のブランチはリベースしない） |
| 手元の master の更新 | `git merge --ff-only upstream/master` |
| push 済みのコミット | `--amend` しない。したら `git reset --soft HEAD@{1}` で戻す |
| `.vcxproj`・`.filters` の衝突 | 別々のブランチで足した登録行が並ぶだけ。目印の 3 行を消し、両方の行を残す |
| 手元だけで無視するファイル | `.git/info/exclude` に書く（`.gitignore` は変えない）。例: `src/test/cpp/tests1/Claude outputs/` |
| ブランチの分け方 | 前の PR に依存するものは、そのブランチから分ける（衝突を避ける） |

## 6. PR・Issue

- Issue を自動で閉じるときは本文に `fix #番号`
- コードブロックの閉じ記号（`~~~`）を貼り残さない
- 不具合で `DISABLED_` にしたテストは、PR 本文に理由と Issue 番号を書く
- CI の `mingw (Release)` と `Build vcpkg (x64-mingw-static)` の失敗は既知（MSYS2 の mingw64 環境の廃止）。放置してよい。MSBuild の 4 ジョブと Test Results で判断する
- SonarCloud の新しい指摘（new issue）が出たら、同じ PR のブランチで直す。直し方は §9

## 7. 進め方（Grep）

- 順番: T1 → T2 → T3 → C1〜C5 → R1〜R3 → D1・D2
- 除外ファイルの正規表現（C 系）は**フルパスで照合**する
- 並列化（D 系）: D1 で直列のまま分け、D2 で実行先を差し替えられる形にする。ワーカーは COM・UI・文書に触れない。出力の順序は毎回同じ。Grep 置換は直列のまま
- 見つけた不具合は Issue にして、テストは `DISABLED_` で入れる（例: #2686、`:HWND:` のフラグ）

## 8. レビューで繰り越した件

レビューで「別件」「対応不要」「このままでよい」とされたもの、PR の範囲外として残したもの。該当する箇所を触る PR で拾う。未 resolve のスレッドは、拾う PR を決めたら返信して resolve する。

### 8-1. コードの書き方

| # | 内容 | 出典 | 扱い |
|---|---|---|---|
| 1 | **メモリの自動解放**: `Clear～()` を呼ばないと漏れる構造自体がおかしい。要素が `vector` なら放っておいても解放されるはず | #2626 `CGrepEnumKeys.h`（未 resolve） | A2（#2631）で型は `std::vector` になった。`Clear～()` が要らなくなっているかを、`CGrepEnumKeys` を触る PR（C1）で確認して片付ける |
| 2 | `std::pair` を範囲 for で回すときは構造化束縛（`for (const auto& [fileName, count] : m_vpItems)`） | #2631 `CGrepEnumFileBase.h`（未 resolve） | `CGrepEnumFileBase.h` を触る PR で直す |
| 3 | パス結合に `std::filesystem::path` の `/=` を使う案 | #2631（対応不要） | 使わない場合は理由を言えるようにしておく |
| 4 | リセットは `*this = Me();`（`using Me = 自クラス名;` 前提） | #2631（対応不要） | 同上 |
| 5 | `BOOL` の戻り値を `== TRUE` で比較しない。新しい関数は `bool`。既存の `BOOL` の一括変更は別 PR | #2631 | 新しく書くコードで守る。一括変更は未着手 |
| 6 | 3 つ以上の文字列を `+` でつなぐより `std::format` | #2649 `charset.cpp` | `Bracket()` は #2676 で対応済み。ほかの箇所は、Grep の出力処理の整理（R 系）で拾う |
| 6a | **書式付き文字列の移行順**: `auto_sprintf`（バッファサイズを考慮しない。新規では使わない）→ `auto_sprintf_s`（サイズを考慮）→ `strprintf`（文字配列が要らない独自関数）→ `std::format`（書式の指定形式が異なる）。`lineColumnToString()` などの C 形式の配列（`auto_sprintf` が可変長引数の C 関数で、配列参照＋`static_assert` でサイズを確かめているため）もいずれ移す | #2641（berryzplus「対応はいずれ必要」「この PR では対応不要」） | 別テーマの PR で行う。新しく書くコードでは `auto_sprintf` を増やさない |
| 7 | テストのローカル変数 `pszBracket` が残っている | #2676 `test-charset.cpp`（未 resolve） | 次に `test-charset.cpp` を触るときに直す |
| 8 | `GetNameBracket(std::span)` に長さ 0 の span が来たときのガード | #2676 `CCodePage.cpp`（Copilot、resolve 済み） | 入れていない。必要になったら `outName.empty()` で `std::invalid_argument` |
| 9 | `LS()` の `thread_local` の説明コメントを `.cpp` 側にも | #2679 `CSelectLang.h`（対応不要） | 次に `CSelectLang.cpp` を触るときに足す |
| 10 | `test-loadstring.cpp` のスレッドのテストが、確かめたいことを確かめていない（別スレッドの後に、先に取得した文字列が変わっていないことを見ている） | #2679（未 resolve） | 妥当な確かめ方が決まっていない。D2 の前に見直す |
| 11 | テストのアサーションは `EXPECT_THAT` | #2641 | 新しいテストは T3 以降から。T1・T2 などの既存テストは、マルチスレッド対応（D1・D2）のときに書き換える（§4） |
| 12 | **`msCodeSet` の排他**: 更新は初期化の 1 回だけで `std::call_once` で排他済み。取得側（`UseBom()`・`IsBomDefOn()`・`CanDefault()` は `find()`、`GetName()` は `vDispIdx` 経由）は挿入しないのでロック不要。mutex を入れると名前の取得のたびにワーカーが直列化する | #2649（berryzplus さんの指摘に回答済み） | 現状のまま。文字コード関連の処理は散在しているので、最終的には `TSakuraSingleton` 派生のグローバルインスタンスにキャッシュする方向（berryzplus さんの案に賛成済み）。段階的に進める |
| 13 | **プロジェクトファイルのパス**: `sakura.vcxproj` は `sakura_core` に移動済みなので、`sakura_core` 配下のファイルは `grep\GrepMessageFormat.h` のように相対で書ける（`..\sakura_core\...` は不要）。新規ファイルは `src\main\cpp\` 配下に置くのが berryzplus さんの好み（「`sakura_core` の core ってなに？」） | #2641 `sakura.vcxproj` | 既存コードを変えない PR では `sakura_core` 配下でよい。新しいファイルを足すときは `src\main\cpp\` を検討する |

### 8-2. 機能・仕様

| # | 内容 | 出典 | 扱い |
|---|---|---|---|
| 1 | 手組みのコマンドラインから Grep するときの入力チェック（引用符の閉じ忘れなど）。`CDlgGrep::GetData()` のガードはダイアログ経由なら必ず通るが、コマンドライン経由は通らない。対策は「ダメならダイアログを出す」か「ダメなら stderr にエラーを出す」の 2 通り | #2682 `CDlgGrep.cpp`（berryzplus さんの課題、未 resolve） | 未着手。CI のテストを考えると stderr、使いやすさならダイアログに寄せる（berryzplus さんは後者寄り） |
| 2 | 長いコマンドライン引数（16,000 文字超）で `DebugOutW` があふれ、Debug ビルドで `DebugBreak()` | #2596（PR-W） | 未着手。`CCommandLine` の全オプションに文字数制限を付ける別 PR |
| 3 | 無効な `:HWND:` の後、実行中フラグが戻らない | T2 で発見 | Issue を立てて `DISABLED_InvalidHwndTargetResetsRunningFlag` の `#xxxx` を置き換える。修正の PR で `DISABLED_` を外す |
| 4 | Grep 置換で元のファイルを消せない・移せないとき、`.skrnew` が残る | T2 で発見 | 【現状の制限】としてテストに固定。未着手 |
| 5 | 除外ファイルが、フォルダー部分を含む検索キー（`sub\*`）で見つかったファイルに効かない | T1 で発見 | 【現状の制限】としてテストに固定。未着手 |
| 6 | 絶対パスの除外は完全一致なので、大文字小文字や 8.3 形式など表記が違うと効かない | #2686 の範囲外 | 未着手 |
| 7 | `DoGrep()` の再入は Debug ビルドでは `DebugBreak()` するので、`RejectsReentry` は Release でしか確かめられない | T2 | 現状のまま |

## 9. SonarCloud の指摘

| 項目 | ルール |
|---|---|
| 認知的複雑度（Cognitive Complexity） | 1 つの関数は **25 以下**。超えると new issue になる。分岐（`if`・`else if`・`for`・`&&`・`||`・三項演算子）が増えるほど、入れ子が深いほど大きく数えられる |
| 既に分岐の多い関数に分岐を足すとき | 足す前に、複雑度がどれだけ増えるかを見積もる。ループの中に分岐を入れると、入れ子の分だけ重く数えられる。足した結果が 25 を超えそうなら、最初から 1 要素ぶんの処理を private 関数に切り出す |
| 直し方 | 挙動を変えずに、ループ内の処理を関数に切り出す。既存のテストで同じ結果になることを確かめる（件数も変えない）。直した後は SonarCloud の再解析で、元の指摘が消えたことと、切り出した関数に新しい指摘が出ていないことを確かめる |
| 例（C1） | `CGrepEnumKeys::SetFileKeys()` が 30 になった（上限 25）。ループ内の「種類判定〜振り分け」を `AddFileKey()` に切り出した（`SetFileKeys()` 約 5・`AddFileKey()` 約 22 の見積もり。再解析で確認） |
| 指摘を直す指示書 | 指摘 1 件ごとに、修正箇所だけの指示書を別に作る（§1）。元の PR の指示書には混ぜない |
| カバレッジ | New Code のカバレッジは 80% 以上。新しい分岐にはテストを付ける |
