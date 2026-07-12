/*! @file */
/*
	Copyright (C) 2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "io/CIoBridge.h"
#include "charset/CCodeBase.h"
#include "charset/CCodeFactory.h"
#include "mem/CMemory.h"
#include "mem/CNativeW.h"

/*!
	CIoBridge::FileToImpl のテスト
	pCode = nullptr で RESULT_FAILURE が返ること（フェーズ 0 安全性）
 */
TEST(CIoBridge, FileToImpl_NullCodeBase)
{
	// テスト用のソースデータ
	CMemory cSrc;
	cSrc.SetRawData("test", 4);
	
	// 変換先
	CNativeW cDst;
	
	// pCode = nullptr で FileToImpl を呼ぶ
	EConvertResult result = CIoBridge::FileToImpl(cSrc, &cDst, nullptr, 0);
	
	// RESULT_FAILURE が返ることを確認
	ASSERT_EQ(RESULT_FAILURE, result);
	
	// 出力が空文字列になっていることを確認
	ASSERT_EQ(0, cDst.GetStringLength());
}

/*!
	CIoBridge::ImplToFile のテスト
	pCode = nullptr で RESULT_FAILURE が返ること（フェーズ 0 安全性）
 */
TEST(CIoBridge, ImplToFile_NullCodeBase)
{
	// テスト用のソースデータ
	CNativeW cSrc;
	cSrc.SetString(L"test");
	
	// 変換先
	CMemory cDst;
	
	// pCode = nullptr で ImplToFile を呼ぶ
	EConvertResult result = CIoBridge::ImplToFile(cSrc, &cDst, nullptr);
	
	// RESULT_FAILURE が返ることを確認
	ASSERT_EQ(RESULT_FAILURE, result);
	
	// 出力が空データになっていることを確認
	ASSERT_EQ(0, cDst.GetRawLength());
}

/*!
	CIoBridge::FileToImpl の正常系テスト
	有効な pCode で正しく変換されること（基本動作確認）
 */
TEST(CIoBridge, FileToImpl_ValidCodeBase)
{
	// UTF-8 エンコードされたテストデータ
	const char* utf8Data = "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";
	CMemory cSrc;
	cSrc.SetRawData(utf8Data, strlen(utf8Data));
	
	// 変換先
	CNativeW cDst;
	
	// UTF-8 の CCodeBase を作成
	std::unique_ptr<CCodeBase> pCode(CCodeFactory::CreateCodeBase(CODE_UTF8, 0));
	ASSERT_NE(nullptr, pCode.get());
	
	// FileToImpl を呼ぶ
	EConvertResult result = CIoBridge::FileToImpl(cSrc, &cDst, pCode.get(), 0);
	
	// RESULT_COMPLETE が返ることを確認
	ASSERT_EQ(RESULT_COMPLETE, result);
	
	// 変換結果が正しい Unicode 文字列であることを確認 (= "テスト")
	const std::wstring expected = L"\u30C6\u30B9\u30C8";
	ASSERT_EQ(expected, std::wstring(cDst.GetStringPtr(), cDst.GetStringLength()));
}

/*!
	CIoBridge::ImplToFile の正常系テスト
	有効な pCode で正しく変換されること（基本動作確認）
 */
TEST(CIoBridge, ImplToFile_ValidCodeBase)
{
	// テスト用のUnicode文字列
	CNativeW cSrc;
	cSrc.SetString(L"テスト");
	
	// 変換先
	CMemory cDst;
	
	// UTF-8 の CCodeBase を作成
	std::unique_ptr<CCodeBase> pCode(CCodeFactory::CreateCodeBase(CODE_UTF8, 0));
	ASSERT_NE(nullptr, pCode.get());
	
	// ImplToFile を呼ぶ
	EConvertResult result = CIoBridge::ImplToFile(cSrc, &cDst, pCode.get());
	
	// RESULT_COMPLETE が返ることを確認
	ASSERT_EQ(RESULT_COMPLETE, result);
	
	// 変換結果が UTF-8 バイト列であることを確認 (= UTF-8 of "テスト")
	const std::string expectedUtf8 = "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";
	ASSERT_EQ(expectedUtf8, std::string(reinterpret_cast<const char*>(cDst.GetRawPtr()), cDst.GetRawLength()));
}
