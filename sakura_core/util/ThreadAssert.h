/*! @file */
/*
	Copyright (C) 2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_THREAD_ASSERT_H_
#define SAKURA_THREAD_ASSERT_H_
#pragma once

#include <source_location>
#include <crtdbg.h>
#include <Windows.h>

// UI スレッド ID（WinMain 等の初期化時に設定すること）
extern DWORD g_uiThreadId;

namespace sakura::detail {
	inline void AssertUiThread(
		std::source_location loc = std::source_location::current()) noexcept
	{
#if defined(_DEBUG)
		if (::GetCurrentThreadId() != g_uiThreadId) {
			_CrtDbgReport(_CRT_ASSERT, loc.file_name(),
						  static_cast<int>(loc.line()),
						  nullptr, "UI thread violation in %s",
						  loc.function_name());
		}
#endif
	}
} // namespace sakura::detail

#endif // SAKURA_THREAD_ASSERT_H_
