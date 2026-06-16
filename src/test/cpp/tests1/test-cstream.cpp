/*! @file */
/*
	Copyright (C) 2018-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "io/CStream.h"

#include <filesystem>

#include "util/file.h"

/*!
 * @brief secure オープンが既存ファイルに対して失敗することのテスト（#2469）
 *
 * 一時ファイルの TOCTOU 対策。secure=true かつ "wb" のとき
 * _O_CREAT|_O_EXCL で排他オープンするため、既存ファイルに対しては
 * オープンに失敗し、例外モードでは CError_FileOpen が送出される。
 */
TEST(CStream, SecureOpenFailsWhenFileExists)
{
	// 既存ファイルを用意する（GetTempFilePath は一時ファイルを実際に生成する）
	auto path = GetTempFilePath(L"tes");
	ASSERT_TRUE(fexist(path));

	// 既存ファイルに対する secure な "wb" オープンは失敗し例外を送出する
	EXPECT_THROW(
		{ COutputStream stream(path.c_str(), L"wb", true, true); },
		CError_FileOpen
	);

	std::filesystem::remove(path);
}

/*!
 * @brief secure オープンが新規ファイルに対して成功することのテスト（#2469）
 */
TEST(CStream, SecureOpenSucceedsForNewFile)
{
	// 生成された一時ファイルを削除し、存在しないパスにする
	auto path = GetTempFilePath(L"tes");
	std::filesystem::remove(path);
	ASSERT_FALSE(fexist(path));

	{
		// 新規ファイルとして排他オープンできる
		COutputStream stream(path.c_str(), L"wb", true, true);
		EXPECT_TRUE(stream.Good());
	}
	EXPECT_TRUE(fexist(path));

	std::filesystem::remove(path);
}

/*!
 * @brief secure 未指定なら既存ファイルを上書きオープンできることのテスト（#2469）
 *
 * 排他オープンは secure 指定時のみ。従来どおりの secure 未指定 "wb" は
 * 既存ファイルでも成功する。
 */
TEST(CStream, NonSecureOpenOverwritesExistingFile)
{
	auto path = GetTempFilePath(L"tes");
	ASSERT_TRUE(fexist(path));

	{
		COutputStream stream(path.c_str(), L"wb", true, false);
		EXPECT_TRUE(stream.Good());
	}

	std::filesystem::remove(path);
}
