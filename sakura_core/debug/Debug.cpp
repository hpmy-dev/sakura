/*! @file */
/*
	Copyright (C) 2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "debug/Debug.h"
#include <cassert>
#include <Windows.h>

namespace sakura {
namespace detail {

// UI スレッドの ID を保持するグローバル変数
static std::uint32_t g_uiThreadId = 0;

/*!
 * UI スレッド ID を初期化する
 */
void InitializeUiThreadId(std::uint32_t threadId) noexcept
{
	g_uiThreadId = threadId;
}

/*!
 * UI スレッド検証用のアサート関数
 *
 * 現在のスレッドが UI スレッドであることを検証します。
 * UI スレッド以外から呼び出された場合、デバッグビルドではアサートが発生します。
 */
void AssertUiThread() noexcept
{
	// UI スレッドが初期化されていない場合は何もしない
	// （アプリケーション起動前の初期化フェーズ）
	if (g_uiThreadId == 0) {
		return;
	}

	// 現在のスレッド ID を取得
	const auto currentThreadId = ::GetCurrentThreadId();

	// UI スレッドかどうかを検証
	assert(currentThreadId == g_uiThreadId && "UI thread violation detected!");
}

} // namespace detail
} // namespace sakura
