/*! @file */
/*
	Copyright (C) 2018-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "io/CFile.h"

#include <cstdio>

/*!
 * @brief CTmpFile 生成・自動クローズのテスト（#2477）
 *
 * 一時ファイルを開き、読み書きできること、
 * スコープ離脱時にデストラクタで安全に fclose されることを確認する。
 */
TEST(CTmpFile, OpenAndAutoClose)
{
	{
		CTmpFile tmp;
		FILE* fp = tmp.GetFilePointer();
		ASSERT_THAT(fp, NotNull());

		const char data[] = "sakura";
		ASSERT_THAT(std::fwrite(data, 1, sizeof(data), fp), Eq(sizeof(data)));

		std::rewind(fp);

		char readBuf[sizeof(data)] = {};
		ASSERT_THAT(std::fread(readBuf, 1, sizeof(data), fp), Eq(sizeof(data)));
		EXPECT_THAT(readBuf, StrEq(data));
	}
	// スコープ離脱でデストラクタが fclose する（二重 close しない）
}
