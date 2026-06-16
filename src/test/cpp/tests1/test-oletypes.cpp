/*! @file */
/*
	Copyright (C) 2018-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "_os/OleTypes.h"

/*!
 * @brief SysString コピー代入のテスト（#2465）
 *
 * 新規確保 → 旧解放 → 差し替えの順で代入する。
 * 代入後の値が正しく、代入前が維持されることを確認する。
 */
TEST(OleTypes, SysStringAssign)
{
	SysString src(L"hello", 5);
	SysString dst(L"x", 1);

	dst = src;

	std::wstring sd;
	dst.GetW(&sd);
	EXPECT_THAT(sd, StrEq(L"hello"));
	EXPECT_THAT(dst.Length(), Eq(5));

	std::wstring ss;
	src.GetW(&ss);
	EXPECT_THAT(ss, StrEq(L"hello"));
}

/*!
 * @brief SysString 自己代入のテスト（#2465）
 *
 * 自己代入で内容が破壊されないことを確認する。
 */
TEST(OleTypes, SysStringSelfAssign)
{
	SysString s(L"keep", 4);

	SysString& ref = s;
	s = ref;

	std::wstring out;
	s.GetW(&out);
	EXPECT_THAT(out, StrEq(L"keep"));
	EXPECT_THAT(s.Length(), Eq(4));
}

/*!
 * @brief Variant コピー代入のテスト（#2466）
 *
 * 中間変数経由で代入され、代入後の値が正しく、
 * 代入前が維持されることを確認する。
 */
TEST(OleTypes, VariantAssign)
{
	Variant src;
	src.Receive(42);

	Variant dst;
	dst = src;

	EXPECT_THAT(dst.Data.vt, Eq(VT_I4));
	EXPECT_THAT(dst.Data.lVal, Eq(42));

	EXPECT_THAT(src.Data.vt, Eq(VT_I4));
	EXPECT_THAT(src.Data.lVal, Eq(42));
}

/*!
 * @brief Variant 自己代入のテスト（#2466）
 */
TEST(OleTypes, VariantSelfAssign)
{
	Variant v;
	v.Receive(7);

	Variant& ref = v;
	v = ref;

	EXPECT_THAT(v.Data.vt, Eq(VT_I4));
	EXPECT_THAT(v.Data.lVal, Eq(7));
}
