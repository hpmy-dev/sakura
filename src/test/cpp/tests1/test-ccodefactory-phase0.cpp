/*! @file */
/*
	Copyright (C) 2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "pch.h"
#include "charset/CCodeFactory.h"
#include "charset/CCodeBase.h"

/*!
	CCodeFactory::LoadFromCode のテスト
	不正な ECodeType で CreateCodeBase が nullptr を返す場合に RESULT_FAILURE が返ること（フェーズ 0 安全性）
 */
TEST(CCodeFactory, LoadFromCode_InvalidCodeType)
{
	// 不正な ECodeType（範囲外の値）
	const ECodeType invalidCodeType = static_cast<ECodeType>(9999);
	
	// テスト用のバイト列
	std::string testData = "test data";
	
	// LoadFromCode を呼び出す
	auto result = CCodeFactory::LoadFromCode(invalidCodeType, testData);
	
	// RESULT_FAILURE が返ることを確認
	ASSERT_EQ(RESULT_FAILURE, result.result);
	
	// 出力文字列が空であることを確認
	ASSERT_TRUE(result.destination.empty());
}

/*!
	CCodeFactory::ConvertToCode のテスト
	不正な ECodeType で CreateCodeBase が nullptr を返す場合に RESULT_FAILURE が返ること（フェーズ 0 安全性）
 */
TEST(CCodeFactory, ConvertToCode_InvalidCodeType)
{
	// 不正な ECodeType（範囲外の値）
	const ECodeType invalidCodeType = static_cast<ECodeType>(9999);
	
	// テスト用の文字列
	std::wstring testData = L"test data";
	
	// ConvertToCode を呼び出す
	auto result = CCodeFactory::ConvertToCode(invalidCodeType, testData);
	
	// RESULT_FAILURE が返ることを確認
	ASSERT_EQ(RESULT_FAILURE, result.result);
	
	// 出力バイト列が空であることを確認
	ASSERT_TRUE(result.destination.empty());
}

/*!
	CCodeFactory::LoadFromCode のテスト
	正常な ECodeType で正しく変換されること（基本動作確認）
 */
TEST(CCodeFactory, LoadFromCode_ValidCodeType)
{
	// UTF-8 エンコードされたテストデータ
	std::string utf8Data = u8"テスト";
	
	// LoadFromCode を呼び出す（UTF-8）
	auto result = CCodeFactory::LoadFromCode(CODE_UTF8, utf8Data);
	
	// RESULT_COMPLETE が返ることを確認
	ASSERT_EQ(RESULT_COMPLETE, result.result);
	
	// 変換結果が空でないことを確認
	ASSERT_FALSE(result.destination.empty());
}

/*!
	CCodeFactory::ConvertToCode のテスト
	正常な ECodeType で正しく変換されること（基本動作確認）
 */
TEST(CCodeFactory, ConvertToCode_ValidCodeType)
{
	// テスト用のUnicode文字列
	std::wstring testData = L"テスト";
	
	// ConvertToCode を呼び出す（UTF-8）
	auto result = CCodeFactory::ConvertToCode(CODE_UTF8, testData);
	
	// RESULT_COMPLETE が返ることを確認
	ASSERT_EQ(RESULT_COMPLETE, result.result);
	
	// 変換結果が空でないことを確認
	ASSERT_FALSE(result.destination.empty());
}
