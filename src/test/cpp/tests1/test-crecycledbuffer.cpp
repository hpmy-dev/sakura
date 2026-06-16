/*! @file */
/*
	Copyright (C) 2018-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "mem/CRecycledBuffer.h"

#include <cstring>

/*!
 * @brief CRecycledBufferDynamic 動的再確保のテスト（#2472）
 *
 * CHAIN_COUNT(64) を超えて呼び出すと同一スロットの
 * 解放→再確保の経路を通る。修正後は解放後に nullptr 代入するため
 * 二重解放が起きないことを確認する。
 */
TEST(CRecycledBuffer, DynamicReuse)
{
	CRecycledBufferDynamic buf;

	for (int i = 0; i < 200; ++i) {
		auto* p = buf.GetBuffer<BYTE>(128);
		ASSERT_THAT(p, NotNull());
		std::memset(p, 0, 128);
	}
}

/*!
 * @brief CRecycledBuffer 取得のテスト
 */
TEST(CRecycledBuffer, FixedGetBuffer)
{
	CRecycledBuffer buf;

	size_t count = 0;
	auto* p = buf.GetBuffer<WCHAR>(&count);
	ASSERT_THAT(p, NotNull());
	EXPECT_THAT(count, Eq(buf.GetMaxCount<WCHAR>()));
}
