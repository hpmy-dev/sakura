# 修正指示書 03c-1: PR-C2 追補 — `CGrepExceptFileRegexps.h` をフルパス照合に直す

03c（`修正指示書_Grep_03c_PR-C2_フルパス照合.md`）の **C2-1 の追補**。スタッシュから戻した `sakura_core/grep/CGrepExceptFileRegexps.h` が C1 マージ前の版（ファイル名照合）のままなので、該当の 4 か所だけを直す。03c の他の項目（C2-2〜C2-8）は変更なし。

| 項目 | 内容 |
|---|---|
| 目的 | 照合関数の設定先を `m_fnIsExceptFilePath` に、照合対象をフルパスに直す |
| 対象 | `sakura_core/grep/CGrepExceptFileRegexps.h`（未追跡。UTF-8 BOM / CRLF / タブインデント） |
| 振る舞い | 製品コードの機能の土台（C2）。C1 のメンバー名に合わせる修正で、ビルドが通らない状態の解消 |
| 禁止事項 | ビルド・git 操作は AI が行わない |

## 0. 現状の確認結果（2026-10-05 実測）

| 確認 | 結果 |
|---|---|
| `sakura.vcxproj` | 03c C2-2 どおり |
| `sakura.vcxproj.filters` | 03c C2-3 どおり（旧表記 `sakura_core\grep` は 0 件） |
| `sakura_rc.h` | 03c C2-4 どおり（35059 / 次の値 35060） |
| `.rc` 3 本 | 03c C2-5〜7 どおり（`STR_GREP_ERR_ENUMKEYS2` の直後） |
| `CGrepExceptFileRegexps.h` | **旧版**（`m_fnIsExceptFileName`・`fileName`）。C1 のメンバーにないのでビルドエラーになる |
| テスト（C2-8） | 未適用 |

## 1. 修正

エディター（Visual Studio またはサクラエディタ）で、文字コード・改行（UTF-8 BOM / CRLF）を変えずに編集する。

### 修正 1: クラスのコメント

**before**

```
	CGrepEnumKeys::m_fnIsExceptFileName に照合関数を設定する。
```

**after**

```
	CGrepEnumKeys::m_fnIsExceptFilePath に照合関数を設定する。
```

### 修正 2: 大文字小文字のコメント

**before**

```
			// ファイル名なので、既存の除外ワイルドカードと同じく大文字小文字を区別しない
```

**after**

```
			// Windows のパスなので、既存の除外ワイルドカードと同じく大文字小文字を区別しない
```

### 修正 3: 照合関数の設定

**before**

```
			cGrepEnumKeys.m_fnIsExceptFileName = [this]( std::wstring_view fileName ){ return IsMatch( fileName ); };
```

**after**

```
			cGrepEnumKeys.m_fnIsExceptFilePath = [this]( std::wstring_view filePath ){ return IsMatch( filePath ); };
```

### 修正 4: `IsMatch()`

**before**

```
	/*!
		@brief ファイル名がいずれかの正規表現に一致するか調べる
		@param[in]	fileName	フォルダーを含まないファイル名
	*/
	bool IsMatch( std::wstring_view fileName )
	{
		return std::ranges::any_of( m_regexps, [fileName]( const auto& pRegexp ){
			return pRegexp->Match( fileName.data(), int( fileName.length() ) );
		} );
	}
```

**after**

```
	/*!
		@brief ファイルのフルパスがいずれかの正規表現に一致するか調べる
		@param[in]	filePath	ファイルのフルパス
	*/
	bool IsMatch( std::wstring_view filePath )
	{
		return std::ranges::any_of( m_regexps, [filePath]( const auto& pRegexp ){
			return pRegexp->Match( filePath.data(), int( filePath.length() ) );
		} );
	}
```

## 2. 確認

未追跡ファイルなので `git grep` には出ない。`findstr` で確かめる。

```cmd
findstr /n /i "fileName" sakura_core\grep\CGrepExceptFileRegexps.h
findstr /n "m_fnIsExceptFilePath filePath" sakura_core\grep\CGrepExceptFileRegexps.h
```

**期待**:

- 1 本目: 何も出ない（`fileName` が残っていない）
- 2 本目: 6 行（クラスのコメント、設定、`@param`、`IsMatch` の宣言、ラムダのキャプチャ、`Match` の呼び出し）【要実測】

文字コードは、先頭に BOM が残っていること、改行が CRLF のままであることを確認する（エディターのステータスバーで見る）。

## 3. この後

03c の C2-8（テスト）を `test-grep-exclude-regex.cpp` に追記し、ビルド → 次のテストで 53 件 PASSED を確認してから `git add` する。

```cmd
win32\Debug\tests1.exe --gtest_filter=CGrepExceptFileRegexpsTest.*:*ExceptRegex*:CGrepEnumKeys.*:CGrepEnumFilterFiles.*
```

ステージ済みの 6 ファイルはそのままでよい。ヘッダーと tests のファイルは、テストが通ってから個別に `git add` する（`git add .` は使わない）。

## 4. 検証したこと / していないこと

**したこと**: 手元の `sakura_core/grep/CGrepExceptFileRegexps.h`・`.rc` 3 本・`sakura.vcxproj.filters` の内容と、`git diff --cached`（`sakura.vcxproj`・`sakura_rc.h`）の出力を、03c と照らした。

**していないこと**: ビルド・テストの実行。
