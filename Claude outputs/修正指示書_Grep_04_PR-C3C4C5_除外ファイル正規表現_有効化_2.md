# 修正指示書 04: PR-C3・C4・C5 — 除外ファイル正規表現の有効化

| 項目 | 内容 |
|---|---|
| 目的 | 除外ファイルの正規表現を、コマンドライン（C3）・ダイアログ（C4）・マクロ（C5）から使えるようにする |
| 基準 | upstream master `e3877f79`（2026-10-06。C1 #2710・#2711 を含む）。**C2（照合器 `CGrepExceptFileRegexps`・文字列リソース 35059）のマージ後。** C3 → C4 → C5 の順に適用して before が 1 回だけ一致することを確認（C3 は upstream master で確認。C4・C5 は手元のツリーで確認。C2 のマージ後に再確認する【要実測】） |
| ブランチ（案） | C3 `feature/grep-exclude-regex-enable`、C4（＋C5）`feature/grep-exclude-regex-dialog` |
| 追加テスト | C3 6 件、C4 4 件、C5 はなし（実行側は新しいウィンドウを開くのでマクロの手動確認） |
| ファイル形式 | C++ ソース・テスト・`sakura_rc.h`・`sakura.hh`・ヘルプ（`.html` / `Cshelp.txt`）は UTF-8 BOM / CRLF。**`.rc` は作業ツリーで UTF-16LE BOM / CRLF**。`.vcxproj` は ASCII |
| 禁止事項 | ビルド・git 操作は AI が行わない。PR 本文は別途作成 |

仕様は文書 00 の §3。**照合対象はファイルのフルパス（C1・C2 のとおり）で、大文字小文字を区別しない。** #2459 からの変更点は、C3 の PR 説明に書く（【要確認】#2459 の仕様と照らす。フルパス照合は #2459 と同じ）。

---

## 0. 文書 04 の最新化（2026-10-06）

| # | 内容 |
|---|---|
| 1 | **照合対象がファイル名からフルパスに変わった。** C1（#2710）・C2（文書 03c）のとおり、正規表現にはフルパスが渡る。`^`・`$` はパス全体に当たるので、ファイル名だけに当てるときは `\\[^\\]+$` のように `\\` を使う。**本書の「ファイル名（フォルダーを含まない）」の記述、`^[^.]+$` などの正規表現の例、テストの期待を直した**（C3-13、C3-16、§1-1・§1-4、C4-21〜23、C4-27） |
| 2 | **R1（`GrepInfo::MakeCommandLine()`・`FromMacroFlags()`）は master に無い。** 旧版は「`E` は R1 が出す」としていたが、`-GOPT` の組み立ては 3 か所に直接書かれている（`CControlTray::DoGrepCreateWindow()`・`CViewCommander::Command_GREP_REPLACE()`・`CMacro` の実行側）。**C4 で前の 2 か所（C4-28・C4-29）、C5 で `CMacro` に `E` を足す。** 旧版の C5-3（`GrepInfo.cpp` の `FromMacroFlags()`）と C5-6（そのテスト）は削除した。R1 が後でマージされるときは、R1 側で `E`（`bGrepExceptFileRegexp`）を引き継ぐ |
| 3 | **C5 は C4 と同じ PR にする（案）。** C5 の変更行（記録 1 行・実行側 1 行）は単体テストで通らないので、C5 だけの PR だと New Code カバレッジ 80% 以上（ルール §9）を満たせない。C4 の PR ならダイアログ側のテストで全体の割合を保てる。C4・C5 を別 PR にするなら、実行側を小さな関数に切り出してテストを付ける（要相談） |
| 4 | C3-5〜C3-8 の before（`CGrepAgent.cpp`）は、#2711（存在しないフォルダーの対応）で `DoGrep()` が変わった後の upstream master でも 1 回だけ一致する |
| 5 | `VGrepEnumKeys` は `CGrepEnumKeys.h` で定義済み（C4-25 でそのまま使える）。`m_vecExceptFileRegexKeys` の型は `VGrepEnumKeys` |
| 6 | 文字列リソースの番号は、C2 が `STR_GREP_ERR_EXCLUDE_REGEXP` = 35059 を使うので、C3 は 35060、`_APS_NEXT_RESOURCE_VALUE` は 35061。C3-9 の before は **C2 の適用後**の `sakura_rc.h` が前提 |

---

## 1. PR-C3: 有効化（コマンドライン）

| 変更 | 内容 |
|---|---|
| `GrepInfo` | `bGrepExceptFileRegexp` |
| `CCommandLine` | `-GOPT=E` |
| `CGrepAgent::DoGrep()` | `SetFileKeys()` に指定を渡し、`CGrepExceptFileRegexps::Attach()` で照合関数を設定。失敗はエラー番号 3 として既存のエラー処理でメッセージを出す。条件表示に「(除外ファイルは正規表現)」 |
| リソース | `STR_GREP_EXCLUDE_FILE_REGEXP`（`35060`） |
| ヘルプ | `HLP000109.html` の `-GOPT` の一覧に `E` |

### 修正 C3-1: GrepInfo.h

**before**

```
	bool			bGrepBackup = false;			//!< 置換でバックアップを保存
```

**after**

```
	bool			bGrepBackup = false;			//!< 置換でバックアップを保存
	bool			bGrepExceptFileRegexp = false;	//!< 除外ファイルを正規表現で指定する
```

### 修正 C3-2: CCommandLine.cpp: Copyright

**before**

```
	Copyright (C) 2018-2022, Sakura Editor Organization
```

**after**

```
	Copyright (C) 2018-2026, Sakura Editor Organization
```

### 修正 C3-3: CCommandLine.cpp: -GOPT=E

**before**

```
					case 'O':
						m_gi.bGrepBackup = true;	break;
```

**after**

```
					case 'O':
						m_gi.bGrepBackup = true;	break;
					case 'E':
						// 除外ファイルを正規表現で指定する
						m_gi.bGrepExceptFileRegexp = true;	break;
```

### 修正 C3-4: test-ccommandline.cpp

**before**

```
	cCommandLine.ParseCommandLine(L"-GOPT=O", false);
	EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepBackup);
}
```

**after**

```
	cCommandLine.ParseCommandLine(L"-GOPT=O", false);
	EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepBackup);
}

/*!
* @brief パラメータ解析(-GOPT)の仕様
* @remark -GOPTが指定されていなければFALSE
* @remark -GOPTが指定されていたらTRUE
*/
TEST(CCommandLine, ParseGrepExceptFileRegexp)
{
	CCommandLine cCommandLine;
	cCommandLine.ParseCommandLine(L"", false);
	EXPECT_FALSE(cCommandLine.GetGrepInfoRef().bGrepExceptFileRegexp);
	cCommandLine.ParseCommandLine(L"-GOPT=E", false);
	EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepExceptFileRegexp);
}
```

### 修正 C3-5: CGrepAgent.cpp: include

**before**

```
#include "grep/CGrepEnumFilterFiles.h"

```

**after**

```
#include "grep/CGrepEnumFilterFiles.h"
#include "grep/CGrepExceptFileRegexps.h"

```

### 修正 C3-6: CGrepAgent.cpp: DoGrep() のキー解析

**before**

```
	CGrepEnumKeys cGrepEnumKeys;
	{
		int nErrorNo = cGrepEnumKeys.SetFileKeys( gi.cmGrepFile.GetStringPtr() );

```

**after**

```
	CGrepExceptFileRegexps cExceptFileRegexps;	// 除外ファイル(正規表現)。cGrepEnumKeys の照合関数が参照するので先に宣言する
	CGrepEnumKeys cGrepEnumKeys;
	{
		int nErrorNo = cGrepEnumKeys.SetFileKeys( gi.cmGrepFile.GetStringPtr(), gi.bGrepExceptFileRegexp );
		if( nErrorNo == 0 && !cExceptFileRegexps.Attach( cGrepEnumKeys, GetDllShareData().m_Common.m_sSearch.m_szRegexpLib ) ){
			nErrorNo = 3;
		}

```

- 宣言の順序: `cGrepEnumKeys` の照合関数が `cExceptFileRegexps` を参照するので、`cExceptFileRegexps` を先に宣言し、後に破棄されるようにする
- DLL 名は検索パターンの `InitRegexp()` と同じ `m_szRegexpLib`
- 失敗時の後始末（`m_bGrepRunning` など）は既存のエラー処理をそのまま使う

### 修正 C3-7: CGrepAgent.cpp: DoGrep() のエラーメッセージ

**before**

```
			else if( nErrorNo == 2 ){
				pszErrorMessage = LS(STR_GREP_ERR_ENUMKEYS2);
			}

```

**after**

```
			else if( nErrorNo == 2 ){
				pszErrorMessage = LS(STR_GREP_ERR_ENUMKEYS2);
			}
			else if( nErrorNo == 3 ){
				pszErrorMessage = cExceptFileRegexps.GetErrorMessage().c_str();
			}

```

### 修正 C3-8: CGrepAgent.cpp: DoGrep() の条件表示

**before**

```
		pszWork = LS( STR_GREP_SUBFOLDER_NO );	//L"    (サブフォルダーを検索しない)\r\n"
	}
	cmemMessage.AppendString( pszWork );

```

**after**

```
		pszWork = LS( STR_GREP_SUBFOLDER_NO );	//L"    (サブフォルダーを検索しない)\r\n"
	}
	cmemMessage.AppendString( pszWork );

	if( sGrepOption.bGrepExceptFileRegexp ){
		cmemMessage.AppendString( LS( STR_GREP_EXCLUDE_FILE_REGEXP ) );	//L"    (除外ファイルは正規表現)\r\n"
	}

```

### 修正 C3-9: sakura_rc.h

**before**

```
#define STR_GREP_ERR_EXCLUDE_REGEXP     35059
```

**after**

```
#define STR_GREP_ERR_EXCLUDE_REGEXP     35059
#define STR_GREP_EXCLUDE_FILE_REGEXP    35060
```
**before**

```
#define _APS_NEXT_RESOURCE_VALUE        35060
```

**after**

```
#define _APS_NEXT_RESOURCE_VALUE        35061
```

### 修正 C3-10: sakura_rc.rc

**before**

```
    STR_GREP_SUBFOLDER_YES  "    (サブフォルダーも検索)\r\n"
```

**after**

```
    STR_GREP_SUBFOLDER_YES  "    (サブフォルダーも検索)\r\n"
    STR_GREP_EXCLUDE_FILE_REGEXP "    (除外ファイルは正規表現)\r\n"
```

### 修正 C3-11: sakura_rc_en-US.rc

**before**

```
    STR_GREP_SUBFOLDER_YES  "    (search sub-folders = Yes)\r\n"
```

**after**

```
    STR_GREP_SUBFOLDER_YES  "    (search sub-folders = Yes)\r\n"
    STR_GREP_EXCLUDE_FILE_REGEXP "    (exclude files = regular expression)\r\n"
```

### 修正 C3-12: sakura_rc_zh-CN.rc

**before**

```
    STR_GREP_SUBFOLDER_YES  "    (包含子目录 = 是)\r\n"
```

**after**

```
    STR_GREP_SUBFOLDER_YES  "    (包含子目录 = 是)\r\n"
    STR_GREP_EXCLUDE_FILE_REGEXP "    (排除文件 = 正则表达式)\r\n"
```

### 修正 C3-13: HLP000109.html: -GOPT の一覧

**before**

```
[S][L][R][P][W][1|2|3][K][F][B][G][X][C][O][U][H]
```

**after**

```
[S][L][R][P][W][1|2|3][K][F][B][G][X][C][O][E][U][H]
```
**before**

```
<tr><td>O</td><td>(置換)バックアップ作成 (sakura:2.2.0.0以降)</td></tr>

```

**after**

```
<tr><td>O</td><td>(置換)バックアップ作成 (sakura:2.2.0.0以降)</td></tr>
<tr><td>E</td><td>除外ファイルを正規表現で指定する。ファイルのフルパスと照合し(パスの一部に一致すれば除外)、英大文字と小文字を区別しない。ファイル名だけに当てるときは \\[^\\]+$ のように書く</td></tr>

```

- 既存の一覧の `G`（フォルダー毎に表示）は、実装では `D`。既存の誤りなので本 PR では直さない

### 修正 C3-14: test-ccommandline.cpp: 組み合わせ・順序・小文字

対象: `src/test/cpp/tests1/test-ccommandline.cpp`

**before**

```
	cCommandLine.ParseCommandLine(L"-GOPT=E", false);
	EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepExceptFileRegexp);
}
```

**after**

```
	cCommandLine.ParseCommandLine(L"-GOPT=E", false);
	EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepExceptFileRegexp);
}

/*!
* @brief パラメータ解析(-GOPT)の仕様
* @remark 他の文字と組み合わせても、順序によらず指定できる
* @remark 小文字の e は受け付けない(他の文字と同じく大文字だけ)
*/
TEST(CCommandLine, ParseGrepExceptFileRegexp_Combination)
{
	{
		CCommandLine cCommandLine;
		cCommandLine.ParseCommandLine(L"-GOPT=SE", false);
		EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepSubFolder);
		EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepExceptFileRegexp);
	}
	{
		CCommandLine cCommandLine;
		cCommandLine.ParseCommandLine(L"-GOPT=ES", false);
		EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepSubFolder);
		EXPECT_TRUE(cCommandLine.GetGrepInfoRef().bGrepExceptFileRegexp);
	}
	{
		CCommandLine cCommandLine;
		cCommandLine.ParseCommandLine(L"-GOPT=e", false);
		EXPECT_FALSE(cCommandLine.GetGrepInfoRef().bGrepExceptFileRegexp);
	}
}
```

### 修正 C3-15: test-grepinfo.cpp: 既定値と Normalized()

対象: `src/test/cpp/tests1/test-grepinfo.cpp`

**before**

```
TEST(GrepInfo, Normalized_KeepsOtherMembers)
{
	GrepInfo gi;
	gi.bGrepSubFolder = true;
	gi.bGrepStdout = true;
	gi.bGrepHeader = false;
	gi.nGrepCharSet = CODE_EUC;
	gi.nGrepOutputLineType = 1;
	gi.nGrepOutputStyle = 3;
	gi.bGrepOutputFileOnly = true;
	gi.bGrepOutputBaseFolder = true;
	gi.bGrepSeparateFolder = true;
	gi.bGrepReplace = false;
	gi.bGrepPaste = true;
	gi.bGrepBackup = true;

	const GrepInfo normalized = gi.Normalized();

	EXPECT_TRUE(normalized.bGrepSubFolder);
	EXPECT_TRUE(normalized.bGrepStdout);
	EXPECT_FALSE(normalized.bGrepHeader);
	EXPECT_EQ(CODE_EUC, normalized.nGrepCharSet);
	EXPECT_EQ(1, normalized.nGrepOutputLineType);
	EXPECT_EQ(3, normalized.nGrepOutputStyle);
	EXPECT_TRUE(normalized.bGrepOutputFileOnly);
	EXPECT_TRUE(normalized.bGrepOutputBaseFolder);
	EXPECT_TRUE(normalized.bGrepSeparateFolder);
	EXPECT_FALSE(normalized.bGrepReplace);
	EXPECT_TRUE(normalized.bGrepPaste);
	EXPECT_TRUE(normalized.bGrepBackup);
}
```

**after**

```
TEST(GrepInfo, Normalized_KeepsOtherMembers)
{
	GrepInfo gi;
	gi.bGrepSubFolder = true;
	gi.bGrepStdout = true;
	gi.bGrepHeader = false;
	gi.nGrepCharSet = CODE_EUC;
	gi.nGrepOutputLineType = 1;
	gi.nGrepOutputStyle = 3;
	gi.bGrepOutputFileOnly = true;
	gi.bGrepOutputBaseFolder = true;
	gi.bGrepSeparateFolder = true;
	gi.bGrepReplace = false;
	gi.bGrepPaste = true;
	gi.bGrepBackup = true;

	const GrepInfo normalized = gi.Normalized();

	EXPECT_TRUE(normalized.bGrepSubFolder);
	EXPECT_TRUE(normalized.bGrepStdout);
	EXPECT_FALSE(normalized.bGrepHeader);
	EXPECT_EQ(CODE_EUC, normalized.nGrepCharSet);
	EXPECT_EQ(1, normalized.nGrepOutputLineType);
	EXPECT_EQ(3, normalized.nGrepOutputStyle);
	EXPECT_TRUE(normalized.bGrepOutputFileOnly);
	EXPECT_TRUE(normalized.bGrepOutputBaseFolder);
	EXPECT_TRUE(normalized.bGrepSeparateFolder);
	EXPECT_FALSE(normalized.bGrepReplace);
	EXPECT_TRUE(normalized.bGrepPaste);
	EXPECT_TRUE(normalized.bGrepBackup);
}

/*!
 * @brief GrepInfo の既定値と Normalized() で、除外ファイルの正規表現の指定が保たれること
 */
TEST(GrepInfo, ExceptFileRegexp_DefaultAndNormalized)
{
	GrepInfo gi;
	EXPECT_FALSE(gi.bGrepExceptFileRegexp);

	gi.bGrepExceptFileRegexp = true;
	gi.bGrepReplace = true;
	gi.nGrepOutputLineType = 2;
	const GrepInfo normalized = gi.Normalized();
	EXPECT_TRUE(normalized.bGrepExceptFileRegexp);
}
```

### 修正 C3-16: test-grep-cmdline.cpp: -GOPT=E の実行テスト

対象: `src/test/cpp/tests1/test-grep-cmdline.cpp`

**before**

```
} // namespace grep_test
```

**after**

```
// ---------------------------------------------------------------------------
// 除外ファイルの正規表現(-GOPT=E)
// ---------------------------------------------------------------------------

//! E: フルパスで照合し、サブフォルダーにも効き、大文字小文字を区別しない。結果の条件表示に出る
TEST_F(GrepCommandLineTest, ExceptFileRegexp)
{
	folder.AddFile(L"a.txt", "HIT\r\n");
	folder.AddFile(L"README", "HIT\r\n");
	folder.AddFile(L"app.log", "HIT\r\n");
	folder.AddFile(L"app.20260801.log", "HIT\r\n");
	folder.AddFile(LR"(sub\readme)", "HIT\r\n");
	EXPECT_EQ(2u, Grep(L"HIT", LR"(*;!\\[^.\\]+$;!\\APP\.\d{8}\.log$)", L"XSE"));
	const auto text = GetDocumentText();
	EXPECT_FALSE(LineContaining(text, L"a.txt(").empty());
	EXPECT_FALSE(LineContaining(text, L"app.log(").empty());
	EXPECT_TRUE(LineContaining(text, L"app.20260801.log(").empty());
	EXPECT_TRUE(Contains(text, LS(STR_GREP_EXCLUDE_FILE_REGEXP)));

	// E が無ければワイルドカードとして扱われ、何も除外されない
	ResetDocument();
	EXPECT_EQ(5u, Grep(L"HIT", LR"(*;!\\[^.\\]+$)", L"XS"));
}

//! E: 区切り文字を含む正規表現は引用符で囲む(コマンドラインでは "" と書く)
TEST_F(GrepCommandLineTest, ExceptFileRegexpQuoted)
{
	folder.AddFile(L"a1.txt", "HIT\r\n");
	folder.AddFile(L"a12.txt", "HIT\r\n");
	folder.AddFile(L"a123.txt", "HIT\r\n");
	EXPECT_EQ(1u, Grep(L"HIT", LR"(*;!""\\a\d{2,3}\.txt$"")", L"XE"));
}

//! E: 正しくない正規表現はエラーメッセージを出して何もしない
TEST_F(GrepCommandLineTest, ExceptFileRegexpInvalid)
{
	folder.AddFile(L"a.txt", "HIT\r\n");
	std::wstring shown;
	auto pUser32 = (MockUser32*)User32::getInstance();
	EXPECT_CALL(*pUser32, MessageBoxExW(_, _, _, _, _)).WillOnce(Invoke([&shown](HWND, LPCWSTR text, LPCWSTR, UINT, WORD) {
		shown = text ? text : L"";
		return IDOK;
	}));
	EXPECT_EQ(0u, Grep(L"HIT", L"*;!^[a$", L"XE"));
	EXPECT_TRUE(shown.starts_with(LS(STR_GREP_ERR_EXCLUDE_REGEXP)));
	EXPECT_TRUE(LineContaining(GetDocumentText(), L"a.txt(").empty());
}

} // namespace grep_test
```

- `DoGrep()` の中で追加した行（照合器の宣言・`Attach()`・エラー・条件表示）は、T2 の基盤で同じプロセス内で実行されるので、SonarQube の New Code でも通る
- `a.txt(` のように `(` まで含めて探すのは、条件表示の行（除外ファイル `^[^.]+$` など）に一致させないため


### 1-1. 注意

- **照合器を `cGrepEnumKeys` より先に宣言する**（照合関数が照合器を参照するので、後に破棄されるようにする）
- 正規表現の DLL 名は検索パターンと同じ `m_szRegexpLib`
- ヘルプに「ファイルのフルパスと照合し、一部に一致すれば除外する。ファイル名だけに当てるときは `\\[^\\]+$` のように `\\` を使う」を書き足す（`HLP000109.html` の `E` の説明。C3-13）。「空白・`;`・`,` を含む場合は `"` で囲む」は C4 のヘルプ（C4-22・C4-23）に書く
- `E` なしのときの `!\\[^.\\]+$` は、先頭が `\\` なので絶対パスの除外（完全一致）として扱われ、何も除外しない（`GrepCommandLineTest.ExceptFileRegexp` の 5 件）【要実測】
- `Attach()` が失敗したときの表示は、既存の `STR_GREP_ERR_ENUMKEYS*` と同じ `ErrorMessage()`（標準出力 `U` のときもメッセージボックスが出る）。#2711 の `AllFoldersExist()` のように標準出力へ書く案もあるが、C3 の範囲外とする【要判断】
- 既存のヘルプの一覧の `G`（フォルダー毎に表示）は、実装では `D`。既存の誤りなので本 PR では直さない

### 1-2. テストの一覧

| テスト | 確かめること |
|---|---|
| `CCommandLine.ParseGrepExceptFileRegexp` | `-GOPT=E` |
| `CCommandLine.ParseGrepExceptFileRegexp_Combination` | `SE` / `ES`、小文字 `e` は受け付けない |
| `GrepInfo.ExceptFileRegexp_DefaultAndNormalized` | 既定値、Grep 置換の補正で消えない |
| `GrepCommandLineTest.ExceptFileRegexp` | フルパスで照合（ファイル名だけに当てる書き方 `\\[^.\\]+$`）、サブフォルダーにも効く、大文字小文字を区別しない、条件表示、`E` 無しでは何も除外しない |
| `GrepCommandLineTest.ExceptFileRegexpQuoted` | 区切り文字を含む正規表現を `""` で囲む |
| `GrepCommandLineTest.ExceptFileRegexpInvalid` | 誤った正規表現でメッセージを出し、何もしない |

### 1-3. 自己チェック・コミット

```cmd
git grep -n "bGrepExceptFileRegexp" -- sakura_core/
win32\Debug\tests1.exe --gtest_filter=CCommandLine.ParseGrep*:GrepInfo.*:GrepCommandLineTest.ExceptFileRegexp*
```

**期待**: 1 本目は `GrepInfo.h`、`CCommandLine.cpp`、`CGrepAgent.cpp`（2 箇所）。6 件 PASSED。`DoGrep()` の変更行は T2 の基盤で同じプロセス内で通るので、New Code カバレッジは高い見込み（実測する）。

**コミット**（例）: `-GOPT=Eで除外ファイルを正規表現で指定できるようにする`

### 1-4. 手動確認（任意）

```cmd
sakura.exe -GREPMODE -GKEY=HIT -GFILE="*;!\\[^.\\]+$;!\\app\.\d{8}\.log$" -GFOLDER=C:\temp\grep_c3 -GOPT=SE
```

`README`・`sub\readme`・`app.20260801.log` が除外され、`a.txt`・`app.log` がヒットする。

---

## 2. PR-C4: ダイアログ

| 変更 | 内容 |
|---|---|
| 設定 | `CommonSetting_Search::m_bGrepExceptFileRegexp`、既定値、プロファイルの読み書き、**`N_SHAREDATA_VERSION` を 183 に** |
| ダイアログ | チェックボックス「正規表現(&Q)」（`IDC_CHK_EXCLUDE_FILE_REGEXP` = 1742）を除外ファイル欄の右に。3 言語の `.rc` × 2 ダイアログ、ヘルプ ID 12028 |
| `CDlgGrep` | メンバー・コンストラクター・`DoModal()` の読み込み・`SetData()`・`GetData()`・保存・`MakeGrepInfo()` |
| 新しいウィンドウで開く経路 | `CControlTray::DoGrepCreateWindow()`（Grep）・`CViewCommander::Command_GREP_REPLACE()`（Grep 置換）が作る `-GOPT` に `E` を足す（C4-28・C4-29） |
| ヘルプ | `Cshelp.txt`、`HLP000067.html`・`HLP000362.html` |

### 修正 C4-1: CommonSetting.h: Copyright

**before**

```
	Copyright (C) 2018-2022, Sakura Editor Organization
```

**after**

```
	Copyright (C) 2018-2026, Sakura Editor Organization
```

### 修正 C4-2: CommonSetting.h

**before**

```
	bool			m_bGrepSeparateFolder;		//!< Grep: フォルダー毎に表示

```

**after**

```
	bool			m_bGrepSeparateFolder;		//!< Grep: フォルダー毎に表示
	bool			m_bGrepExceptFileRegexp;	//!< Grep: 除外ファイルを正規表現で指定

```

### 修正 C4-3: system_constants.h

**before**

```
	Version 181:
	m_hAccel削除

```

**after**

```
	Version 181:
	m_hAccel削除

	Version 183:
	CommonSetting_Search::m_bGrepExceptFileRegexp 追加

```
**before**

```
#define N_SHAREDATA_VERSION		182
```

**after**

```
#define N_SHAREDATA_VERSION		183
```

- 182 の履歴は書かれていない（既存）。手元のツリーで `N_SHAREDATA_VERSION` が 182 のままなのを確認した（2026-10-06）。着手時に upstream で 183 以降が使われていないか確認し、使われていれば次の番号にする【要実測】

### 修正 C4-4: CShareData.cpp: Copyright

**before**

```
	Copyright (C) 2018-2022, Sakura Editor Organization
```

**after**

```
	Copyright (C) 2018-2026, Sakura Editor Organization
```

### 修正 C4-5: CShareData.cpp: 既定値

**before**

```
			sSearch.m_bGrepSeparateFolder = false;

```

**after**

```
			sSearch.m_bGrepSeparateFolder = false;
			sSearch.m_bGrepExceptFileRegexp = false;

```

### 修正 C4-6: CShareData_IO.cpp: Copyright

**before**

```
	Copyright (C) 2018-2022, Sakura Editor Organization
```

**after**

```
	Copyright (C) 2018-2026, Sakura Editor Organization
```

### 修正 C4-7: CShareData_IO.cpp: 設定の読み書き

**before**

```
	cProfile.IOProfileData( pszSecName, L"bGrepSeparateFolder"	, common.m_sSearch.m_bGrepSeparateFolder );
```

**after**

```
	cProfile.IOProfileData( pszSecName, L"bGrepSeparateFolder"	, common.m_sSearch.m_bGrepSeparateFolder );
	cProfile.IOProfileData( pszSecName, L"bGrepExceptFileRegexp"	, common.m_sSearch.m_bGrepExceptFileRegexp );
```

### 修正 C4-8: CDlgGrep.h

**before**

```
	bool		m_bGrepSeparateFolder;		/*!< フォルダー毎に表示 */

```

**after**

```
	bool		m_bGrepSeparateFolder;		/*!< フォルダー毎に表示 */
	bool		m_bGrepExceptFileRegexp;	/*!< 除外ファイルを正規表現で指定 */

```

### 修正 C4-9: CDlgGrep.cpp: ヘルプID

**before**

```
	IDC_COMBO_EXCLUDE_FOLDER,		HIDC_GREP_COMBO_EXCLUDE_FOLDER,		//除外フォルダー

```

**after**

```
	IDC_COMBO_EXCLUDE_FOLDER,		HIDC_GREP_COMBO_EXCLUDE_FOLDER,		//除外フォルダー
	IDC_CHK_EXCLUDE_FILE_REGEXP,	HIDC_GREP_CHK_EXCLUDE_FILE_REGEXP,	//除外ファイルの正規表現

```

### 修正 C4-10: CDlgGrep.cpp: コンストラクター

**before**

```
	m_bGrepSeparateFolder = false;


```

**after**

```
	m_bGrepSeparateFolder = false;
	m_bGrepExceptFileRegexp = false;


```

### 修正 C4-11: CDlgGrep.cpp: MakeGrepInfo()

**before**

```
	gi.bGrepSeparateFolder = m_bGrepSeparateFolder;

```

**after**

```
	gi.bGrepSeparateFolder = m_bGrepSeparateFolder;
	gi.bGrepExceptFileRegexp = m_bGrepExceptFileRegexp;

```

### 修正 C4-12: CDlgGrep.cpp: DoModal() の設定読み込み

**before**

```
	m_bGrepSeparateFolder = m_pShareData->m_Common.m_sSearch.m_bGrepSeparateFolder;

```

**after**

```
	m_bGrepSeparateFolder = m_pShareData->m_Common.m_sSearch.m_bGrepSeparateFolder;
	m_bGrepExceptFileRegexp = m_pShareData->m_Common.m_sSearch.m_bGrepExceptFileRegexp;

```

### 修正 C4-13: CDlgGrep.cpp: SetData()

**before**

```
	CheckDlgButtonBool( GetHwnd(), IDC_CHECK_SEP_FOLDER, m_bGrepSeparateFolder );

```

**after**

```
	CheckDlgButtonBool( GetHwnd(), IDC_CHECK_SEP_FOLDER, m_bGrepSeparateFolder );
	CheckDlgButtonBool( GetHwnd(), IDC_CHK_EXCLUDE_FILE_REGEXP, m_bGrepExceptFileRegexp );

```

### 修正 C4-14: CDlgGrep.cpp: GetData() の取得

**before**

```
	m_bGrepSeparateFolder = IsDlgButtonCheckedBool( GetHwnd(), IDC_CHECK_SEP_FOLDER );

```

**after**

```
	m_bGrepSeparateFolder = IsDlgButtonCheckedBool( GetHwnd(), IDC_CHECK_SEP_FOLDER );
	m_bGrepExceptFileRegexp = IsDlgButtonCheckedBool( GetHwnd(), IDC_CHK_EXCLUDE_FILE_REGEXP );

```

### 修正 C4-15: CDlgGrep.cpp: GetData() の保存

**before**

```
	m_pShareData->m_Common.m_sSearch.m_bGrepSeparateFolder = m_bGrepSeparateFolder;

```

**after**

```
	m_pShareData->m_Common.m_sSearch.m_bGrepSeparateFolder = m_bGrepSeparateFolder;
	m_pShareData->m_Common.m_sSearch.m_bGrepExceptFileRegexp = m_bGrepExceptFileRegexp;

```

### 修正 C4-16: sakura_rc.h

**before**

```
#define IDC_CHECK_bDarkMode             1741

```

**after**

```
#define IDC_CHECK_bDarkMode             1741
#define IDC_CHK_EXCLUDE_FILE_REGEXP     1742

```
**before**

```
#define _APS_NEXT_CONTROL_VALUE         1741
```

**after**

```
#define _APS_NEXT_CONTROL_VALUE         1743
```

- `_APS_NEXT_CONTROL_VALUE` は既存の値（1741）が `IDC_CHECK_bDarkMode` と重なっているので、1742 を使い 1743 にする

### 修正 C4-17: sakura.hh

**before**

```
#define HIDC_GREP_COMBO_EXCLUDE_FOLDER	12027	//除外フォルダー

```

**after**

```
#define HIDC_GREP_COMBO_EXCLUDE_FOLDER	12027	//除外フォルダー
#define HIDC_GREP_CHK_EXCLUDE_FILE_REGEXP	12028	//除外ファイルの正規表現

```

### 修正 C4-18: sakura_rc.rc: ダイアログ

**IDD_GREP**

**before**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,60,110,272,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
```

**after**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,60,110,272,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    CONTROL         "正規表現(&Q)",IDC_CHK_EXCLUDE_FILE_REGEXP,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,336,112,50,8
```
**IDD_GREP_REPLACE**

**before**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,60,124,272,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
```

**after**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,60,124,272,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    CONTROL         "正規表現(&Q)",IDC_CHK_EXCLUDE_FILE_REGEXP,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,336,126,50,8
```

### 修正 C4-19: sakura_rc_en-US.rc: ダイアログ

**IDD_GREP**

**before**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,110,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
```

**after**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,110,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    CONTROL         "Regex (&Q)",IDC_CHK_EXCLUDE_FILE_REGEXP,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,376,112,50,8
```
**IDD_GREP_REPLACE**

**before**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,124,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
```

**after**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,124,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    CONTROL         "Regex (&Q)",IDC_CHK_EXCLUDE_FILE_REGEXP,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,376,126,50,8
```

### 修正 C4-20: sakura_rc_zh-CN.rc: ダイアログ

**IDD_GREP**

**before**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,110,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
```

**after**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,110,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    CONTROL         "正则(&Q)",IDC_CHK_EXCLUDE_FILE_REGEXP,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,376,112,50,8
```
**IDD_GREP_REPLACE**

**before**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,124,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
```

**after**

```
    COMBOBOX        IDC_COMBO_EXCLUDE_FILE,72,124,300,120,CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP
    CONTROL         "正则(&Q)",IDC_CHK_EXCLUDE_FILE_REGEXP,"Button",BS_AUTOCHECKBOX | WS_TABSTOP,376,126,50,8
```

### 修正 C4-21: Cshelp.txt

**before**

```

.topic 12100

```

**after**

```

.topic 12028
【正規表現(除外ファイル)】

除外ファイルを正規表現で指定します。ファイルのフルパスと照合し、英大文字と小文字を区別しません。

.topic 12100

```

### 修正 C4-22: HLP000067.html: 除外ファイルの説明

**before**

```
	ファイルパターンを;で区切って指定することができます。;を含むファイルパターンを指定する場合は""で囲ってください。(sakura:2.4.0.0以降)<br>

```

**after**

```
	ファイルパターンを;で区切って指定することができます。;を含むファイルパターンを指定する場合は""で囲ってください。(sakura:2.4.0.0以降)<br>
	<strong>(正規表現)</strong> … 除外ファイルを正規表現で指定します。ファイルのフルパスと照合し、英大文字と小文字を区別しません。<br>
	パスの一部に一致すれば除外します。ファイル名だけに当てるときは \\[^\\]+$ のように書きます。<br>
	正規表現に空白・;・, を含む場合は""で囲ってください。" は指定できません。<br>

```

### 修正 C4-23: HLP000362.html: 除外ファイルの説明

**before**

```
	ファイルパターンを;で区切って指定することができます。;を含むファイルパターンを指定する場合は""で囲ってください。(sakura:2.4.0.0以降)<br>

```

**after**

```
	ファイルパターンを;で区切って指定することができます。;を含むファイルパターンを指定する場合は""で囲ってください。(sakura:2.4.0.0以降)<br>
	<strong>(正規表現)</strong> … 除外ファイルを正規表現で指定します。ファイルのフルパスと照合し、英大文字と小文字を区別しません。<br>
	パスの一部に一致すれば除外します。ファイル名だけに当てるときは \\[^\\]+$ のように書きます。<br>
	正規表現に空白・;・, を含む場合は""で囲ってください。" は指定できません。<br>

```

### 修正 C4-24: test-grepinfo.cpp

**before**

```
	EXPECT_TRUE(gi.bGrepPaste);
	EXPECT_TRUE(gi.bGrepBackup);
}
```

**after**

```
	EXPECT_TRUE(gi.bGrepPaste);
	EXPECT_TRUE(gi.bGrepBackup);
}

/*!
 * @brief CDlgGrep::MakeGrepInfo() のテスト
 *  除外ファイルの正規表現の指定が GrepInfo に写されること
 */
TEST_F(CDlgGrepTest, MakeGrepInfo_CopiesExceptFileRegexp)
{
	CDlgGrep dlg;
	EXPECT_FALSE(dlg.MakeGrepInfo().bGrepExceptFileRegexp);

	dlg.m_bGrepExceptFileRegexp = true;
	EXPECT_TRUE(dlg.MakeGrepInfo().bGrepExceptFileRegexp);
}
```

### 修正 C4-25: test-grepinfo.cpp: 往復と既定値

対象: `src/test/cpp/tests1/test-grepinfo.cpp`

**before**

```
	dlg.m_bGrepExceptFileRegexp = true;
	EXPECT_TRUE(dlg.MakeGrepInfo().bGrepExceptFileRegexp);
}
```

**after**

```
	dlg.m_bGrepExceptFileRegexp = true;
	EXPECT_TRUE(dlg.MakeGrepInfo().bGrepExceptFileRegexp);
}

/*!
 * @brief CDlgGrep::MakeGrepInfo() のテスト
 *  正規表現の除外ファイル(区切り文字を含むものは引用符付き)が、Grep 実行時の解析で正規表現として振り分けられること
 */
TEST_F(CDlgGrepTest, MakeGrepInfo_ExceptFileRegexpRoundTrip)
{
	CDlgGrep dlg;
	dlg.m_szFile = L"*.cpp";
	dlg.m_szExcludeFile = LR"("\d{2,4}" ^a$)";
	dlg.m_bGrepExceptFileRegexp = true;

	const GrepInfo gi = dlg.MakeGrepInfo();
	const std::wstring expectedFile = LR"(*.cpp;!"\d{2,4}";!^a$)";	// 引用符を含むのでマクロの外で作る(C2017 の回避)
	EXPECT_EQ(expectedFile, gi.cmGrepFile.GetStringPtr());

	CGrepEnumKeys keys;
	ASSERT_THAT(keys.SetFileKeys(gi.cmGrepFile.GetStringPtr(), gi.bGrepExceptFileRegexp), 0);
	EXPECT_THAT(keys.m_vecExceptFileRegexKeys, (VGrepEnumKeys{ LR"(\d{2,4})", L"^a$" }));
	EXPECT_TRUE(keys.m_vecExceptFileKeys.empty());
}

/*!
 * @brief 共有データの既定値では、除外ファイルの正規表現はオフであること
 */
TEST_F(CDlgGrepTest, ShareDataDefault_ExceptFileRegexpIsOff)
{
	EXPECT_FALSE(GetDllShareData().m_Common.m_sSearch.m_bGrepExceptFileRegexp);
}
```

### 修正 C4-26: test-grep-dialog.cpp: 共通の入力に正規表現のチェックを加える

対象: `src/test/cpp/tests1/test-grep-dialog.cpp`

**before**

```
		SetText(hDlg, IDC_COMBO_EXCLUDE_FOLDER, L"");

```

**after**

```
		SetText(hDlg, IDC_COMBO_EXCLUDE_FOLDER, L"");
		Check(hDlg, IDC_CHK_EXCLUDE_FILE_REGEXP, false);

```

- 前のテストや設定でチェックが残っていても、既存のテストの結果が変わらないようにする

### 修正 C4-27: test-grep-dialog.cpp: チェックボックスの実行テスト

対象: `src/test/cpp/tests1/test-grep-dialog.cpp`

**before**

```
} // namespace grep_test
```

**after**

```
// ---------------------------------------------------------------------------
// 除外ファイルの正規表現(チェックボックス)
// ---------------------------------------------------------------------------

//! チェックボックスをオンにすると除外ファイルが正規表現になり、設定に保存される
TEST_F(GrepDialogTest, ExceptFileRegexpCheckbox)
{
	folder.AddFile(L"a.txt", "HIT\r\n");
	folder.AddFile(L"README", "HIT\r\n");
	auto& search = GetDllShareData().m_Common.m_sSearch;
	AcceptGrepDialog([this](HWND hDlg) {
		SetConditions(hDlg, L"HIT", L"*");
		SetText(hDlg, IDC_COMBO_EXCLUDE_FILE, LR"(\\[^.\\]+$)");
		Check(hDlg, IDC_CHK_EXCLUDE_FILE_REGEXP, true);
	});
	const auto text = GetDocumentText();
	const bool saved = search.m_bGrepExceptFileRegexp;
	search.m_bGrepExceptFileRegexp = false;	// 後続のテストのために戻す

	EXPECT_TRUE(Contains(text, MatchCountText(1)));
	EXPECT_TRUE(Contains(text, LS(STR_GREP_EXCLUDE_FILE_REGEXP)));
	EXPECT_TRUE(saved);
}

} // namespace grep_test
```


### 修正 C4-28: CControlTray.cpp: DoGrepCreateWindow() の -GOPT

対象: `sakura_core/_main/CControlTray.cpp`

Grep の実行中や Grep 結果のウィンドウから Grep するとき、ダイアログの内容は新しいウィンドウにコマンドラインで渡される。そこに `E` を足さないと、チェックを入れても新しいウィンドウでは効かない。

**before**

```
	if( cDlgGrep.m_bGrepSeparateFolder		)wcscat( pOpt, L"D" );
	if( pOpt[0] != L'\0' ){
```

**after**

```
	if( cDlgGrep.m_bGrepSeparateFolder		)wcscat( pOpt, L"D" );
	if( cDlgGrep.m_bGrepExceptFileRegexp		)wcscat( pOpt, L"E" );	// 除外ファイルを正規表現で指定する
	if( pOpt[0] != L'\0' ){
```

### 修正 C4-29: CViewCommander_Grep.cpp: Command_GREP_REPLACE() の -GOPT

対象: `sakura_core/cmd/CViewCommander_Grep.cpp`

**before**

```
		if( cDlgGrepRep.m_bGrepSeparateFolder		)wcscat( pOpt, L"D" );
		if( cDlgGrepRep.m_bPaste					)wcscat( pOpt, L"C" );	// クリップボードから貼り付け
```

**after**

```
		if( cDlgGrepRep.m_bGrepSeparateFolder		)wcscat( pOpt, L"D" );
		if( cDlgGrepRep.m_bGrepExceptFileRegexp		)wcscat( pOpt, L"E" );	// 除外ファイルを正規表現で指定する
		if( cDlgGrepRep.m_bPaste					)wcscat( pOpt, L"C" );	// クリップボードから貼り付け
```

- `-GOPT` の文字数: 最大でも `S L R W P 1 F B D E C O` の 12 文字で、`pOpt[64]` に収まる
- この 2 か所は新しいウィンドウを開くので、単体テストでは通らない（手動確認: §2-3）。ダイアログ側（C4-8〜C4-15）のテストで、同じ PR の New Code カバレッジを保つ

### 2-1. 注意

- **`.rc` は作業ツリーで UTF-16LE BOM。** MCP で編集しない
- `_APS_NEXT_CONTROL_VALUE` は既存の値（1741）が `IDC_CHECK_bDarkMode` と重なっているので、1742 を使って 1743 にする
- `N_SHAREDATA_VERSION` を上げるので、旧版を起動したまま新版を起動しない。着手時に upstream で 183 が使われていないか確認する
- ダイアログで正規表現の誤りを入力した場合、エラーは Grep の実行時に出る（ダイアログの OK 時の検査は範囲外）
- `-GOPT` の `E` は、新しいウィンドウに開く経路（C4-28・C4-29）で足す。R1 は master に無いので、`GrepInfo` に組み立て関数は無い

### 2-2. テストの一覧

| テスト | 確かめること |
|---|---|
| `CDlgGrepTest.MakeGrepInfo_CopiesExceptFileRegexp` | ダイアログの指定が `GrepInfo` に写る |
| `CDlgGrepTest.MakeGrepInfo_ExceptFileRegexpRoundTrip` | 引用符付きの正規表現がダイアログ → `-GFILE` → 解析で正規表現になる |
| `CDlgGrepTest.ShareDataDefault_ExceptFileRegexpIsOff` | 既定値（**要実測**: フィクスチャが共有データを既定値で初期化している前提） |
| `GrepDialogTest.ExceptFileRegexpCheckbox` | チェックボックスで実行され、条件表示に出て、設定に保存される |

### 2-3. 自己チェック・コミット

```cmd
git grep -n "m_bGrepExceptFileRegexp" -- sakura_core/
git grep -n -e "IDC_CHK_EXCLUDE_FILE_REGEXP" -e "HIDC_GREP_CHK_EXCLUDE_FILE_REGEXP" -- sakura_core/ sakura_lang/ src/main/
win32\Debug\tests1.exe --gtest_filter=CDlgGrepTest.*:GrepDialogTest.*
```

**期待**: すべて PASSED。`SetData()`・`GetData()` の変更行は `ExceptFileRegexpCheckbox` で通る。新しいウィンドウに開く経路の `E` は C4-28・C4-29 で足す（この 2 行は単体テストで通らないので、手動確認する）。

**手動確認**: チェックボックスの位置と `Alt+Q`、再起動後の保存、Grep 置換・タスクトレイからの Grep、`F1` のヘルプ、英語・中国語の表示。**Grep 結果のウィンドウからもう一度 Grep する（新しいウィンドウに開く経路。Grep と Grep 置換の両方）とき、チェックを入れたまま除外が正規表現として効く。**

**コミット**（例）: `Grepダイアログに除外ファイルの正規表現を追加`

---

## 3. PR-C5: マクロ

| 変更 | 内容 |
|---|---|
| 記録 | `CMacro` の記録に、第 4 引数のフラグ `0x800000`（除外ファイルは正規表現）を足す |
| 実行 | `CMacro::HandleCommand()` が組み立てる `-GOPT` に、`0x800000` のとき `E` を足す |
| ヘルプ | `HLP000067.html`・`HLP000362.html` |
| PR | **C4 と同じ PR にする（案。§0 の #3）** |

### 修正 C5-1: CMacro.cpp: Copyright

**before**

```
	Copyright (C) 2018-2022, Sakura Editor Organization
```

**after**

```
	Copyright (C) 2018-2026, Sakura Editor Organization
```

### 修正 C5-2: CMacro.cpp: 記録

**before**

```
			lFlag |= GetDllShareData().m_Common.m_sSearch.m_bGrepSeparateFolder			? 0x80000 : 0x00;

```

**after**

```
			lFlag |= GetDllShareData().m_Common.m_sSearch.m_bGrepSeparateFolder			? 0x80000 : 0x00;
			lFlag |= GetDllShareData().m_Common.m_sSearch.m_bGrepExceptFileRegexp		? 0x800000 : 0x00;

```

### 修正 C5-3: CMacro.cpp: 実行（-GOPT=E）

対象: `sakura_core/macro/CMacro.cpp`

**before**

```
			if( lFlag & 0x80000 )wcscat( pOpt, L"D" );
			if( bGrepReplace ){
```

**after**

```
			if( lFlag & 0x80000 )wcscat( pOpt, L"D" );
			if( lFlag & 0x800000 )wcscat( pOpt, L"E" );	/* 除外ファイルを正規表現で指定する */
			if( bGrepReplace ){
```

- `szTemp[20]` に ` -GOPT=` と `pOpt` を書く。最大の組み合わせ（`S L R P 1 W F B D E C O` = 12 文字）で 7 + 12 + 終端 = 20 文字ちょうど。以後 `-GOPT` に文字を足すときは `szTemp` を広げること

### 修正 C5-4: HLP000067.html: マクロのフラグ

**before**

```
&nbsp;&nbsp;&nbsp;&nbsp;0x080000;&nbsp;&nbsp;フォルダー毎に表示(sakura:2.1.0.0以降)<br>

```

**after**

```
&nbsp;&nbsp;&nbsp;&nbsp;0x080000;&nbsp;&nbsp;フォルダー毎に表示(sakura:2.1.0.0以降)<br>
&nbsp;&nbsp;&nbsp;&nbsp;0x800000;&nbsp;&nbsp;除外ファイルを正規表現で指定<br>

```

### 修正 C5-5: HLP000362.html: マクロのフラグ

**before**

```
&nbsp;&nbsp;&nbsp;&nbsp;0x200000;&nbsp;&nbsp;バックアップ作成<br>

```

**after**

```
&nbsp;&nbsp;&nbsp;&nbsp;0x200000;&nbsp;&nbsp;バックアップ作成<br>
&nbsp;&nbsp;&nbsp;&nbsp;0x800000;&nbsp;&nbsp;除外ファイルを正規表現で指定<br>

```

### 修正 C5-6: CMacro.cpp: フラグの説明コメント

対象: `sakura_core/macro/CMacro.cpp`

**before**

```
		//		0x080000	フォルダー毎に表示
		{
```

**after**

```
		//		0x080000	フォルダー毎に表示
		//		0x800000	除外ファイルを正規表現で指定する
		{
```

- `0x800000` は未使用（使用中: `0x01`〜`0x80`、`0xFF00`（文字コード）、`0x10000`〜`0x400000`）
- 記録は共有データ（ダイアログの `GetData()` で保存済み）から取る
- 実行側（C5-3）は `-GOPT=E` を作るだけで、除外ファイルの解析・照合は C3 の `DoGrep()` が行う
- **単体テストは付けない。** `HandleCommand()` は新しいウィンドウを開くので、手動で確認する。C4 と別 PR にする場合は、New Code カバレッジを満たすために、フラグから `-GOPT` を作る部分を関数に切り出してテストを付ける（要相談）

**手動確認**: キーマクロの記録で Grep の第 4 引数に `0x800000` が入り、再生で同じ結果になる。手で書いた `Grep` マクロ（第 4 引数に `0x800000` を加える）で、新しいウィンドウの条件表示に「(除外ファイルは正規表現)」が出る。

**コミット**（例）: `Grepマクロで除外ファイルの正規表現を指定できるようにする`

---

## 4. 検証したこと / していないこと

**したこと（2026-10-06）**:

- upstream master `e3877f79` の `CGrepAgent.cpp` で、C3-5〜C3-8 の before が各 1 回だけ一致する（#2711 の後）。`CGrepEnumKeys.h`（C1 のマージ版）の `VGrepEnumKeys`・`m_vecExceptFileRegexKeys`・`SetFileKeys( lpKeys, bExceptFileRegex )`・`m_fnIsExceptFilePath`
- 手元のツリー（別ブランチ `test/grep-cmdline-review-fixes`）で、C3・C4・C5 の before を機械的に突き合わせた。`GrepInfo.h`・`CCommandLine.cpp`・`CGrepAgent.cpp`・`CommonSetting.h`・`system_constants.h`（182）・`CShareData*.cpp`・`CDlgGrep.*`・`sakura.hh`・`CMacro.cpp`・`CControlTray.cpp`・`CViewCommander_Grep.cpp`・`.rc` 3 本・ヘルプ 3 本・`Cshelp.txt`・テスト 4 本が 1 回だけ一致する（C3-14・C4-25 は直前の修正の after に対する before なので、単独では一致しない。C3-9 の `sakura_rc.h` は C2 の適用後が前提で、手元のツリーには C2 が無いので未確認）
- `-GOPT` の組み立て箇所が `CControlTray`・`CViewCommander_Grep`・`CMacro` の 3 か所だけであること、`E` が未使用であること、`0x800000` が未使用であること

**していないこと**:

- ビルド・テストの実行
- **C2 のマージ後の master での再確認**（`sakura_rc.h` の C3-9・C4-16、`.vcxproj`・`.filters` に C2 の差が入る。着手時に `git grep` で確かめる）
- 手元のツリーが upstream master と同じかどうか（別ブランチ）。特に C4 の `system_constants.h`（182）・`CShareData_IO.cpp`・`CDlgGrep.cpp` は、着手時に upstream で確かめる
- bregonig での `\\[^.\\]+$` や `\\APP\.\d{8}\.log$` の動き（`[^.\\]` の中の `\\` を含む）、`E` なしのときの `!\\[^.\\]+$` が何も除外しないこと（絶対パスの除外として扱われる想定）
- チェックボックスの見た目と各言語の表現、`CGrepExceptFileRegexps.h` で `LS()` / `STR_*` が使えること（C2 のビルドで確認）
