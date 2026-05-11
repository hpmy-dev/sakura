/*! @file */
/*
	Copyright (C) 2021-2022, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "CConvert_CodeToSjis.h"

#include "charset/CCodeFactory.h"

/*!
	コンストラクタ
 */
CConvert_CodeToSjis::CConvert_CodeToSjis(ECodeType eCodeType) noexcept
	: m_eCodeType(eCodeType)
{
}

/*!
	文字コード変換 xxx→SJIS

	@date 2009/03/26 ryoji コード変換はできるだけANSI版のsakuraと互換の結果が得られるように実装する
 */
bool CConvert_CodeToSjis::DoConvert(CNativeW* pcData)
{
	// バッファの内容がANSI版相当になるよう Unicode→SJIS 変換する
	CMemory cmemSjis;
	std::unique_ptr<CCodeBase> pCodeSjis(
		CCodeFactory::CreateCodeBase(CODE_SJIS)
	);
	if( !pCodeSjis ){
		return false; // 変換失敗
	}
	pCodeSjis->UnicodeToCode(*pcData, &cmemSjis);

	// xxx→SJIS 変換後にバッファ内容をUNICODE版相当に戻す（SJIS→Unicode）のと等価な結果を得るために
	// xxx→Unicode 変換する
	std::unique_ptr<CCodeBase> pcCodeBase;
	if (m_eCodeType == CODE_JIS) {
		// JIS変換はbase64デコードを行うモードを使う
		auto pJisCodeBase = std::unique_ptr<CCodeBase>(CCodeFactory::CreateCodeBase(CODE_JIS, true));
		pcCodeBase = std::move(pJisCodeBase);
	}
	else {
		// その他の変換
		auto pOtherCodeBase = std::unique_ptr<CCodeBase>(
			CCodeFactory::CreateCodeBase(m_eCodeType)
		);
		pcCodeBase = std::move(pOtherCodeBase);
	}

	// nullptr チェックを追加
	if( !pcCodeBase ){
		return false; // 変換失敗
	}

	// 指定された文字コードに基づいてUnicode変換する（変換エラーは無視する）
	pcCodeBase->CodeToUnicode(cmemSjis, pcData);

	return true;
}
