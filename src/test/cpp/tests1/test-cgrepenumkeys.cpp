/*! @file */
/*
	Copyright (C) 2018-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "grep/CGrepEnumKeys.h"

/*!
 * @brief ClearItems が絶対パス用途配列をクリアすることのテスト（#2471）
 *
 * SetFileKeys は内部で ClearItems を呼ぶ。修正前は
 * m_vecExceptAbsFileKeys / m_vecExceptAbsFolderKeys をクリアせずに
 * 残っていた。修正後はクリアされることを確認する。
 */
TEST(CGrepEnumKeys, ClearItemsClearsAbsoluteKeys)
{
	CGrepEnumKeys keys;

	// 絶対パスの除外ファイル・フォルダーを追加する
	EXPECT_THAT(keys.AddExceptFile(L"C:\\work\\*.tmp"), Eq(0));
	EXPECT_THAT(keys.AddExceptFolder(L"C:\\work\\sub"), Eq(0));
	EXPECT_THAT(keys.m_vecExceptAbsFileKeys.size(), Eq(size_t(1)));
	EXPECT_THAT(keys.m_vecExceptAbsFolderKeys.size(), Eq(size_t(1)));

	// SetFileKeys は冒頭で ClearItems を呼ぶ
	EXPECT_THAT(keys.SetFileKeys(L"*.txt"), Eq(0));

	// 絶対パス用途配列がクリアされること（修正後の期待値）
	EXPECT_THAT(keys.m_vecExceptAbsFileKeys.size(), Eq(size_t(0)));
	EXPECT_THAT(keys.m_vecExceptAbsFolderKeys.size(), Eq(size_t(0)));
}
