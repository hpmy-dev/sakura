/*! @file */
/*
	Copyright (C) 2018-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "macro/CCookieManager.h"

/*!
 * @brief Cookie 名前検証のテスト（#2480）
 *
 * ValidateCookieName は英数字とアンダースコアのみ許可する。
 * 修正前は `L'_' <= c` の比較により '_'(0x5F) 以上の記号
 * （'`' 0x60 など）を誤って許可していた。修正後は `L'_' == c`
 * となるため、これらは無効（SetCookie 戻り値 2）になる。
 *
 * SetCookie 戻り値: 0=正常 / 1=scope不正 / 2=名前不正
 */
TEST(CCookieManager, ValidateCookieName)
{
	CCookieManager mgr;

	// 英数字・アンダースコアのみの名前は有効
	EXPECT_THAT(mgr.SetCookie(L"window", L"good_name_1", L"v", 1), Eq(0));
	EXPECT_THAT(mgr.SetCookie(L"window", L"_", L"v", 1), Eq(0));

	// '`'(0x60) は '_'(0x5F) 以上だが許可されない（修正後は 2）
	EXPECT_THAT(mgr.SetCookie(L"window", L"a`b", L"v", 1), Eq(2));

	// '_' 未満の記号（'-' 0x2D）は従来どおり無効
	EXPECT_THAT(mgr.SetCookie(L"window", L"a-b", L"v", 1), Eq(2));

	// scope が不正な場合は 1
	EXPECT_THAT(mgr.SetCookie(L"invalid", L"name", L"v", 1), Eq(1));
}
