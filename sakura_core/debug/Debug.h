/*! @file */
/*
	Copyright (C) 2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_DEBUG_H_
#define SAKURA_DEBUG_H_
#pragma once

#include <cstdint>

namespace sakura {
namespace detail {

/*!
 * UI スレッド検証用のアサート関数
 *
 * この関数は、呼び出し元が UI スレッド上で実行されているかを検証します。
 * UI スレッド以外から呼び出された場合、デバッグビルドではアサートが発生します。
 *
 * @note この関数は GetEditWndHwndSafe() などの UI スレッド専用関数から呼び出されます。
 */
void AssertUiThread() noexcept;

/*!
 * UI スレッド ID を初期化する
 *
 * @param threadId 初期化する UI スレッドの ID
 */
void InitializeUiThreadId(std::uint32_t threadId) noexcept;

} // namespace detail
} // namespace sakura

#endif /* SAKURA_DEBUG_H_ */
