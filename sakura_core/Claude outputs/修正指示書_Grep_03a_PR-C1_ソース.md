# 修正指示書 03a: PR-C1 — 除外ファイル正規表現のキー振り分けと照合関数のフック（ソース）

文書 03（C1・C2）の C1 だけを、現在の upstream master に合わせて独立させたもの。C2 は 03 の §2 をそのまま使う（テストの追加先は本書 C1-12 のファイル）。

| 項目 | 内容 |
|---|---|
| 目的 | 除外ファイルを正規表現で指定するための部品（キーの振り分けと照合関数のフック）を、振る舞いを変えずに入れる |
| 基準 | upstream master `3a7c59b9`（T3 #2692 マージ後。2026-10-02）。`CGrepEnumKeys.h`・`CGrepEnumFilterFiles.h` は T3 で変わっていない（T3 はテストと vcxproj のみ）ので `e8833121` で確認した before がそのまま有効。`.filters` は `3a7c59b9` から読み直して確認済み |
| ブランチ（案） | `feature/grep-exclude-regex-keys`（`upstream/master` から） |
| 振る舞い | 変化なし（正規表現モードを使う呼び出し元は C3 で入る。`SetFileKeys()` の既定値は `false`） |
| 変更ファイル | `sakura_core/grep/CGrepEnumKeys.h`、`sakura_core/grep/CGrepEnumFilterFiles.h`、`src/test/cpp/tests1/test-grep-exclude-regex.cpp`（新規）、`sakura_core/tests1.vcxproj`、`sakura_core/tests1.vcxproj.filters` |
| 追加テスト | 16 件（すべて新規ファイル。製品コードの変更は「テストのための変更」ではなく C 系の機能の土台） |
| ファイル形式 | C++ ソース・テストは UTF-8 BOM / CRLF / タブインデント。`.vcxproj`・`.filters` は既存のまま（BOM 付き・CRLF） |
| 禁止事項 | ビルド・git 操作は AI が行わない。PR 本文は作成しない（今回の依頼で不要） |

仕様は文書 00 の §3。**照合対象はフルパスではなくファイル名（フォルダーを含まない）、大文字小文字は区別しない**（C3 の PR 説明に明記する。§0 参照）。

---

## 0. 文書 03 からの変更点と確認結果

| # | 内容 |
|---|---|
| 1 | **基準を `9127a1c1` → `3a7c59b9` に更新。** `CGrepEnumKeys.h` の Copyright は既に 2018-2026。`CGrepEnumFilterFiles.h` は 2018-2022 のまま（C1-8 で更新）。C1 の before はすべて一致 |
| 2 | **vcxproj の場所は `sakura_core/tests1.vcxproj`**（`..\src\test\...` の `..` はここから見たもの）。03 の before は `test-grep-dialog.cpp` を目印にしていたが、本書は `test-grepenum.cpp` を目印にして直前に挿入する形に変えた（T3 マージ後は `test-grep-dialog.cpp` の後ろ、アルファベット順で `test-grep-dialog` < `test-grep-exclude-regex` < `test-grepenum`）。03 の before でも当たるが、本書の方が目印が単純 |
| 3 | **テストのアサーションを `EXPECT_THAT(実際の値, マッチャー)` に書き直した**（プロジェクトルール §4。新規テストなので適用）。03 の `ToStrings()` は不要になったので削除。`ElementsAre` は `pch.h` に using が無いので、テストファイルの先頭で宣言する（`Eq`・`IsEmpty`・`IsTrue`・`IsFalse` は `pch.h` にある） |
| 4 | 区切りの `,` を含む生文字列（`\d{2,4}` など）は、マクロの外の変数に入れてから渡す形にした（プロジェクトルール §4 の引用符の注意の念のための拡大。実際に必要かは 【要実測】） |
| 5 | **仕様の食い違い**: プロジェクトルール §7 の「C 系はフルパスで照合する」は古い。文書 03・04 は「ファイル名で照合」に変更済み（#2459 からの変更点）。本書は 03・04 に従う。**ルール §7 の更新を推奨**（下の「ルール更新案」） |
| 6 | **繰越 8-1 #1（メモリの自動解放、#2626 `CGrepEnumKeys.h` の未 resolve スレッド）の確認結果**: 要素は `std::vector<std::wstring>` で自動解放される。`ClearItems()` は private で `SetFileKeys()` の先頭からだけ呼ばれ、「再解析のときに前回の結果を消す」ためのもの。**漏れる構造ではない**ので、コードは変えず、スレッドに「A2 で `std::vector` になり、`Clear～()` は再解析のリセットのみ。メモリ管理には関係しない」と返信して resolve できる。C1 は `ClearItems()` に 2 行足す（C1-7）が、方針は変わらない |
| 7 | 先に指摘した `.filters` の `test-grep-cmdline.cpp` エントリの欠けは、T3 #2692 のマージで直っていることを `3a7c59b9` で確認した（`<Filter>`・`</ClCompile>` あり）。対応不要 |

**ルール更新案（`プロジェクトルール_Grep作業.md` §7）**

```
- 除外ファイルの正規表現（C 系）は**ファイル名（フォルダーを含まない）で照合**し、大文字小文字を区別しない（#2459 からの変更。文書 00 §3）
```

---

## 1. 変更の概要

| 変更 | 内容 |
|---|---|
| `CGrepEnumKeys` | `m_vecExceptFileRegexKeys` と照合関数 `m_fnIsExceptFileName` を追加。`SetFileKeys( lpKeys, bExceptFileRegex = false )` で、正規表現モードなら除外ファイル（`!`）をパスとして検査せずに正規表現の配列へ入れる（空は入れない）。`IsExceptFileName()` を追加。`GetExcludeFiles()` に正規表現を含める。`ClearItems()` で両方を消す |
| `CGrepEnumFilterFiles` | `Enumerates()` でキーへのポインタを覚え、`IsValid()` でファイル名（`w32fd.cFileName`）を照合関数に渡す |

### 修正 C1-1: CGrepEnumKeys.h: include

対象: `sakura_core/grep/CGrepEnumKeys.h`

**before**

```
#include <algorithm>
#include <list>
```

**after**

```
#include <algorithm>
#include <functional>
#include <list>
```

### 修正 C1-2: CGrepEnumKeys.h: メンバーの追加

**before**

```
	VGrepEnumKeys m_vecExceptAbsFolderKeys;

public:
```

**after**

```
	VGrepEnumKeys m_vecExceptAbsFolderKeys;
	VGrepEnumKeys m_vecExceptFileRegexKeys;	//!< 除外ファイル(正規表現)。SetFileKeys() の bExceptFileRegex が true のときに使う

	//! 除外ファイル名の照合関数(正規表現)。未設定なら照合しない
	std::function<bool(std::wstring_view)> m_fnIsExceptFileName;

public:
```

- `CGrepEnumKeys` はコピー・ムーブ禁止のクラス。`std::function` のメンバーを足しても既定コンストラクターの `noexcept` は保たれる（`std::function` の既定コンストラクターは `noexcept`）

### 修正 C1-3: CGrepEnumKeys.h: GetExcludeFiles() に正規表現を含める

**before**

```
		const auto& absFileKeys = m_vecExceptAbsFileKeys;
		excludeFiles.insert( excludeFiles.cend(), absFileKeys.cbegin(), absFileKeys.cend() );
		return excludeFiles;
```

**after**

```
		const auto& absFileKeys = m_vecExceptAbsFileKeys;
		excludeFiles.insert( excludeFiles.cend(), absFileKeys.cbegin(), absFileKeys.cend() );
		const auto& regexFileKeys = m_vecExceptFileRegexKeys;
		excludeFiles.insert( excludeFiles.cend(), regexFileKeys.cbegin(), regexFileKeys.cend() );
		return excludeFiles;
```

- Grep 結果の先頭の「除外ファイル」の表示に使われる。正規表現もそのまま表示される
- 直前のコメント「除外ファイルの2つの解析済み配列から1つのリストを作る」は 3 つになるが、既存コメントなので触らない（表面的な修正を避ける）

### 修正 C1-4: CGrepEnumKeys.h: SetFileKeys() の引数

**before**

```
	int SetFileKeys( LPCWSTR lpKeys ){
```

**after**

```
	/*!
		@brief ファイルパターンを解析して、種類ごとの配列に振り分ける
		@param[in]	lpKeys				ファイルパターン
		@param[in]	bExceptFileRegex	true なら除外ファイル(!)を正規表現として m_vecExceptFileRegexKeys に入れる
		@retval 0 正常
		@retval 0以外 エラー(ValidateKey() の戻り値、または絶対パスの検索対象で 2)
	*/
	int SetFileKeys( LPCWSTR lpKeys, bool bExceptFileRegex = false ){
```

- 既定値 `false` なので既存の呼び出し元（`CGrepAgent::DoGrep()`、`CDlgGrep::GetData()`）はそのまま

### 修正 C1-5: CGrepEnumKeys.h: 正規表現の振り分け

**before**

```
			}else if( token[0] == L'#' ){
				token++;
				keyType = FILTER_EXCEPT_FOLDER;
			}

			bool bRelPath = _IS_REL_PATH( token );
```

**after**

```
			}else if( token[0] == L'#' ){
				token++;
				keyType = FILTER_EXCEPT_FOLDER;
			}

			// 除外ファイルを正規表現として扱うときは、パスとしての検査(ValidateKey・絶対パス)をしない
			if( bExceptFileRegex && keyType == FILTER_EXCEPT_FILE ){
				if( token[0] != L'\0' ){
					push_back_unique( m_vecExceptFileRegexKeys, token );
				}
				continue;
			}

			bool bRelPath = _IS_REL_PATH( token );
```

- `ValidateKey()` を通さない理由: `.*\.txt$` のような正規表現は「フォルダー部分にワイルドカード」と判定されてエラーになる
- 空のパターン（`!` だけ）は入れない。空の正規表現はすべてのファイル名に一致し、全ファイルが除外されるため
- 先頭の `!` を外すのは 1 回だけ（`!!x` → 正規表現 `!x`、`!#y` → 正規表現 `#y`）。既存の振り分けと同じ

### 修正 C1-6: CGrepEnumKeys.h: IsExceptFileName() の追加

**before**

```
	int AddExceptFolder(LPCWSTR lpKeys) {
		return ParseAndAddException(lpKeys, m_vecExceptFolderKeys, m_vecExceptAbsFolderKeys);
	}
```

**after**

```
	int AddExceptFolder(LPCWSTR lpKeys) {
		return ParseAndAddException(lpKeys, m_vecExceptFolderKeys, m_vecExceptAbsFolderKeys);
	}

	/*!
		@brief ファイル名が除外ファイル(正規表現)に一致するか調べる
		@param[in]	fileName	フォルダーを含まないファイル名
		@retval false 一致しない、または照合関数が未設定
	*/
	bool IsExceptFileName( std::wstring_view fileName ) const {
		return m_fnIsExceptFileName && m_fnIsExceptFileName( fileName );
	}
```

### 修正 C1-7: CGrepEnumKeys.h: ClearItems()

**before**

```
		m_vecExceptAbsFolderKeys.clear();
		return;
```

**after**

```
		m_vecExceptAbsFolderKeys.clear();
		m_vecExceptFileRegexKeys.clear();
		m_fnIsExceptFileName = nullptr;
		return;
```

- `SetFileKeys()` を呼び直したときに、前回の正規表現と照合関数が残らないようにする

### 修正 C1-8: CGrepEnumFilterFiles.h: Copyright

対象: `sakura_core/grep/CGrepEnumFilterFiles.h`

**before**

```
	Copyright (C) 2018-2022, Sakura Editor Organization
```

**after**

```
	Copyright (C) 2018-2026, Sakura Editor Organization
```

### 修正 C1-9: CGrepEnumFilterFiles.h: メンバーの追加

**before**

```
public:
	CGrepEnumFiles m_cGrepEnumExceptFiles;

```

**after**

```
public:
	CGrepEnumFiles m_cGrepEnumExceptFiles;

	//! 除外ファイル(正規表現)の照合に使う。Enumerates() で設定する
	const CGrepEnumKeys* m_pGrepEnumKeys = nullptr;

```

### 修正 C1-10: CGrepEnumFilterFiles.h: IsValid()

**before**

```
			if( m_cGrepEnumExceptFiles.IsValid( w32fd, pFile ) ){
				return TRUE;
			}
```

**after**

```
			if( m_cGrepEnumExceptFiles.IsValid( w32fd, pFile ) ){
				// 除外ファイル(正規表現)はフォルダーを含まないファイル名で照合する
				if( m_pGrepEnumKeys && m_pGrepEnumKeys->IsExceptFileName( w32fd.cFileName ) ){
					return FALSE;
				}
				return TRUE;
			}
```

- `pFile` は検索キーのフォルダー部分（`subdir\*.h` の `subdir\`）を含むことがあるので使わない。`w32fd.cFileName` はファイル名だけ
- ディレクトリは先頭の `CGrepEnumFiles::IsValid()` が除くので、照合関数はファイルにしか呼ばれない（テスト `MatcherNotCalledForFolders` で確かめる）
- 除外ワイルドカードを列挙する `m_cGrepEnumExceptFiles` は基底の `CGrepEnumFiles::IsValid()` を使うので、照合関数は呼ばれない

### 修正 C1-11: CGrepEnumFilterFiles.h: Enumerates()

**before**

```
	int Enumerates( LPCWSTR lpBaseFolder, CGrepEnumKeys& cGrepEnumKeys, CGrepEnumOptions option, CGrepEnumFiles& pExcept ){
```

**after**

```
	int Enumerates( LPCWSTR lpBaseFolder, CGrepEnumKeys& cGrepEnumKeys, CGrepEnumOptions option, CGrepEnumFiles& pExcept ){
		m_pGrepEnumKeys = &cGrepEnumKeys;
```

- `CGrepEnumFileBase::Enumerates()` の中から仮想関数 `IsValid()` が呼ばれるので、その前に設定する

### 修正 C1-12: 新規: 除外ファイルの正規表現のテスト

対象: `src/test/cpp/tests1/test-grep-exclude-regex.cpp`（新規。UTF-8 BOM / CRLF）

C1〜C2 の「新しい仕組み」のテストをこのファイルにまとめる（C2 は 03 の修正 C2-8 で同じファイルに足す。C2-8 のテストはアサーションを `EXPECT_THAT` に直してから足す）。既存の `test-cgrepenumkeys.cpp`・`test-grepenum.cpp` は現状の振る舞いを固定するテストなので混ぜない。C3〜C5 の実行テストは T2・T3 の fixture を使うので `test-grep-cmdline.cpp`・`test-grep-dialog.cpp` に置く（文書 04 のまま）。

```cpp
/*! @file */
/*
	Copyright (C) 2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "grep/CGrepEnumKeys.h"
#include "grep/CGrepEnumFiles.h"
#include "grep/CGrepEnumFilterFiles.h"
#include "grep/GrepTestSuite.hpp"	// grep_test::TempFolder

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

/*
	除外ファイルの正規表現のテスト

	CGrepEnumKeys: 除外ファイル(正規表現)のキーの振り分けと照合関数
	CGrepEnumFilterFiles: 照合関数による列挙の除外
	CGrepExceptFileRegexps: 正規表現の照合器

	コマンドライン・ダイアログからの実行のテストは、Grep の実行基盤(GrepTestSuite)を使うので
	test-grep-cmdline.cpp・test-grep-dialog.cpp に置く。

	注意: 引用符を含む生文字列はマクロの引数に書かない(MSVC の従来のプリプロセッサーで C2017 やマクロ引数の分割が起きる)。
	変数に入れてからマクロに渡す。区切りの「,」を含む文字列も同様に変数に入れる。
*/

using ::testing::ElementsAre;	// pch.h に using が無いのでここで宣言する

namespace {

using grep_test::TempFolder;

//! 列挙結果の名前を昇順(符号単位)で返す
std::vector<std::wstring> Names(const CGrepEnumFileBase& items)
{
	std::vector<std::wstring> names;
	for (int i = 0; i < items.GetCount(); ++i) {
		names.emplace_back(items.GetFileName(i));
	}
	std::ranges::sort(names);
	return names;
}

} // namespace

// ---------------------------------------------------------------------------
// キーの振り分け(CGrepEnumKeys)
// ---------------------------------------------------------------------------

/*!
	@brief 正規表現モードでは除外ファイル(!)が正規表現の配列に振り分けられること
*/
TEST(CGrepEnumKeysExceptRegex, Classify)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(LR"(*.cpp;!^[^.]+$;!\.bak$;#obj)", true), Eq(0));

	EXPECT_THAT(keys.m_vecSearchFileKeys, ElementsAre(L"*.cpp"));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, ElementsAre(L"^[^.]+$", LR"(\.bak$)"));
	EXPECT_THAT(keys.m_vecExceptFileKeys, IsEmpty());
	EXPECT_THAT(keys.m_vecExceptAbsFileKeys, IsEmpty());
	EXPECT_THAT(keys.m_vecExceptFolderKeys, ElementsAre(L"obj"));
}

/*!
	@brief 正規表現モードではパスとしての検査をしないこと
*/
TEST(CGrepEnumKeysExceptRegex, SkipsPathValidation)
{
	CGrepEnumKeys keys;
	// ワイルドカードとして扱うと、フォルダー部分に * があるのでエラー
	EXPECT_THAT(keys.SetFileKeys(LR"(!.*\.txt$)"), Eq(1));
	// 正規表現として扱うときはエラーにならない。絶対パスに見えるものも正規表現として扱う
	EXPECT_THAT(keys.SetFileKeys(LR"(!.*\.txt$;!C:\a$)", true), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, ElementsAre(LR"(.*\.txt$)", LR"(C:\a$)"));
	EXPECT_THAT(keys.m_vecExceptAbsFileKeys, IsEmpty());
}

/*!
	@brief 空の正規表現は入れないこと(すべてのファイルが除外されるのを防ぐ)
*/
TEST(CGrepEnumKeysExceptRegex, IgnoresEmpty)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"*.cpp;!", true), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, IsEmpty());
}

/*!
	@brief SetFileKeys() を呼び直すと、正規表現と照合関数がクリアされること
*/
TEST(CGrepEnumKeysExceptRegex, ClearsOnReentry)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"!^a$", true), Eq(0));
	keys.m_fnIsExceptFileName = [](std::wstring_view) { return true; };

	ASSERT_THAT(keys.SetFileKeys(L"*.cpp"), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, IsEmpty());
	EXPECT_THAT(keys.IsExceptFileName(L"a"), IsFalse());
}

/*!
	@brief 除外ファイルの一覧(結果の先頭に表示するもの)に正規表現も含まれること
*/
TEST(CGrepEnumKeysExceptRegex, GetExcludeFilesIncludesRegex)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"!^a$;!^b$", true), Eq(0));
	EXPECT_THAT(keys.GetExcludeFiles(), ElementsAre(L"^a$", L"^b$"));
}

/*!
	@brief 照合関数が未設定なら一致しない扱い、設定されていればその結果になること
*/
TEST(CGrepEnumKeysExceptRegex, IsExceptFileName)
{
	CGrepEnumKeys keys;
	EXPECT_THAT(keys.IsExceptFileName(L"README"), IsFalse());

	keys.m_fnIsExceptFileName = [](std::wstring_view fileName) { return fileName == L"README"; };
	EXPECT_THAT(keys.IsExceptFileName(L"README"), IsTrue());
	EXPECT_THAT(keys.IsExceptFileName(L"a.txt"), IsFalse());
}

/*!
	@brief 正規表現モードでなければ、除外ファイルは従来どおりワイルドカードとして振り分けられること
*/
TEST(CGrepEnumKeysExceptRegex, OffKeepsWildcard)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"!*.bak"), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileKeys, ElementsAre(L"*.bak"));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, IsEmpty());
}

/*!
	@brief 同じ正規表現は 1 つにまとめられること
*/
TEST(CGrepEnumKeysExceptRegex, Unique)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"!^a$;!^a$", true), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, ElementsAre(L"^a$"));
}

/*!
	@brief 区切り文字(空白・;・,)を含む正規表現は引用符で囲めば分割されないこと
*/
TEST(CGrepEnumKeysExceptRegex, QuotedDelimiters)
{
	const std::wstring pattern = LR"(!"^a b$";!"\d{2,4}";!"a;b")";	// 引用符を含むのでマクロの外で作る
	const std::wstring digits = LR"(\d{2,4})";

	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(pattern.c_str(), true), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, ElementsAre(L"^a b$", digits, L"a;b"));
}

/*!
	@brief 引用符で囲まない「,」では分割されること(引用符が必要なことの確認)
*/
TEST(CGrepEnumKeysExceptRegex, UnquotedCommaSplits)
{
	const std::wstring pattern = LR"(!\d{2,4})";	// 「,」を含むのでマクロの外で作る

	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(pattern.c_str(), true), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, ElementsAre(LR"(\d{2)"));
	EXPECT_THAT(keys.m_vecSearchFileKeys, ElementsAre(L"4}"));
}

/*!
	@brief 正規表現モードでも、除外フォルダーと検索対象の検査は従来どおりであること
*/
TEST(CGrepEnumKeysExceptRegex, OtherRulesUnchanged)
{
	CGrepEnumKeys keys;
	EXPECT_THAT(keys.SetFileKeys(LR"(#a*\b)", true), Eq(1));
	EXPECT_THAT(keys.SetFileKeys(LR"(sub*\a.txt)", true), Eq(1));
	EXPECT_THAT(keys.SetFileKeys(LR"(C:\x\*.txt)", true), Eq(2));
}

/*!
	@brief 先頭の「!」を 1 つだけ取り除き、残りはそのまま正規表現になること
*/
TEST(CGrepEnumKeysExceptRegex, PrefixCharsInPattern)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"!!x;!#y", true), Eq(0));
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, ElementsAre(L"!x", L"#y"));
	EXPECT_THAT(keys.m_vecExceptFolderKeys, IsEmpty());
}

/*!
	@brief 除外ファイルしか指定しないとき、検索対象は既定の「*.*」になること
*/
TEST(CGrepEnumKeysExceptRegex, OnlyExcludes)
{
	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"!^a$", true), Eq(0));
	EXPECT_THAT(keys.m_vecSearchFileKeys, ElementsAre(L"*.*"));
}

// ---------------------------------------------------------------------------
// 照合関数のフック(CGrepEnumFilterFiles)
// ---------------------------------------------------------------------------

/*!
	@brief 照合関数に一致したファイルが列挙から除かれること。照合関数が無ければすべて列挙されること
*/
TEST(CGrepEnumFilterFilesExceptRegex, ExceptByFileNameMatcher)
{
	TempFolder folder;
	for (const auto name : { L"a.txt", L"b.log", L"README" }) {
		folder.AddFile(name, "x");
	}

	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"*", true), Eq(0));
	{
		CGrepEnumFilterFiles files;
		CGrepEnumFiles absExcept;
		files.Enumerates(folder.Path().c_str(), keys, CGrepEnumOptions(), absExcept);
		EXPECT_THAT(Names(files), ElementsAre(L"README", L"a.txt", L"b.log"));
	}

	keys.m_fnIsExceptFileName = [](std::wstring_view fileName) { return fileName == L"README"; };
	{
		CGrepEnumFilterFiles files;
		CGrepEnumFiles absExcept;
		files.Enumerates(folder.Path().c_str(), keys, CGrepEnumOptions(), absExcept);
		EXPECT_THAT(Names(files), ElementsAre(L"a.txt", L"b.log"));
	}
}

/*!
	@brief 照合関数にはフォルダー部分を含まないファイル名が渡されること
*/
TEST(CGrepEnumFilterFilesExceptRegex, MatcherGetsFileNameOnly)
{
	TempFolder folder;
	folder.AddFile(LR"(sub\x.txt)", "x");

	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(LR"(sub\*.txt)", true), Eq(0));
	std::vector<std::wstring> calledNames;
	keys.m_fnIsExceptFileName = [&calledNames](std::wstring_view fileName) {
		calledNames.emplace_back(fileName);
		return true;
	};

	CGrepEnumFilterFiles files;
	CGrepEnumFiles absExcept;
	files.Enumerates(folder.Path().c_str(), keys, CGrepEnumOptions(), absExcept);
	EXPECT_THAT(calledNames, ElementsAre(L"x.txt"));
	EXPECT_THAT(files.GetCount(), Eq(0));
}

/*!
	@brief フォルダーには照合関数が呼ばれないこと
*/
TEST(CGrepEnumFilterFilesExceptRegex, MatcherNotCalledForFolders)
{
	TempFolder folder;
	folder.AddFolder(L"d");
	folder.AddFile(L"f", "x");

	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(L"*", true), Eq(0));
	std::vector<std::wstring> calledNames;
	keys.m_fnIsExceptFileName = [&calledNames](std::wstring_view fileName) {
		calledNames.emplace_back(fileName);
		return false;
	};

	CGrepEnumFilterFiles files;
	CGrepEnumFiles absExcept;
	files.Enumerates(folder.Path().c_str(), keys, CGrepEnumOptions(), absExcept);
	EXPECT_THAT(calledNames, ElementsAre(L"f"));
}
```

- **ソースのコメントに作業用の名称（T2・T3・C1・C2 など）を書かない**（upstream の読み手には通じない）。テストファイルの冒頭コメントとセクション区切りは、クラス名・機能名で書いた。03 の C2-8 のセクション区切り `// C2: 照合器(CGrepExceptFileRegexps)` も、C2 の指示書を作るときに `// 照合器(CGrepExceptFileRegexps)` に直す
- テストスイート名は既存の `CGrepEnumKeys`・`CGrepEnumFilterFiles` と分け、`CGrepEnumKeysExceptRegex`・`CGrepEnumFilterFilesExceptRegex` にする。`--gtest_filter=*ExceptRegex*` で C 系だけを実行できる
- 一時フォルダーは T2 の `grep_test::TempFolder`（`grep/GrepTestSuite.hpp`）を使う。T1 の `test-grepenum.cpp` の補助クラスは無名名前空間の中にあり共有できない
- 生文字列の `\` は 1 つ（`LR"(\.bak$)"` は `\.bak$` の 6 文字）。`\\` と書くと「`\` の後に任意の 1 文字」という別の意味になる
- `GetExcludeFiles()` は一時の `std::vector` を返すので、`EXPECT_THAT` にそのまま渡せる
- `CGrepEnumFilterFiles::Enumerates()` の第 3 引数は値渡しの `CGrepEnumOptions` なので、`CGrepEnumOptions()` を直接渡せる（基底の `CGrepEnumFileBase::Enumerates()` は非 const 参照）

### 修正 C1-13: tests1.vcxproj

対象: `sakura_core/tests1.vcxproj`

**before**

```
    <ClCompile Include="..\src\test\cpp\tests1\test-grepenum.cpp" />
```

**after**

```
    <ClCompile Include="..\src\test\cpp\tests1\test-grep-exclude-regex.cpp" />
    <ClCompile Include="..\src\test\cpp\tests1\test-grepenum.cpp" />
```

### 修正 C1-14: tests1.vcxproj.filters

対象: `sakura_core/tests1.vcxproj.filters`

**before**

```
    <ClCompile Include="..\src\test\cpp\tests1\test-grepenum.cpp">
      <Filter>Test Files</Filter>
    </ClCompile>
```

**after**

```
    <ClCompile Include="..\src\test\cpp\tests1\test-grep-exclude-regex.cpp">
      <Filter>Test Files</Filter>
    </ClCompile>
    <ClCompile Include="..\src\test\cpp\tests1\test-grepenum.cpp">
      <Filter>Test Files</Filter>
    </ClCompile>
```

- `tests1.cmake` は `GLOB_RECURSE` なので変更不要
- T3 のマージ後は、`test-grep-dialog.cpp` の行が `test-grep-cmdline.cpp` と `test-grepenum.cpp` の間にある。本書の挿入位置は `test-grepenum.cpp` の直前（`test-grep-dialog.cpp` の後ろ）で、アルファベット順が保たれる

---

## 2. 注意

- **新規の `test-grep-exclude-regex.cpp` は UTF-8 BOM / CRLF で保存する**（エディターで新規作成すると BOM なし・LF になることがある）
- **引用符・`,` を含む生文字列をマクロの引数に書かない。** 変数に入れてから渡す（T1 の C2017 と同じ原因）
- `grep/GrepTestSuite.hpp` はエディターの基盤（`EditorTestSuite` など）を含むので、このテストのコンパイルはやや重くなる。`TempFolder` だけが目的なので、ビルド時間の増え方を 【要実測】。目立って重ければ、`TempFolder` を別ヘッダーに切り出す案を berryzplus さんに相談する（C1 の範囲外）
- `m_pGrepEnumKeys` は生ポインターで、`Enumerates()` に渡した `cGrepEnumKeys` の寿命の間だけ有効。`CGrepEnumFilterFiles` は `DoGrepTree()` のローカルなので問題ない（C3 の実装時に再確認する）
- `std::function` を呼ぶ照合関数は C2 の `CBregexp::Match()` を使い、**スレッドセーフではない**（D2 の並列化で考慮する。03 §2-1 のとおり）
- 新しく書いたコードは `bool`（`BOOL` を使わない）。既存の `IsValid()` の `BOOL` はそのまま（プロジェクトルール §8-1 #5）

## 3. テストの一覧

| テスト | 確かめること | 種類 |
|---|---|---|
| `CGrepEnumKeysExceptRegex.Classify` | 正規表現・検索・除外フォルダーの振り分け | 正常 |
| `SkipsPathValidation` | パスとして不正な形（`*` を含む、`C:` で始まる）もエラーにならない | イレギュラー |
| `IgnoresEmpty` | `!` だけは入れない | イレギュラー |
| `ClearsOnReentry` | 呼び直しで正規表現と照合関数が消える | 状態 |
| `GetExcludeFilesIncludesRegex`・`IsExceptFileName` | 表示用の一覧、照合関数の有無 | 正常・境界 |
| `OffKeepsWildcard` | 正規表現モードでなければ従来どおり | 回帰 |
| `Unique` | 同じ正規表現は 1 つ | 境界 |
| `QuotedDelimiters`・`UnquotedCommaSplits` | 区切り文字を含む正規表現と引用符の有無 | イレギュラー |
| `OtherRulesUnchanged` | 除外フォルダー・検索対象の検査は変わらない | 回帰 |
| `PrefixCharsInPattern` | `!!x` → `!x`、`!#y` → `#y` | イレギュラー |
| `OnlyExcludes` | 除外しか無ければ検索対象は `*.*` | 境界 |
| `CGrepEnumFilterFilesExceptRegex.ExceptByFileNameMatcher` | 照合関数の有無で列挙が変わる | 正常 |
| `CGrepEnumFilterFilesExceptRegex.MatcherGetsFileNameOnly` | `sub\*.txt` でも照合関数には `x.txt` だけ | イレギュラー |
| `CGrepEnumFilterFilesExceptRegex.MatcherNotCalledForFolders` | フォルダーには呼ばれない | イレギュラー |

## 4. 自己チェック・コミット

```cmd
git diff --stat
git grep -n -e "m_vecExceptFileRegexKeys" -e "m_fnIsExceptFileName" -e "m_pGrepEnumKeys" -- sakura_core/
x64\Debug\tests1.exe --gtest_filter=*ExceptRegex*:CGrepEnumKeys.*:CGrepEnumFilterFiles.*
```

**期待**:

- `git diff --stat`: 5 files changed（`CGrepEnumKeys.h`・`CGrepEnumFilterFiles.h`・`test-grep-exclude-regex.cpp`（新規）・`tests1.vcxproj`・`tests1.vcxproj.filters`）
- `git grep`: `m_vecExceptFileRegexKeys` は `CGrepEnumKeys.h` に 5 行（宣言・`GetExcludeFiles()`・`SetFileKeys()` のコメントと `push_back_unique`・`ClearItems()`）、`m_fnIsExceptFileName` は `CGrepEnumKeys.h` に 3 行（宣言・`IsExceptFileName()`・`ClearItems()`）、`m_pGrepEnumKeys` は `CGrepEnumFilterFiles.h` に 3 行（宣言・`IsValid()`・`Enumerates()`）。**C1 の時点では `CGrepAgent.cpp` などに現れない**（振る舞い変化なし）
- テスト: すべて PASSED。`*ExceptRegex*` は 16 件。既存の `CGrepEnumKeys.*`・`CGrepEnumFilterFiles.*` の結果は変わらない
- カバレッジ: 変更はヘッダーの関数で、テストが直接呼ぶので約 100% の見込み（【要実測】。SonarCloud の New Code で確認）

**コミット**（例。2 つに分ける）:

1. `除外ファイルの正規表現キーを振り分ける`（`CGrepEnumKeys.h`、新規テスト（`CGrepEnumKeysExceptRegex.*` の 13 件）、`.vcxproj` 2 本）
2. `除外ファイルの照合関数で列挙から除く`（`CGrepEnumFilterFiles.h` と `CGrepEnumFilterFilesExceptRegex.*` の 3 件）

2 つに分ける場合、`CGrepEnumFilterFilesExceptRegex.*` の 3 件は `m_pGrepEnumKeys` が無いと意味を持たないので、コミット 2 に回す（テストファイルを 2 回に分けて追加する）。手間なら 1 コミットでもよい。

---

## 5. 検証したこと / していないこと

**したこと**: `e8833121` の `CGrepEnumKeys.h`・`CGrepEnumFilterFiles.h`・`CGrepEnumFiles.h`・`CGrepEnumFileBase.h`・`tests1.vcxproj`・`tests1.vcxproj.filters`・`pch.h`・`GrepTestSuite.hpp` を読み、C1-1〜C1-11・C1-13・C1-14 の before が各 1 回だけ一致すること、`CGrepEnumFiles::IsValid()` がディレクトリを除くこと、`Enumerates()` が `IsValid()` を `strName`（キーのフォルダー部分を含む）付きで呼ぶこと、`pch.h` に `Eq`・`IsEmpty`・`IsFalse`・`IsTrue` の using があり `ElementsAre` が無いこと、`TempFolder` の `AddFile`・`AddFolder`・`Path` を確認した。テスト 16 件の期待値は、`SetFileKeys()`・`SplitPatternKeepQuotes()`・`ValidateKey()`・`_IS_REL_PATH` の既存の動きから机上で追った（`_IS_REL_PATH` の定義は未確認で、`C:\x\*.txt` が絶対パス扱いになることは T1 の既存テストの前提）。

**していないこと**: ビルド・テストの実行。`ElementsAre` と `L"..."` のリテラルの比較（`std::wstring` との `Eq`）のコンパイル。`GrepTestSuite.hpp` を含めたときのビルド時間の増え方。`3a7c59b9` の `tests1.vcxproj` の読み直し（`.filters` と C1 の対象ヘッダー 2 本の T3 での無変更は確認したが、vcxproj は T3 の差分が 1 行であることまでの確認）。

---

## 追補 C1-15: `SetFileKeys()` の認知的複雑度を下げる（SonarCloud 指摘）

**対象**: `sakura_core/grep/CGrepEnumKeys.h` のみ。**挙動は変えない**（1 トークンの振り分けを private 関数へ移すだけ）。

**指摘**: `SetFileKeys()` の Cognitive Complexity が 30（上限 25）。C1 で追加した正規表現の分岐が、元から分岐の多いループ内に入ったため。
**対策**: ループ内の「種類判定〜振り分け」を `AddFileKey()` に切り出す。`SetFileKeys()` 側はループ・戻り値の判定・既定値の補完だけになる。複雑度は机上で、`SetFileKeys()` が約 5、`AddFileKey()` が約 22（【要実測】Sonar の再解析で確認）。

### C1-15-1: `SetFileKeys()` のループ本体を置き換える

**before**（ファイル内で一意。`for` の中身すべて。`std::vector< std::wstring > patterns = SplitPattern(lpKeys);` の直後）:

```cpp
		for (size_t i = 0; i < patterns.size(); i++) {
			const std::wstring& element = patterns[i];
			const WCHAR* token = element.c_str();

			//フィルタを種類ごとに振り分ける
			enum KeyFilterType{
				FILTER_SEARCH,
				FILTER_EXCEPT_FILE,
				FILTER_EXCEPT_FOLDER,
			};
			KeyFilterType keyType = FILTER_SEARCH;
			if( token[0] == L'!' ){
				token++;
				keyType = FILTER_EXCEPT_FILE;
			}else if( token[0] == L'#' ){
				token++;
				keyType = FILTER_EXCEPT_FOLDER;
			}

			// 除外ファイルを正規表現として扱うときは、パスとしての検査(ValidateKey・絶対パス)をしない
			if( bExceptFileRegex && keyType == FILTER_EXCEPT_FILE ){
				if( token[0] != L'\0' ){
					push_back_unique( m_vecExceptFileRegexKeys, token );
				}
				continue;
			}

			bool bRelPath = _IS_REL_PATH( token );
			int nValidStatus = ValidateKey( token );
			if( 0 != nValidStatus ){

				return nValidStatus;
			}
			if( keyType == FILTER_SEARCH ){
				if( bRelPath ){
					push_back_unique( m_vecSearchFileKeys, token );
				}else{
//					push_back_unique( m_vecSearchAbsFileKeys, token );
//					push_back_unique( m_vecSearchFileKeys, token );
					return 2; // 絶対パス指定は不可
				}
			}else if( keyType == FILTER_EXCEPT_FILE ){
				if( bRelPath ){
					push_back_unique( m_vecExceptFileKeys, token );
				}else{
					push_back_unique( m_vecExceptAbsFileKeys, token );
				}
			}else if( keyType == FILTER_EXCEPT_FOLDER ){
				if( bRelPath ){
					push_back_unique( m_vecExceptFolderKeys, token );
				}else{
					push_back_unique( m_vecExceptAbsFolderKeys, token );
				}
			}
		}
```

**after**:

```cpp
		for (const auto& element : patterns) {
			if( const int nStatus = AddFileKey( element.c_str(), bExceptFileRegex ); 0 != nStatus ){
				return nStatus;
			}
		}
```

### C1-15-2: private 関数 `AddFileKey()` を追加する

`ValidateKey()` の直後（`ParseAndAddException()` の説明コメントの直前）に挿入する。

**before**（一意）:

```cpp
		return 0;
	}

	/*!
		@brief 除外ファイルパターンを追加する
		@param[in]		lpKeys					除外ファイルパターン
		@param[in,out]	exceptionKeys			除外ファイルパターンの解析結果を追加する
```

**after**（先頭の `return 0; }` はそのまま。その後ろに追加）:

```cpp
		return 0;
	}

	/*!
		@brief ファイルパターン 1 つを解析して、種類ごとの配列に振り分ける
		@param[in]	pattern				引用符を取り除いた 1 要素(先頭の ! と # は種類の指定)
		@param[in]	bExceptFileRegex	true なら除外ファイル(!)を正規表現として m_vecExceptFileRegexKeys に入れる
		@retval 0 正常
		@retval 0以外 エラー(ValidateKey() の戻り値、または絶対パスの検索対象で 2)
	*/
	int AddFileKey( const WCHAR* pattern, bool bExceptFileRegex ){
		//フィルタを種類ごとに振り分ける
		enum KeyFilterType{
			FILTER_SEARCH,
			FILTER_EXCEPT_FILE,
			FILTER_EXCEPT_FOLDER,
		};
		const WCHAR* token = pattern;
		KeyFilterType keyType = FILTER_SEARCH;
		if( token[0] == L'!' ){
			token++;
			keyType = FILTER_EXCEPT_FILE;
		}else if( token[0] == L'#' ){
			token++;
			keyType = FILTER_EXCEPT_FOLDER;
		}

		// 除外ファイルを正規表現として扱うときは、パスとしての検査(ValidateKey・絶対パス)をしない
		if( bExceptFileRegex && keyType == FILTER_EXCEPT_FILE ){
			if( token[0] != L'\0' ){
				push_back_unique( m_vecExceptFileRegexKeys, token );
			}
			return 0;
		}

		const bool bRelPath = _IS_REL_PATH( token );
		const int nValidStatus = ValidateKey( token );
		if( 0 != nValidStatus ){
			return nValidStatus;
		}
		if( keyType == FILTER_SEARCH ){
			if( !bRelPath ){
				return 2; // 絶対パス指定は不可
			}
			push_back_unique( m_vecSearchFileKeys, token );
		}else if( keyType == FILTER_EXCEPT_FILE ){
			push_back_unique( bRelPath ? m_vecExceptFileKeys : m_vecExceptAbsFileKeys, token );
		}else{
			push_back_unique( bRelPath ? m_vecExceptFolderKeys : m_vecExceptAbsFolderKeys, token );
		}
		return 0;
	}

	/*!
		@brief 除外ファイルパターンを追加する
		@param[in]		lpKeys					除外ファイルパターン
		@param[in,out]	exceptionKeys			除外ファイルパターンの解析結果を追加する
```

メモ: 元の `//push_back_unique( m_vecSearchAbsFileKeys, ...` のコメントアウト 2 行は、今回の移動で削除した（無効なコード）。残したい場合は `return 2;` の前に戻す。`bRelPath` と `nValidStatus` を `const` にしたのは、移動先で書き換えないため。

### 確認

文字コード: UTF-8 BOM・CRLF・タブ字下げのまま。

```
x64\Debug\tests1.exe --gtest_filter=*ExceptRegex*:CGrepEnumKeys.*:CGrepEnumFilterFiles.*:GrepCommandLineTest.*:GrepOutputSnapshotTest.*:GrepDialogTest.*:CDlgGrepTest.*
```

期待値: 140 件実行・139 PASSED・1 SKIPPED（`GrepCommandLineTest.RejectsReentry`）・1 DISABLED。C1 の時点と**同じ**になること（件数が変わらない＝テスト追加なし）。

コミット例: `refactor: SetFileKeys の 1 要素の振り分けを AddFileKey に切り出す（認知的複雑度の低減）`
