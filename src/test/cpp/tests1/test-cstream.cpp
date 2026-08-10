/*! @file */
/*
	Copyright (C) 2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "io/CStream.h"

/*!
	CStream コンストラクタのテスト
	pszPath = nullptr で例外モード時に CError_FileOpen がスローされること（フェーズ 0 安全性）
 */
TEST(CStream, Constructor_NullPath_ExceptionMode)
{
	// pszPath = nullptr で例外モードのコンストラクタを呼ぶ
	ASSERT_THROW({
		CStream stream(nullptr, L"rb", true);
	}, CError_FileOpen);
}

/*!
	CStream コンストラクタのテスト
	pszPath = nullptr で非例外モード時にクラッシュせず、Good() == false となること（フェーズ 0 安全性）
 */
TEST(CStream, Constructor_NullPath_NonExceptionMode)
{
	// pszPath = nullptr で非例外モードのコンストラクタを呼ぶ
	// クラッシュしないことを確認
	CStream stream(nullptr, L"rb", false);
	
	// ファイルが開けていないことを確認
	ASSERT_FALSE(stream.Good());
}

/*!
	CStream::Open のテスト
	pszPath = nullptr で非例外モード時にクラッシュせず、Good() == false となること（フェーズ 0 安全性）
 */
TEST(CStream, Open_NullPath_NonExceptionMode)
{
	// 非例外モードの CStream を作成
	CStream stream(L"dummy.txt", L"rb", false); // 一旦ダミーで作成
	stream.Close(); // すぐに閉じる
	
	// nullptr を渡して Open を呼ぶ（クラッシュしないことを確認）
	stream.Open(nullptr, L"rb");
	
	// ファイルが開けていないことを確認
	ASSERT_FALSE(stream.Good());
}

/*!
	CStream の正常系テスト
	有効なパスでファイルが開けること（基本動作確認）
 */
TEST(CStream, Open_ValidPath)
{
	// テスト用の一時ファイルを作成
	const wchar_t* tempPath = L"test_cstream_temp.txt";
	{
		FILE* fp = _wfopen(tempPath, L"wb");
		ASSERT_NE(nullptr, fp) << "Failed to create temp file for test";
		fwrite("test", 1, 4, fp);
		fclose(fp);
	}
	
	// ファイルを開く
	CStream stream(tempPath, L"rb", false);
	
	// 正常に開けたことを確認
	ASSERT_TRUE(stream.Good());
	
	// クリーンアップ
	stream.Close();
	_wremove(tempPath);
}
