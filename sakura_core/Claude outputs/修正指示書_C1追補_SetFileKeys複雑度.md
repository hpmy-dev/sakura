# 修正指示書 C1 追補: SetFileKeys() の認知的複雑度の低減

既に適用済みの C1 の修正は含まない。この文書の変更だけを行う。

## 概要（SonarCloud 指摘）

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
