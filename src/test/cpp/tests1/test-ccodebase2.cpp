/*
	Copyright (C) 2021-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/

/**
 * @file test-ccodebase2.cpp
 * @brief CCodeBase / CCodeFactory の文字コード変換回帰テスト群
 *
 * 検証対象:
 *   - CCodeFactory::LoadFromCode   : バイト列 → wstring 変換
 *   - CCodeFactory::ConvertToCode  : wstring  → バイト列 変換
 *   - CCodeBase::MIMEHeaderDecode  : MIME ヘッダーのデコード
 *   - CCodeBase::GetBom            : BOM バイト列の取得
 *   - CCodeBase::GetEol            : 改行コードのバイト列取得
 *   - CMainStatusBar::UnicodeToHex : ステータスバー用16進表示変換
 *
 * 文字コードの対応範囲:
 *   Shift-JIS / ISO-2022-JP(JIS) / EUC-JP / Latin-1 /
 *   UTF-7 / UTF-8 / CESU-8 / UTF-16LE/BE / UTF-32LE/BE
 *
 * テストフレームワーク: Google Test (gtest)
 *   - 単純テスト : TEST() マクロ
 *   - パラメータ化テスト : TEST_P() + INSTANTIATE_TEST_SUITE_P()
 */

#include "pch.h"
#include "charset/CCodeFactory.h"

#include "window/CMainStatusBar.h"

namespace convert {

// =========================================================================
// バイト列生成ユーティリティ
//
// UTF-16/UTF-32 はバイナリデータとして扱うため、
// char 配列リテラルをそのまま使うと BOM や '\0' 終端の問題が生じる。
// 以下のヘルパー関数群は std::string にバイト列を明示的に詰め込む。
//
// Windows API の LOBYTE/HIBYTE/LOWORD/HIWORD マクロを使用することで
// エンディアン変換を自己文書化している。
// =========================================================================

/**
 * @brief ASCII 文字列を UTF-16LE バイト列に変換する
 *
 * ASCII 文字（< 0x80）は UTF-16 で上位バイト 0x00 + 下位バイト そのまま
 * という形になる（Little Endian = 下位先）。
 * 例: 'A'(0x41) → { 0x41, 0x00 }
 */
std::string ToUtf16LeBytes(std::string_view ascii)
{
	std::string dest;
	dest.reserve(ascii.size() * 2);
	std::ranges::for_each(ascii, [&dest](char8_t c) {
		dest += LOBYTE(c);	// 下位バイト先（LE）
		dest += '\0';		// ASCII は上位バイト常に 0
	});
	return dest;
}

/**
 * @brief wstring（UTF-16）を UTF-16LE バイト列に変換する
 *
 * Windows の wchar_t は 2 バイト UTF-16 コードユニット。
 * LOBYTE で下位バイト、HIBYTE で上位バイトを取り出して LE 順に詰める。
 * サロゲートペアも 2 コードユニット分そのまま変換される。
 */
std::string ToUtf16LeBytes(std::wstring_view wide)
{
	std::string dest;
	dest.reserve(wide.size() * 2);
	std::ranges::for_each(wide, [&dest](uchar16_t c) {
		dest += LOBYTE(c);	// 下位バイト先（LE）
		dest += HIBYTE(c);	// 上位バイト後
	});
	return dest;
}

/**
 * @brief ASCII 文字列を UTF-16BE バイト列に変換する
 *
 * Big Endian では上位バイトが先に来る。
 * 例: 'A'(0x41) → { 0x00, 0x41 }
 */
std::string ToUtf16BeBytes(std::string_view ascii)
{
	std::string dest;
	dest.reserve(ascii.size() * 2);
	std::ranges::for_each(ascii, [&dest](char8_t c) {
		dest += '\0';		// ASCII は上位バイト常に 0（BE では先）
		dest += LOBYTE(c);	// 下位バイト後
	});
	return dest;
}

/**
 * @brief wstring（UTF-16）を UTF-16BE バイト列に変換する
 *
 * HIBYTE（上位バイト）を先に、LOBYTE（下位バイト）を後に並べる。
 * ネットワーク通信や Mac 側ファイルとの相互運用で BE が使われることがある。
 */
std::string ToUtf16BeBytes(std::wstring_view wide)
{
	std::string dest;
	dest.reserve(wide.size() * 2);
	std::ranges::for_each(wide, [&dest](uchar16_t c) {
		dest += HIBYTE(c);	// 上位バイト先（BE）
		dest += LOBYTE(c);	// 下位バイト後
	});
	return dest;
}

/**
 * @brief ASCII 文字列を UTF-32LE バイト列に変換する
 *
 * UTF-32 は各コードポイントを 4 バイト固定長で表す。
 * ASCII の場合 U+0000〜U+007F なので上位 3 バイトはすべて 0x00。
 * LOWORD/HIWORD で 16 ビット単位を、さらに LOBYTE/HIBYTE で 8 ビット単位を取り出す。
 * 例: 'A'(0x00000041) LE → { 0x41, 0x00, 0x00, 0x00 }
 */
std::string ToUtf32LeBytes(std::string_view ascii)
{
	std::string dest;
	dest.reserve(ascii.size() * 4);
	std::ranges::for_each(ascii, [&dest](uchar32_t c) {
		dest += LOBYTE(LOWORD(c));	// バイト0（最下位）
		dest += HIBYTE(LOWORD(c));	// バイト1
		dest += LOBYTE(HIWORD(c));	// バイト2
		dest += HIBYTE(HIWORD(c));	// バイト3（最上位、ASCII では常に 0）
	});
	return dest;
}

/**
 * @brief wstring（UTF-16）を UTF-32LE バイト列に変換する
 *
 * Windows の wchar_t は 16 ビット（BMP 文字のみ直接表現）。
 * サロゲートペアは 2 コードユニットを 1 コードポイントに合成せず、
 * それぞれ 4 バイトとして展開する点に注意（このヘルパーの制約）。
 */
std::string ToUtf32LeBytes(std::wstring_view wide)
{
	std::string dest;
	dest.reserve(wide.size() * 2);
	std::ranges::for_each(wide, [&dest](uchar32_t c) {
		dest += LOBYTE(LOWORD(c));
		dest += HIBYTE(LOWORD(c));
		dest += LOBYTE(HIWORD(c));
		dest += HIBYTE(HIWORD(c));
	});
	return dest;
}

/**
 * @brief ASCII 文字列を UTF-32BE バイト列に変換する
 *
 * BE では最上位バイトが先頭に来る。
 * 例: 'A'(0x00000041) BE → { 0x00, 0x00, 0x00, 0x41 }
 */
std::string ToUtf32BeBytes(std::string_view ascii)
{
	std::string dest;
	dest.reserve(ascii.size() * 4);
	std::ranges::for_each(ascii, [&dest](uchar32_t c) {
		dest += HIBYTE(HIWORD(c));	// バイト3（最上位）先
		dest += LOBYTE(HIWORD(c));	// バイト2
		dest += HIBYTE(LOWORD(c));	// バイト1
		dest += LOBYTE(LOWORD(c));	// バイト0（最下位）後
	});
	return dest;
}

/**
 * @brief wstring（UTF-16）を UTF-32BE バイト列に変換する
 */
std::string ToUtf32BeBytes(std::wstring_view wide)
{
	std::string dest;
	dest.reserve(wide.size() * 2);
	std::ranges::for_each(wide, [&dest](uchar32_t c) {
		dest += HIBYTE(HIWORD(c));
		dest += LOBYTE(HIWORD(c));
		dest += HIBYTE(LOWORD(c));
		dest += LOBYTE(LOWORD(c));
	});
	return dest;
}

// =========================================================================
// MIME ヘッダーデコードテスト
//
// RFC 2047 の "encoded-word" 形式（=?charset?encoding?text?=）を
// CCodeBase::MIMEHeaderDecode がどう処理するかを検証する。
//
// encoding には B（Base64）と Q（Quoted-Printable）が存在する。
// charset と引数の eCodeType が一致しないと変換しない、という
// "コードタイプガード" の挙動も確認する。
// =========================================================================

/**
 * @brief MIMEヘッダーデコードテストのパラメーター
 *
 * @param eCodeType  呼び出し側が指定する文字コードセット種別
 *                   ヘッダー内の charset と不一致なら変換をスキップする
 * @param input      デコード対象の生文字列（MIME ヘッダー行全体）
 * @param optExpected デコード後のバイト列期待値。
 *                    変換されない場合は入力と同じ文字列が返る
 */
using MIMEHeaderDecodeTestParam = std::tuple<ECodeType, std::string_view, std::optional<std::string>>;

//! MIMEヘッダーデコードテストのフィクスチャ
struct MIMEHeaderDecodeTest : public ::testing::TestWithParam<MIMEHeaderDecodeTestParam> {};

/**
 * @brief MIMEHeaderDecode のデコード結果を検証する
 *
 * 戻り値（bool）が true なら変換成功、false なら変換なし（入力をそのまま返す）。
 * CMemory に格納されたバイト列を std::string_view で取り出して比較する。
 */
TEST_P(MIMEHeaderDecodeTest, DoDecode)
{
	const auto  eCodeType   = std::get<0>(GetParam());
	const auto  input       = std::get<1>(GetParam());
	const auto& optExpected = std::get<2>(GetParam());

	CMemory m;
	const auto result = CCodeBase::MIMEHeaderDecode(std::data(input), std::size(input), &m, eCodeType);

	EXPECT_THAT((bool)result, optExpected.has_value());

	if (optExpected.has_value()) {
		const std::string_view decoded{ LPCSTR(m.GetRawPtr()), size_t(m.GetRawLength()) };
		EXPECT_THAT(decoded, StrEq(*optExpected));
	}
}

/**
 * @brief MIMEHeaderDecode のパラメーター網羅テストケース一覧
 *
 * 検証シナリオ:
 *   1. Base64 + iso-2022-jp（JIS エスケープシーケンス含む）
 *   2. Base64 + UTF-8（マルチバイト日本語文字）
 *   3. Quoted-Printable + UTF-8（% エンコード形式）
 *   4. charset 不一致（引数 UTF-8 ≠ ヘッダー内 iso-2022-jp）→ 変換スキップ
 *   5. 非対応 charset（UTF-7）→ 変換スキップ
 *   6. 未知の encoding 識別子（'X'）→ 変換スキップ
 *   7. 末尾の "?=" がない不完全なヘッダー → 変換スキップ
 */
INSTANTIATE_TEST_SUITE_P(
	MIMEHeaderCases,
	MIMEHeaderDecodeTest,
	::testing::Values(
		// Base64 + JIS: "サクラ" を iso-2022-jp + Base64 エンコードしたもの
		// 期待値はデコード後の JIS バイト列（$B%5%/%i(B はセクション識別子 + かな）
		MIMEHeaderDecodeTestParam{ CODE_JIS,  "From: =?iso-2022-jp?B?GyRCJTUlLyVpGyhC?=",       "From: $B%5%/%i(B" },

		// Base64 + UTF-8: "サクラ" を UTF-8 + Base64 エンコードしたもの
		// 期待値は UTF-8 バイト列（E3 82 B5 = サ、E3 82 AF = ク、E3 83 A9 = ラ）
		MIMEHeaderDecodeTestParam{ CODE_UTF8, "From: =?utf-8?B?44K144Kv44Op?=",                 "From: \xe3\x82\xb5\xe3\x82\xaf\xe3\x83\xa9" },

		// Quoted-Printable + UTF-8: =XX 形式で UTF-8 バイトを個別エスケープ
		// 末尾の '!' は QP で変換不要な ASCII 文字なのでそのまま残る
		MIMEHeaderDecodeTestParam{ CODE_UTF8, "From: =?utf-8?Q?=E3=82=B5=E3=82=AF=E3=83=A9!?=", "From: \xe3\x82\xb5\xe3\x82\xaf\xe3\x83\xa9!" },

		// charset 不一致: 引数は UTF-8 だがヘッダーは iso-2022-jp
		// 実装は charset 一致を前提とするため変換せず入力をそのまま返す
		MIMEHeaderDecodeTestParam{ CODE_UTF8, "From: =?iso-2022-jp?B?GyRCJTUlLyVpGyhC?=",       "From: =?iso-2022-jp?B?GyRCJTUlLyVpGyhC?=" },

		// 非対応 charset（UTF-7）: Sakura Editor は UTF-7 の MIME ヘッダーデコードを実装しない
		MIMEHeaderDecodeTestParam{ CODE_UTF7, "From: =?utf-7?B?+MLUwrzDp-",                     "From: =?utf-7?B?+MLUwrzDp-" },

		// 未知の encoding 種別 'X': B でも Q でもない識別子 → 何もせずそのまま返す
		MIMEHeaderDecodeTestParam{ CODE_UTF8, "From: =?iso-2022-jp?X?GyRCJTUlLyVpGyhC?=",       "From: =?iso-2022-jp?X?GyRCJTUlLyVpGyhC?=" },

		// 末尾の "?=" がない不完全なエンコードワード → RFC 2047 準拠でスキップ
		MIMEHeaderDecodeTestParam{ CODE_JIS,  "From: =?iso-2022-jp?B?GyRCJTUlLyVpGyhC",         "From: =?iso-2022-jp?B?GyRCJTUlLyVpGyhC" }
	));

// =========================================================================
// Shift-JIS 変換テスト
//
// Windows 環境では CP932（Microsoft 拡張 Shift-JIS）が使われる。
// JIS X 0208 の標準 Shift-JIS とは NEC 特殊文字・IBM 拡張文字の扱いが異なる。
//
// 変換できない文字の処理方針:
//   LoadFromCode: 不正バイトは DC00+バイト値 のサロゲート代替（Lonely Surrogate）
//                 として wchar_t 列に格納する。
//   ConvertToCode: Unicode に存在するが CP932 にない文字は '?'（0x3F）に置換。
// =========================================================================

/**
 * @brief Shift-JIS（CP932）変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換（等価性確認）
 *   2. 半角カナ・ひらがな・カタカナ・漢字の往復変換
 *   3. CP932 で変換できない/不正なバイト列の挙動
 *   4. Unicode→CP932 変換不可文字（鷗: JIS 第4水準）の '?' 置換
 */
TEST(CCodeBase, codeSJis)
{
	const auto eCodeType = CODE_SJIS;
	std::unique_ptr<CCodeBase> pCodeBase = CCodeFactory::CreateCodeBase(eCodeType);

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 全文字（0x01〜0x7F）の往復変換
	//
	// ASCII は Shift-JIS の第1バイト範囲外なので、1バイト=1文字として処理される。
	// source/consumed を検証することで "変換した文字数" が一致することも確認する。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr const auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	// 正常系: 7bit ASCII 範囲（等価変換）
	auto result1 = CCodeFactory::LoadFromCode(eCodeType, mbsAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());
	EXPECT_EQ(std::string_view{mbsAscii}, result1.source);
	EXPECT_EQ(std::string_view{mbsAscii}.size(), result1.consumed);

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(mbsAscii));
	EXPECT_THAT(cresult1, IsTrue());
	EXPECT_EQ(std::wstring_view{wcsAscii}, cresult1.source);
	EXPECT_EQ(std::wstring_view{wcsAscii}.size(), cresult1.consumed);

	// -----------------------------------------------------------------------
	// 2. 半角カナ・ひらがな・カタカナ・漢字の往復変換（CP932 仕様）
	//
	// 半角カナ: 1バイト（0xA1〜0xDF）
	// ひらがな・カタカナ・漢字: 2バイト（第1バイト 0x81〜0x9F / 0xE0〜0xFC）
	//
	// mbsKanaKanji の内訳:
	//   \xB6\xC5          = ｶﾅ（半角カナ）
	//   \x82\xA9\x82\xC8  = かな（ひらがな 2バイト×2）
	//   \x83\x4A\x83\x69  = カナ（全角カタカナ 2バイト×2）
	//   \x8A\xBF\x8E\x9A  = 漢字（2バイト×2）
	// -----------------------------------------------------------------------
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";
	constexpr const auto& mbsKanaKanji = "\xB6\xC5\x82\xA9\x82\xC8\x83\x4A\x83\x69\x8A\xBF\x8E\x9A";

	// 正常系: かな漢字の往復変換
	auto result2 = CCodeFactory::LoadFromCode(eCodeType, mbsKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());
	EXPECT_EQ(std::string_view{mbsKanaKanji}, result2.source);
	EXPECT_EQ(std::string_view{mbsKanaKanji}.size(), result2.consumed);

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(mbsKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
	EXPECT_EQ(std::wstring_view{wcsKanaKanji}, cresult2.source);
	EXPECT_EQ(std::wstring_view{wcsKanaKanji}.size(), cresult2.consumed);

	// -----------------------------------------------------------------------
	// 3. CP932 で変換できない / 不正なバイト列の挙動
	//
	// CP932 には JIS X 0208 にない NEC 特殊文字（①など）が存在するが、
	// Unicode の逆変換が保証されない（NEC 選定 IBM 拡張文字 約400字）。
	// また第1バイト・第2バイトが規格外の範囲のバイト列も存在する。
	//
	// このような "変換できない" バイトは DC00+バイト値 のサロゲート代替値
	//（Lonely Surrogate）として wchar_t 列に格納される。
	// これにより情報を失わずにバイト列を wchar_t として保持できる。
	//
	// mbsCantConvSJis の内訳:
	//   \x87\x40  = ① NEC 特殊文字（Unicode 逆変換不可）
	//   \xED\x40  = 纊 NEC 選定 IBM 拡張文字
	//   \xFA\x40  = ⅰ IBM 拡張文字
	//   \x80\x40  = 第1バイト 0x80 は CP932 規格外
	//   \xFD〜\xFF\x40 = 第1バイト 0xFD/FE/FF は CP932 規格外
	//   \x81\x0A 等  = 第2バイトが規格範囲外（0x40〜0x7E / 0x80〜0xFC 以外）
	//
	// 👈 仕様バグ: 変換できない文字が含まれているのに IsTrue() になっている。
	//    本来は IsFalse() が返るべき。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsCantConvSJis =
		"\x87\x40\xED\x40\xFA\x40"					// "①纊ⅰ" NEC拡張、NEC選定IBM拡張、IBM拡張
		"\x80\x40\xFD\x40\xFE\x40\xFF\x40"			// 第1バイト不正
		"\x81\x0A\x81\x7F\x81\xFD\x81\xFE\x81\xFF"	// 第2バイト不正
		;
	constexpr const auto& wcsCantConvSJis =
		L"①\xDCED\xDC40ⅰ"
		L"\xDC80@\xDCFD@\xDCFE@\xDCFF@"
		L"\xDC81\n\xDC81\x7F\xDC81\xDCFD\xDC81\xDCFE\xDC81\xDCFF"
		;

	auto result3 = CCodeFactory::LoadFromCode(eCodeType, mbsCantConvSJis);
	EXPECT_THAT(result3.destination, StrEq(wcsCantConvSJis));
	EXPECT_THAT(result3, IsTrue());	//👈 仕様バグ。変換できないので false が返るべき。
	EXPECT_EQ(std::string_view{mbsCantConvSJis}, result3.source);
	EXPECT_EQ(std::string_view{mbsCantConvSJis}.size(), result3.consumed);

	// -----------------------------------------------------------------------
	// 4. Unicode→CP932 変換不可文字: 「鷗」(U+9DD7) の '?' 置換
	//
	// 「森鷗外」の「鷗」は JIS 第4水準漢字（JIS X 0213）だが、
	// CP932 には含まれない。ConvertToCode は変換できない文字を
	// '?'（0x3F）に置換し、IsFalse() を返す。
	// 結果: 森?外 = \x90\x58\x3F\x8A\x4F
	//        森=\x90\x58, ?=\x3F, 外=\x8A\x4F
	// -----------------------------------------------------------------------
	constexpr const auto& wcsOGuy = L"森鷗外";
	constexpr const auto& mbsOGuy = "\x90\x58\x3F\x8A\x4F"; //森?外

	const auto cresult4 = CCodeFactory::ConvertToCode(eCodeType, wcsOGuy);
	EXPECT_THAT(cresult4.destination, StrEq(mbsOGuy));
	EXPECT_THAT(cresult4, IsFalse());
	EXPECT_EQ(std::wstring_view{wcsOGuy}, cresult4.source);
	EXPECT_EQ(std::wstring_view{wcsOGuy}.size(), cresult4.consumed);
}

// =========================================================================
// ISO-2022-JP (JIS) 変換テスト
//
// ISO-2022-JP はエスケープシーケンスで文字セットを切り替えるステートフルな
// エンコーディング。メール本文でよく使われる。
//
// エスケープシーケンス一覧（主要なもの）:
//   ESC ( B  = ASCII モードに戻す（最も一般的）
//   ESC ( I  = 半角カナモード
//   ESC $ B  = JIS X 0208 漢字モード
//   ESC ( D  = JIS X 0212 補助漢字モード（非対応）
//
// 実装の既知バグ:
//   1. 不正なエスケープシーケンスをエラーバイナリに格納していない
//   2. C0 制御文字 DEL(0x7F) を取り込めていない
//   3. エラーバイナリを正しく復元していない
// =========================================================================

/**
 * @brief ISO-2022-JP (JIS) 変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII のデコード（C0 制御文字の不具合確認含む）
 *   2. かな漢字の往復変換（ESC シーケンス含む）
 *   3. JIS 範囲外バイト列の RESULT_LOSESOME 確認
 *   4. 未対応エスケープシーケンス（JIS X 0212）の RESULT_LOSESOME 確認
 *   5. Unicode→JIS 変換不可文字の RESULT_LOSESOME 確認
 */
TEST(CCodeBase, codeJis)
{
	const auto eCodeType = CODE_JIS;

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 範囲のデコード（ISO-2022-JP 仕様）
	//
	// ISO-2022-JP では ESC シーケンスがない状態でも ASCII を受け入れるが、
	// C0 制御文字のうち ESC(0x1B)/FS(0x1C)/GS(0x1D)/RS(0x1E)/US(0x1F)/SP(0x20)
	// は不正なエスケープシーケンスとして誤認識される。
	//
	// 期待値に '?' が含まれているのは変換失敗を示す。
	// wcsAscii は "本来あるべき" 変換結果で、途中で直接代入して
	// ConvertToCode のテストに使う。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsAscii = "\x1\x2\x3\x4\x5\x6\a\b\t\n\v\f\r\xE\xF\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr const auto& wcsAscii = L"\x1\x2\x3\x4\x5\x6\a\b\t\n\v\f\r\xE\xF\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\xDC1B\xDC1C\xDC1D\xDC1E\xDC1F\xDC20!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	// 不具合1: 不正なエスケープシーケンスをエラーバイナリに格納していない
	// 不具合2: C0 領域の文字 DEL(0x7F) を取り込めていない
	auto result1 = CCodeFactory::LoadFromCode(eCodeType, mbsAscii);
	EXPECT_THAT(result1.destination, StrEq(L"\x1\x2\x3\x4\x5\x6\a\b\t\n\v\f\r\xE\xF\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A???!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~?"));
	EXPECT_THAT(result1, IsFalse());	// 不正エスケープシーケンスを検出しているので false は正しい
	EXPECT_EQ(std::string_view{mbsAscii}, result1.source);
	EXPECT_EQ(std::string_view{mbsAscii}.size(), result1.consumed);

	// 不具合3: エラーバイナリを正しく復元していない
	// wcsAscii を手動で destination に代入して ConvertToCode をテストする
	result1.destination = wcsAscii;

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq("\x1\x2\x3\x4\x5\x6\a\b\t\n\v\f\r\xE\xF\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A??????!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F"));
	EXPECT_THAT(cresult1, IsFalse());	// 本来はエラーバイナリの復元なので true を返すべき（バグ）
	EXPECT_EQ(std::wstring_view{wcsAscii}, cresult1.source);
	EXPECT_EQ(std::wstring_view{wcsAscii}.size(), cresult1.consumed);

	// -----------------------------------------------------------------------
	// 2. かな漢字の往復変換（ISO-2022-JP 仕様）
	//
	// mbsKanaKanji の ESC シーケンス構造:
	//   \x1B(I  = 半角カナモード開始
	//   6E      = ｶﾅ（半角カナ 2 文字）
	//   \x1B$B  = JIS X 0208 漢字モード開始
	//   $+$J    = かな（U+304B U+306A）
	//   %+%J    = カナ（U+30AB U+30CA）
	//   4A;z    = 漢字（U+6F22 U+5B57）
	//   \x1B(B  = ASCII モードに戻す
	// -----------------------------------------------------------------------
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";
	constexpr const auto& mbsKanaKanji = "\x1B(I6E\x1B$B$+$J%+%J4A;z\x1B(B";

	auto result2 = CCodeFactory::LoadFromCode(eCodeType, mbsKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());
	EXPECT_EQ(std::string_view{mbsKanaKanji}, result2.source);
	EXPECT_EQ(std::string_view{mbsKanaKanji}.size(), result2.consumed);

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(mbsKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
	EXPECT_EQ(std::wstring_view{wcsKanaKanji}, cresult2.source);
	EXPECT_EQ(std::wstring_view{wcsKanaKanji}.size(), cresult2.consumed);

	// -----------------------------------------------------------------------
	// 3. JIS 範囲外バイト（0xFF 0xFF）→ RESULT_LOSESOME
	//
	// JIS X 0208 の有効範囲は 0x21〜0x7E。0xFF は範囲外なので変換失敗。
	// Windows CP932 拡張があるため厳密な境界テストは困難だが、
	// 0xFF は明確に範囲外。
	// -----------------------------------------------------------------------
	EXPECT_THAT(CCodeFactory::LoadFromCode(eCodeType, "\x1B$B\xFF\xFF\x1B(B").result, RESULT_LOSESOME);

	// -----------------------------------------------------------------------
	// 4. 未対応エスケープシーケンス（JIS X 0212 補助漢字モード ESC ( D）
	//
	// JIS X 0212 は「森鷗外」の「鷗」など JIS X 0208 に含まれない文字を扱えるが、
	// Sakura Editor の ISO-2022-JP 実装は対応していない。
	// ESC ( D 以降の文字はデコードできずに RESULT_LOSESOME が返る。
	// -----------------------------------------------------------------------
	EXPECT_THAT(CCodeFactory::LoadFromCode(eCodeType, "\x1B(D33\x1B(B").result, RESULT_LOSESOME);

	// -----------------------------------------------------------------------
	// 5. Unicode→JIS 変換不可文字: 「鷗」(U+9DD7)
	//
	// 「鷗」は JIS X 0208 に存在しない（JIS 第4水準）。
	// Shift-JIS（CP932）でも変換できないため RESULT_LOSESOME が返る。
	// -----------------------------------------------------------------------
	EXPECT_THAT(CCodeFactory::ConvertToCode(eCodeType, L"鷗").result, RESULT_LOSESOME);	// 森鴎外の鷗
}

// =========================================================================
// EUC-JP 変換テスト
//
// EUC-JP（Extended Unix Code for Japanese）は Unix 系システムで使われる
// 日本語エンコーディング。
//
// バイト構造:
//   1バイト文字: 0x00〜0x7F（ASCII 互換）
//   2バイト文字: 0xA1〜0xFE + 0xA1〜0xFE（JIS X 0208）
//   半角カナ:   0x8E + 0xA1〜0xDF（SS2 コードセット 2）
//   補助漢字:   0x8F + 0xA1〜0xFE + 0xA1〜0xFE（SS3、JIS X 0212）
//
// 「森鷗外」の「鷗」について:
//   JIS X 0212 を使えば EUC-JP で正確に表現できるが、
//   Sakura Editor の実装は SS3 に対応していないため '?' に置換される。
// =========================================================================

/**
 * @brief EUC-JP 変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換
 *   2. 半角カナ・ひらがな・カタカナ・漢字の往復変換
 *   3. EUC-JP 変換不可文字のプレースホルダー（現在は空）
 *   4. JIS X 0212 非対応による '?' 置換（「鷗」）
 */
TEST(CCodeBase, codeEucJp)
{
	const auto eCodeType = CODE_EUC;
	std::unique_ptr<CCodeBase> pCodeBase = CCodeFactory::CreateCodeBase(eCodeType);

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 全文字の往復変換
	// EUC-JP は ASCII 互換なので 0x01〜0x7F はそのままマッピングされる。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr const auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	auto result1 = CCodeFactory::LoadFromCode(eCodeType, mbsAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(mbsAscii));
	EXPECT_THAT(cresult1, IsTrue());

	// -----------------------------------------------------------------------
	// 2. 半角カナ・ひらがな・カタカナ・漢字の往復変換（EUC-JP 仕様）
	//
	// mbsKanaKanji の内訳（EUC-JP バイト値）:
	//   \x8E\xB6\x8E\xC5  = ｶﾅ（SS2 + 0xB6/0xC5 で半角カナ）
	//   \xA4\xAB\xA4\xCA  = かな（JIS 0x2422/0x242A の EUC-JP 表現 A4xx）
	//   \xA5\xAB\xA5\xCA  = カナ（JIS 0x2522/0x252A の EUC-JP 表現 A5xx）
	//   \xB4\xC1\xBB\xFA  = 漢字（JIS 0x34A1/0x3B7A の EUC-JP 表現）
	// -----------------------------------------------------------------------
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";
	constexpr const auto& mbsKanaKanji = "\x8E\xB6\x8E\xC5\xA4\xAB\xA4\xCA\xA5\xAB\xA5\xCA\xB4\xC1\xBB\xFA";

	auto result2 = CCodeFactory::LoadFromCode(eCodeType, mbsKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(mbsKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());

	// -----------------------------------------------------------------------
	// 3. EUC-JP 変換不可バイトのプレースホルダー（現在は空・保留）
	//
	// 不正バイト列のテストは後続の開発で追加予定。
	// Shift-JIS と異なり EUC-JP のバイト範囲は厳格（A1〜FE）なため
	// 境界値テストが追加しやすい。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsCantConvEucJp = ""	/* 第1バイト不正 */ ""	/* 第2バイト不正 */;
	constexpr const auto& wcsCantConvEucJp = L"" L"";

	auto result3 = CCodeFactory::LoadFromCode(eCodeType, mbsCantConvEucJp);
	// 将来的に以下を有効化する:
	//ASSERT_THAT(result3.destination, StrEq(wcsCantConvEucJp));
	//ASSERT_THAT(result3.losesome, IsFalse());

	// -----------------------------------------------------------------------
	// 4. JIS X 0212 非対応による '?' 置換: 「鷗」(U+9DD7)
	//
	// 本来の EUC-JP では JIS X 0212 を使って「鷗」を表現できる
	//（\x8F\xEC\x3F に相当）が、Sakura Editor の実装は SS3 未対応のため
	// '?'（0x3F）に置換される。
	//
	// 期待値: 森?外 = \xBF\xB9 \x3F \xB3\xB0
	//   森=\xBF\xB9（JIS 0x3F59），?=\x3F，外=\xB3\xB0（JIS 0x3330）
	// -----------------------------------------------------------------------
	constexpr const auto& wcsOGuy = L"森鷗外";
	constexpr const auto& mbsOGuy = "\xBF\xB9\x3F\xB3\xB0"; //森?外

	const auto cresult4 = CCodeFactory::ConvertToCode(eCodeType, wcsOGuy);
	EXPECT_THAT(cresult4.destination, StrEq(mbsOGuy));
	EXPECT_THAT(cresult4, IsFalse());
	EXPECT_EQ(std::wstring_view{wcsOGuy}, cresult4.source);
	EXPECT_EQ(std::wstring_view{wcsOGuy}.size(), cresult4.consumed);
}

// =========================================================================
// Latin-1（ISO-8859-1）変換テスト
//
// Latin-1 は西ヨーロッパ言語向けの 1 バイト文字コード。
// 0x00〜0xFF の全バイトが Unicode の U+0000〜U+00FF に直接マッピングされる。
// そのため "Unicodeに変換できない文字は存在しない"（全バイトが有効）。
//
// Windows の実装（CP1252）は Latin-1 の 0x80〜0x9F を独自に使用しているため、
// 厳密な ISO-8859-1 と挙動が異なる場合がある（0x81, 0x8D, 0x8F, 0x90, 0x9D
// は CP1252 で未割り当てだがバイト値そのまま出力される）。
// =========================================================================

/**
 * @brief Latin-1（ISO-8859-1 / CP1252）変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換
 *   2. 0x80〜0xFF 全文字の往復変換（CP1252 拡張文字含む）
 *   3. Unicode→Latin-1 変換不可文字（日本語）の '?' 置換
 */
TEST(CCodeBase, codeLatin1)
{
	const auto eCodeType = CODE_LATIN1;

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 範囲の往復変換（等価変換）
	// -----------------------------------------------------------------------
	constexpr const auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr const auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	auto result1 = CCodeFactory::LoadFromCode(eCodeType, mbsAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(mbsAscii));
	EXPECT_THAT(cresult1, IsTrue());

	// -----------------------------------------------------------------------
	// 2. 0x80〜0xFF 全 128 文字の往復変換（CP1252 拡張文字含む）
	//
	// 0x80〜0x9F は CP1252 で独自割り当て（€ ‚ ƒ … Š œ Ž ž Ÿ など）。
	// 0x81, 0x8D, 0x8F, 0x90, 0x9D は CP1252 の未割り当て位置だが
	// バイト値そのまま（U+0081 等として）出力される点に注意。
	// -----------------------------------------------------------------------
	constexpr const auto& wcsLatin1ExtChars =
		L"€\x81‚ƒ„…†‡ˆ‰Š‹Œ\x8DŽ\x8F"
		L"\x90''""•–—˜™š›œ\x9DžŸ"
		L"\xA0¡¢£¤¥¦§¨©ª«¬\xAD®¯"
		L"°±²³´µ¶·¸¹º»¼½¾¿"
		L"ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ"
		L"ÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß"
		L"àáâãäåæçèéêëìíîï"
		L"ðñòóôõö÷øùúûüýþÿ"
		;
	constexpr const auto& mbsLatin1ExtChars =
		"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
		"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
		"\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF"
		"\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
		"\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF"
		"\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
		"\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF"
		"\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF"
		;

	auto result2 = CCodeFactory::LoadFromCode(eCodeType, mbsLatin1ExtChars);
	EXPECT_THAT(result2.destination, StrEq(wcsLatin1ExtChars));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(mbsLatin1ExtChars));
	EXPECT_THAT(cresult2, IsTrue());

	// -----------------------------------------------------------------------
	// 3. Unicode→Latin-1 変換不可文字（日本語かな漢字）の '?' 置換
	//
	// Latin-1 は U+0000〜U+00FF しか表現できないため、
	// 日本語文字はすべて '?'（0x3F）に置換される。
	// 8 文字 → "????????"（8 バイト）
	// -----------------------------------------------------------------------
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";
	constexpr const auto& mbsKanaKanji = "????????";

	const auto cresult4 = CCodeFactory::ConvertToCode(eCodeType, wcsKanaKanji);
	EXPECT_THAT(cresult4.destination, StrEq(mbsKanaKanji));
	EXPECT_THAT(cresult4, IsFalse());
}

// =========================================================================
// UTF-8 変換テスト
//
// UTF-8 は可変長エンコーディング（1〜4 バイト）。
// U+0000〜U+007F: 1 バイト（ASCII 互換）
// U+0080〜U+07FF: 2 バイト（110xxxxx 10xxxxxx）
// U+0800〜U+FFFF: 3 バイト（1110xxxx 10xxxxxx 10xxxxxx）
// U+10000〜U+10FFFF: 4 バイト（11110xxx 10xxxxxx 10xxxxxx 10xxxxxx）
//
// 日本語文字（ひらがな・漢字）は BMP 内（U+3000〜U+9FFF）なので 3 バイト。
// =========================================================================

/**
 * @brief UTF-8 変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換
 *   2. 日本語文字（3 バイト UTF-8）の往復変換
 */
TEST(CCodeBase, codeUtf8)
{
	const auto eCodeType = CODE_UTF8;
	std::unique_ptr<CCodeBase> pCodeBase = CCodeFactory::CreateCodeBase(eCodeType);

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 全文字の往復変換
	// UTF-8 は ASCII 互換なので 0x01〜0x7F はバイト値そのまま。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr const auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	auto result1 = CCodeFactory::LoadFromCode(eCodeType, mbsAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(mbsAscii));
	EXPECT_THAT(cresult1, IsTrue());

	// -----------------------------------------------------------------------
	// 2. 日本語文字の往復変換（UTF-8 仕様）
	//
	// u8"..." リテラルは C++20 で char8_t 配列になるため
	// LPCSTR（const char*）にキャストして LoadFromCode に渡す。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsKanaKanji = u8"ｶﾅかなカナ漢字";
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";

	auto result2 = CCodeFactory::LoadFromCode(eCodeType, (LPCSTR)mbsKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq((LPCSTR)mbsKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
}

// =========================================================================
// CESU-8（Oracle 拡張 UTF-8）変換テスト
//
// CESU-8（Compatibility Encoding Scheme for UTF-16）は
// UTF-16 のサロゲートペアを UTF-8 ルールでそのままバイト化したもの。
// Oracle Database や古い Java 実装で使われる。
//
// UTF-8 との違い:
//   UTF-8:   U+10000 以上は 4 バイト（11110xxx ...）
//   CESU-8:  上位サロゲート（3 バイト: ED A0〜BF xx）
//             + 下位サロゲート（3 バイト: ED B0〜BF xx）= 6 バイト
//
// BMP 文字（U+0000〜U+FFFF）については UTF-8 と同じバイト列になる。
// =========================================================================

/**
 * @brief CESU-8（Oracle 拡張 UTF-8）変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換（UTF-8 と同一）
 *   2. BMP 日本語文字の往復変換（UTF-8 と同一）
 */
TEST(CCodeBase, codeUtf8_OracleImplementation)
{
	const auto eCodeType = CODE_CESU8;
	std::unique_ptr<CCodeBase> pCodeBase = CCodeFactory::CreateCodeBase(eCodeType);

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 全文字の往復変換
	// BMP 範囲内なので CESU-8 と UTF-8 は同じバイト列になる。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr const auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	auto result1 = CCodeFactory::LoadFromCode(eCodeType, mbsAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(mbsAscii));
	EXPECT_THAT(cresult1, IsTrue());

	// -----------------------------------------------------------------------
	// 2. BMP 日本語文字の往復変換（UTF-8 と同一バイト列）
	// ひらがな・漢字はすべて BMP 内なので CESU-8 = UTF-8 となる。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsKanaKanji = u8"ｶﾅかなカナ漢字";
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";

	auto result2 = CCodeFactory::LoadFromCode(eCodeType, (LPCSTR)mbsKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq((LPCSTR)mbsKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
}

// =========================================================================
// UTF-7 変換テスト
//
// UTF-7（RFC 2152）は 7 ビット通信路（SMTP など）で Unicode を送るための
// エンコーディング。Base64 変形を使って non-ASCII バイトを '+...−' で囲む。
//
// 直接表現できる "Set D" 文字（安全な ASCII 文字）:
//   A-Z a-z 0-9 ' ( ) , - . / : ? space TAB LF CR
// '+' 自体はリテラルとして '+-' と表現する。
//
// 現在は新規メールでの使用は非推奨（RFC 3501 で IMAP 以外での使用非奨励）。
// Sakura Editor では読み込み専用として実装されている可能性がある。
// =========================================================================

/**
 * @brief UTF-7 変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換（Base64 + Set D 混在）
 *   2. 日本語文字の往復変換（完全 Base64 エンコード）
 *   3. '+' 文字の '+-' エスケープ往復変換
 */
TEST(CCodeBase, codeUtf7)
{
	const auto eCodeType = CODE_UTF7;

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 全文字の往復変換（UTF-7 仕様）
	//
	// ASCII でも非 Set D 文字（< > [ ] { } ~ \ など）は
	// '+BASE64-' 形式でエンコードされる。
	// mbsAscii はそれを手動で組み立てた UTF-7 表現。
	// -----------------------------------------------------------------------
	constexpr const auto& mbsAscii = "+AAEAAgADAAQABQAGAAcACA-\t\n+AAsADA-\r+AA4ADwAQABEAEgATABQAFQAWABcAGAAZABoAGwAcAB0AHgAf- +ACEAIgAjACQAJQAm-'()+ACoAKw-,-./0123456789:+ADsAPAA9AD4-?+AEA-ABCDEFGHIJKLMNOPQRSTUVWXYZ+AFsAXABdAF4AXwBg-abcdefghijklmnopqrstuvwxyz+AHsAfAB9AH4Afw-";
	constexpr const auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	auto result1 = CCodeFactory::LoadFromCode(eCodeType, mbsAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(mbsAscii));
	EXPECT_THAT(cresult1, IsTrue());

	// -----------------------------------------------------------------------
	// 2. 日本語文字の往復変換（UTF-7 仕様）
	//
	// "+/3b/hTBLMGowqzDKbyJbVw-" の Base64 デコード結果:
	//   UTF-16BE: FF76 FF85 3042 306A 30AB 30CA 6F22 5B57
	//   Unicode:  ｶ    ﾅ    か   な   カ   ナ   漢   字
	// -----------------------------------------------------------------------
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";
	constexpr const auto& mbsKanaKanji = "+/3b/hTBLMGowqzDKbyJbVw-";

	auto result2 = CCodeFactory::LoadFromCode(eCodeType, mbsKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(mbsKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());

	// -----------------------------------------------------------------------
	// 3. '+' 文字のエスケープ往復変換
	//
	// UTF-7 では '+' をリテラルとして使う場合 '+-' と表現する。
	// "C++" → "C+-+-" の変換が正しく往復することを確認。
	// これはプログラミング言語名など '+' を多用するテキストで重要。
	// -----------------------------------------------------------------------
	constexpr const auto& wcsPlusPlus = L"C++";
	constexpr const auto& mbsPlusPlus = "C+-+-";

	auto result5 = CCodeFactory::LoadFromCode(eCodeType, mbsPlusPlus);
	EXPECT_THAT(result5.destination, StrEq(wcsPlusPlus));
	EXPECT_THAT(result5, IsTrue());

	auto cresult5 = CCodeFactory::ConvertToCode(eCodeType, result5.destination);
	EXPECT_THAT(cresult5.destination, StrEq(mbsPlusPlus));
	EXPECT_THAT(cresult5, IsTrue());
}

// =========================================================================
// UTF-16LE 変換テスト
//
// UTF-16LE（Little Endian）は Windows のネイティブ文字列形式。
// wchar_t は 16 ビット（BMP 文字はそのまま、BMP 外はサロゲートペア）。
// ファイル先頭に BOM（0xFF 0xFE）を付けることで LE を識別する。
//
// 注意: ToUtf16LeBytes() ヘルパーを使ってバイト列を準備する。
// =========================================================================

/**
 * @brief UTF-16LE 変換テスト
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換
 *   2. 日本語文字の往復変換
 */
TEST(CCodeBase, codeUtf16Le)
{
	const auto eCodeType = CODE_UTF16LE;

	// -----------------------------------------------------------------------
	// 1. 7bit ASCII 全文字の往復変換
	// ToUtf16LeBytes() で ASCII バイト列を UTF-16LE 形式に変換してから
	// LoadFromCode に渡す。各文字は 2 バイト（下位バイト + 0x00）になる。
	// -----------------------------------------------------------------------
	constexpr auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	const auto bytesAscii = ToUtf16LeBytes(mbsAscii);
	auto result1 = CCodeFactory::LoadFromCode(eCodeType, bytesAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(bytesAscii));
	EXPECT_THAT(cresult1, IsTrue());

	// -----------------------------------------------------------------------
	// 2. 日本語文字の往復変換（UTF-16LE 仕様）
	// wstring をそのままバイト列化する。Windows では wchar_t = UTF-16LE なので
	// ToUtf16LeBytes(wstring) は実質的にメモリコピーと同等。
	// -----------------------------------------------------------------------
	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";

	const auto bytesKanaKanji = ToUtf16LeBytes(wcsKanaKanji);
	auto result2 = CCodeFactory::LoadFromCode(eCodeType, bytesKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(bytesKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
}

/**
 * @brief UTF-16BE 変換テスト
 *
 * UTF-16BE は上位バイト先行形式（ネットワーク標準・Java 内部表現）。
 * BOM は 0xFE 0xFF。ToUtf16BeBytes() で HIBYTE/LOBYTE の順に変換する。
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換
 *   2. 日本語文字の往復変換
 */
TEST(CCodeBase, codeUtf16Be)
{
	const auto eCodeType = CODE_UTF16BE;

	constexpr auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	const auto bytesAscii = ToUtf16BeBytes(mbsAscii);
	auto result1 = CCodeFactory::LoadFromCode(eCodeType, bytesAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(bytesAscii));
	EXPECT_THAT(cresult1, IsTrue());

	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";

	const auto bytesKanaKanji = ToUtf16BeBytes(wcsKanaKanji);
	auto result2 = CCodeFactory::LoadFromCode(eCodeType, bytesKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(bytesKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
}

/**
 * @brief UTF-32LE 変換テスト
 *
 * UTF-32 は全コードポイントを 4 バイト固定長で表す。
 * BOM は 0xFF 0xFE 0x00 0x00（LE）。
 * メモリ消費は大きいが文字列の添え字アクセスが O(1) になる利点がある。
 * Windows では使用頻度が低く、Unix 系（wchar_t = 4バイト）で主に使われる。
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換（各文字が 4 バイト）
 *   2. 日本語文字の往復変換
 */
TEST(CCodeBase, codeUtf32Le)
{
	const auto eCodeType = CODE_UTF32LE;

	constexpr auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	// ASCII 1 文字が 4 バイトになるため bytesAscii.size() = mbsAscii長 × 4
	const auto bytesAscii = ToUtf32LeBytes(mbsAscii);
	auto result1 = CCodeFactory::LoadFromCode(eCodeType, bytesAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(bytesAscii));
	EXPECT_THAT(cresult1, IsTrue());

	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";

	const auto bytesKanaKanji = ToUtf32LeBytes(wcsKanaKanji);
	auto result2 = CCodeFactory::LoadFromCode(eCodeType, bytesKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(bytesKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
}

/**
 * @brief UTF-32BE 変換テスト
 *
 * BOM は 0x00 0x00 0xFE 0xFF（BE）。
 * ネットワーク標準バイト順（ビッグエンディアン）で格納される。
 *
 * 検証項目:
 *   1. 7bit ASCII 全文字の往復変換
 *   2. 日本語文字の往復変換
 */
TEST(CCodeBase, codeUtf32Be)
{
	const auto eCodeType = CODE_UTF32BE;

	constexpr auto& mbsAscii = "\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";
	constexpr auto& wcsAscii = L"\x01\x02\x03\x04\x05\x06\a\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F";

	const auto bytesAscii = ToUtf32BeBytes(mbsAscii);
	auto result1 = CCodeFactory::LoadFromCode(eCodeType, bytesAscii);
	EXPECT_THAT(result1.destination, StrEq(wcsAscii));
	EXPECT_THAT(result1, IsTrue());

	auto cresult1 = CCodeFactory::ConvertToCode(eCodeType, result1.destination);
	EXPECT_THAT(cresult1.destination, StrEq(bytesAscii));
	EXPECT_THAT(cresult1, IsTrue());

	constexpr const auto& wcsKanaKanji = L"ｶﾅかなカナ漢字";

	const auto bytesKanaKanji = ToUtf32BeBytes(wcsKanaKanji);
	auto result2 = CCodeFactory::LoadFromCode(eCodeType, bytesKanaKanji);
	EXPECT_THAT(result2.destination, StrEq(wcsKanaKanji));
	EXPECT_THAT(result2, IsTrue());

	auto cresult2 = CCodeFactory::ConvertToCode(eCodeType, result2.destination);
	EXPECT_THAT(cresult2.destination, StrEq(bytesKanaKanji));
	EXPECT_THAT(cresult2, IsTrue());
}

// =========================================================================
// BOM（Byte Order Mark）取得テスト
//
// BOM はファイル先頭に付加されるバイト列で、エンコーディングとバイト順を識別する。
// Unicode 仕様では省略可能だが、Windows テキストファイルでは慣習的に付加される。
//
// 各エンコーディングの BOM:
//   UTF-7:    2B 2F 76 38 2D（"+/v8-"）※ UTF-7 エンコードされた U+FEFF
//   UTF-16LE: FF FE
//   UTF-16BE: FE FF
//   UTF-32LE: FF FE 00 00（UTF-16LE BOM + 2 NUL）
//   UTF-32BE: 00 00 FE FF
//   UTF-8:    EF BB BF（U+FEFF の UTF-8 表現）
//   CESU-8:   EF BB BF（UTF-8 と同一）
//   非 Unicode エンコーディング（SJIS/JIS/EUC/Latin-1）: BOM なし
// =========================================================================

/**
 * @brief GetBom テストのパラメーター
 *
 * @param eCodeType  対象の文字コードセット種別
 * @param optExpected 期待される BOM バイト列。BOM なしの場合は std::nullopt
 */
using GetBomTestParam = std::tuple<ECodeType, std::optional<std::string>>;

//! GetBom テストのためのフィクスチャクラス
class GetBomTest : public ::testing::TestWithParam<GetBomTestParam> {};

/**
 * @brief GetBom のテスト
 *
 * CCodeBase::GetBom() が返す BOM を CMemory 経由で取得して検証する。
 * BOM なしの場合は GetRawPtr() が nullptr であることを確認する。
 */
TEST_P(GetBomTest, test) {
	const auto  eCodeType   = std::get<0>(GetParam());
	const auto& optExpected = std::get<1>(GetParam());

	const std::unique_ptr<CCodeBase> pCodeBase = CCodeFactory::CreateCodeBase(eCodeType);

	CMemory m;
	pCodeBase->GetBom(&m);

	if (optExpected.has_value()) {
		const std::string_view bom{ LPCSTR(m.GetRawPtr()), size_t(m.GetRawLength()) };
		EXPECT_THAT(bom, StrEq(*optExpected));
	} else {
		EXPECT_THAT(LPCSTR(m.GetRawPtr()), IsNull());
	}
}

/**
 * @brief GetBom パラメーター網羅テストケース
 *
 * SJIS/JIS/EUC/Latin-1 は非 Unicode なので BOM を持たない（{} = nullopt）。
 * UTF-7 は技術的に BOM を持てるが実用上意味がないため、テスト対象外とも言える。
 * UTF-32LE の BOM は UTF-16LE BOM（FF FE）に 2 バイトの NUL を続けた形になる
 * ため、4 バイトを std::string 直接構築で渡す（文字列リテラルは NUL 終端の問題がある）。
 */
INSTANTIATE_TEST_SUITE_P(GetBomCases
	, GetBomTest
	, ::testing::Values(
		GetBomTestParam{ CODE_SJIS,		{} },			// 非 Unicode: BOM なし
		GetBomTestParam{ CODE_JIS,		{} },			// 非 Unicode: BOM なし
		GetBomTestParam{ CODE_EUC,		{} },			// 非 Unicode: BOM なし
		GetBomTestParam{ CODE_LATIN1,	{} },			// 非 Unicode: BOM なし
		GetBomTestParam{ CODE_UTF7,		"+/v8-" },		// U+FEFF を UTF-7 エンコードしたもの
		GetBomTestParam{ CODE_UTF16LE,	"\xFF\xFE" },	// LE BOM（2 バイト）
		GetBomTestParam{ CODE_UTF16BE,	"\xFE\xFF" },	// BE BOM（2 バイト）
		// UTF-32LE: FF FE 00 00（NUL バイト含むので std::string 直接構築）
		GetBomTestParam{ CODE_UTF32LE,	std::string{ "\xFF\xFE\0\0", 4 } },
		// UTF-32BE: 00 00 FE FF
		GetBomTestParam{ CODE_UTF32BE,	std::string{ "\0\0\xFE\xFF", 4 } },
		GetBomTestParam{ CODE_UTF8,		"\xEF\xBB\xBF" },	// U+FEFF の UTF-8 表現（3 バイト）
		GetBomTestParam{ CODE_CESU8,	"\xEF\xBB\xBF" }	// UTF-8 と同一 BOM
	)
);

// =========================================================================
// 改行コード取得テスト
//
// テキストエディタでは複数種類の改行コードを扱う必要がある。
// GetEol() は指定したエンコーディングで指定した改行種別を表すバイト列を返す。
//
// EEolType と各バイト表現:
//   none:                なし（{} = nullopt）
//   cr_and_lf:           CR+LF (Windows 標準)
//   line_feed:           LF のみ (Unix/Linux)
//   carriage_return:     CR のみ (旧 Mac OS)
//   next_line:           NEL U+0085（Unicode 拡張改行）
//   line_separator:      LS  U+2028（Unicode 段落内改行）
//   paragraph_separator: PS  U+2029（Unicode 段落区切り）
//
// Unicode 系エンコーディング（UTF-7/16/32/8/CESU-8）では
// next_line / line_separator / paragraph_separator を表現できる。
// 非 Unicode エンコーディング（SJIS/JIS/EUC/Latin-1）はこれらを返さない。
//
// UTF-16/32 の改行バイト列はエンディアンによって異なる点に注意。
// =========================================================================

/**
 * @brief GetEol テストのパラメーター
 *
 * @param eCodeType  対象の文字コードセット種別
 * @param eEolType   取得したい改行種別
 * @param optExpected 期待される改行バイト列。未対応の場合は std::nullopt
 */
using GetEolTestParam = std::tuple<ECodeType, EEolType, std::optional<std::string>>;

//! GetEol テストのためのフィクスチャクラス
class GetEolTest : public ::testing::TestWithParam<GetEolTestParam> {};

/**
 * @brief GetEol のテスト
 *
 * CCodeBase::GetEol() が返す改行バイト列を CMemory 経由で取得して検証する。
 * 未対応の改行種別は GetRawPtr() が nullptr を返す。
 */
TEST_P(GetEolTest, test)
{
	const auto  eCodeType   = std::get<0>(GetParam());
	const auto  eEolType    = std::get<1>(GetParam());
	const auto& optExpected = std::get<2>(GetParam());

	const std::unique_ptr<CCodeBase> pCodeBase = CCodeFactory::CreateCodeBase(eCodeType);

	CMemory m;
	pCodeBase->GetEol(&m, eEolType);

	if (optExpected.has_value()) {
		const std::string_view eol{ LPCSTR(m.GetRawPtr()), size_t(m.GetRawLength()) };
		EXPECT_THAT(eol, StrEq(*optExpected));
	} else {
		EXPECT_THAT(LPCSTR(m.GetRawPtr()), IsNull());
	}
}

/**
 * @brief GetEol パラメーター網羅テストケース
 *
 * 全 11 コードタイプ × 7 改行種別 = 77 パラメーターをすべて網羅する。
 *
 * UTF-7 の next_line/line_separator/paragraph_separator は
 * Set B 文字（非 ASCII）なので Base64 エンコードされた形式で返る。
 * （"+AIU-" = U+0085 の UTF-7 表現）
 *
 * UTF-16LE/BE の改行は各文字の 2 バイト表現。NUL バイトを含む場合は
 * std::string_view を使って長さを明示する。
 *
 * UTF-32LE/BE の改行は各文字の 4 バイト表現。CR+LF は 8 バイトになる。
 */
INSTANTIATE_TEST_SUITE_P(GetEolCases
	, GetEolTest
	, ::testing::Values(
		// ── SJIS: Unicode 拡張改行（NEL/LS/PS）は表現不可
		GetEolTestParam{ CODE_SJIS,    EEolType::none,                {}     },
		GetEolTestParam{ CODE_SJIS,    EEolType::cr_and_lf,           "\r\n" },
		GetEolTestParam{ CODE_SJIS,    EEolType::line_feed,           "\n"   },
		GetEolTestParam{ CODE_SJIS,    EEolType::carriage_return,     "\r"   },
		GetEolTestParam{ CODE_SJIS,    EEolType::next_line,           {}     },	// U+0085 は Shift-JIS で表現不可
		GetEolTestParam{ CODE_SJIS,    EEolType::line_separator,      {}     },	// U+2028 は Shift-JIS で表現不可
		GetEolTestParam{ CODE_SJIS,    EEolType::paragraph_separator, {}     },	// U+2029 は Shift-JIS で表現不可

		// ── JIS: 同様に Unicode 拡張改行は不可
		GetEolTestParam{ CODE_JIS,     EEolType::none,                {}     },
		GetEolTestParam{ CODE_JIS,     EEolType::cr_and_lf,           "\r\n" },
		GetEolTestParam{ CODE_JIS,     EEolType::line_feed,           "\n"   },
		GetEolTestParam{ CODE_JIS,     EEolType::carriage_return,     "\r"   },
		GetEolTestParam{ CODE_JIS,     EEolType::next_line,           {}     },
		GetEolTestParam{ CODE_JIS,     EEolType::line_separator,      {}     },
		GetEolTestParam{ CODE_JIS,     EEolType::paragraph_separator, {}     },

		// ── EUC-JP: 同様
		GetEolTestParam{ CODE_EUC,     EEolType::none,                {}     },
		GetEolTestParam{ CODE_EUC,     EEolType::cr_and_lf,           "\r\n" },
		GetEolTestParam{ CODE_EUC,     EEolType::line_feed,           "\n"   },
		GetEolTestParam{ CODE_EUC,     EEolType::carriage_return,     "\r"   },
		GetEolTestParam{ CODE_EUC,     EEolType::next_line,           {}     },
		GetEolTestParam{ CODE_EUC,     EEolType::line_separator,      {}     },
		GetEolTestParam{ CODE_EUC,     EEolType::paragraph_separator, {}     },

		// ── Latin-1: 同様
		GetEolTestParam{ CODE_LATIN1,  EEolType::none,                {}     },
		GetEolTestParam{ CODE_LATIN1,  EEolType::cr_and_lf,           "\r\n" },
		GetEolTestParam{ CODE_LATIN1,  EEolType::line_feed,           "\n"   },
		GetEolTestParam{ CODE_LATIN1,  EEolType::carriage_return,     "\r"   },
		GetEolTestParam{ CODE_LATIN1,  EEolType::next_line,           {}     },
		GetEolTestParam{ CODE_LATIN1,  EEolType::line_separator,      {}     },
		GetEolTestParam{ CODE_LATIN1,  EEolType::paragraph_separator, {}     },

		// ── UTF-7: Unicode 拡張改行は Base64 エンコードされた形式で返る
		// "+AIU-" = U+0085 (NEL) の UTF-7 表現
		// "+ICg-" = U+2028 (LS)  の UTF-7 表現
		// "+ICk-" = U+2029 (PS)  の UTF-7 表現
		GetEolTestParam{ CODE_UTF7,    EEolType::none,                {}      },
		GetEolTestParam{ CODE_UTF7,    EEolType::cr_and_lf,           "\r\n"  },
		GetEolTestParam{ CODE_UTF7,    EEolType::line_feed,           "\n"    },
		GetEolTestParam{ CODE_UTF7,    EEolType::carriage_return,     "\r"    },
		GetEolTestParam{ CODE_UTF7,    EEolType::next_line,           "+AIU-" },	// Set B 文字なので Base64
		GetEolTestParam{ CODE_UTF7,    EEolType::line_separator,      "+ICg-" },	// Set B 文字なので Base64
		GetEolTestParam{ CODE_UTF7,    EEolType::paragraph_separator, "+ICk-" },	// Set B 文字なので Base64

		// ── UTF-16LE: 各改行は 2 バイト（NUL 含むので string_view で長さ明示）
		// CR+LF は 4 バイト: 0D 00 0A 00
		GetEolTestParam{ CODE_UTF16LE, EEolType::none,                {}                                },
		GetEolTestParam{ CODE_UTF16LE, EEolType::cr_and_lf,           std::string_view{ "\r\0\n\0", 4 } },
		GetEolTestParam{ CODE_UTF16LE, EEolType::line_feed,           std::string_view{ "\n\0", 2 }     },
		GetEolTestParam{ CODE_UTF16LE, EEolType::carriage_return,     std::string_view{ "\r\0", 2 }     },
		GetEolTestParam{ CODE_UTF16LE, EEolType::next_line,           std::string_view{ "\x85\0", 2 }   },	// U+0085 LE
		GetEolTestParam{ CODE_UTF16LE, EEolType::line_separator,      std::string_view{ "\x28\x20", 2 } },	// U+2028 LE
		GetEolTestParam{ CODE_UTF16LE, EEolType::paragraph_separator, std::string_view{ "\x29\x20", 2 } },	// U+2029 LE

		// ── UTF-16BE: バイト順が逆（上位バイト先行）
		GetEolTestParam{ CODE_UTF16BE, EEolType::none,                {}                                },
		GetEolTestParam{ CODE_UTF16BE, EEolType::cr_and_lf,           std::string_view{ "\0\r\0\n", 4 } },
		GetEolTestParam{ CODE_UTF16BE, EEolType::line_feed,           std::string_view{ "\0\n", 2 }     },
		GetEolTestParam{ CODE_UTF16BE, EEolType::carriage_return,     std::string_view{ "\0\r", 2 }     },
		GetEolTestParam{ CODE_UTF16BE, EEolType::next_line,           std::string_view{ "\0\x85", 2 }   },	// U+0085 BE
		GetEolTestParam{ CODE_UTF16BE, EEolType::line_separator,      std::string_view{ "\x20\x28", 2 } },	// U+2028 BE
		GetEolTestParam{ CODE_UTF16BE, EEolType::paragraph_separator, std::string_view{ "\x20\x29", 2 } },	// U+2029 BE

		// ── UTF-32LE: 各改行は 4 バイト、CR+LF は 8 バイト
		GetEolTestParam{ CODE_UTF32LE, EEolType::none,                {}                                        },
		GetEolTestParam{ CODE_UTF32LE, EEolType::cr_and_lf,           std::string_view{ "\r\0\0\0\n\0\0\0", 8 } },
		GetEolTestParam{ CODE_UTF32LE, EEolType::line_feed,           std::string_view{ "\n\0\0\0", 4 }         },
		GetEolTestParam{ CODE_UTF32LE, EEolType::carriage_return,     std::string_view{ "\r\0\0\0", 4 }         },
		GetEolTestParam{ CODE_UTF32LE, EEolType::next_line,           std::string_view{ "\x85\0\0\0", 4 }       },
		GetEolTestParam{ CODE_UTF32LE, EEolType::line_separator,      std::string_view{ "\x28\x20\0\0", 4 }     },
		GetEolTestParam{ CODE_UTF32LE, EEolType::paragraph_separator, std::string_view{ "\x29\x20\0\0", 4 }     },

		// ── UTF-32BE: 最上位バイト先行
		GetEolTestParam{ CODE_UTF32BE, EEolType::none,                {}                                        },
		GetEolTestParam{ CODE_UTF32BE, EEolType::cr_and_lf,           std::string_view{ "\0\0\0\r\0\0\0\n", 8 } },
		GetEolTestParam{ CODE_UTF32BE, EEolType::line_feed,           std::string_view{ "\0\0\0\n", 4 }         },
		GetEolTestParam{ CODE_UTF32BE, EEolType::carriage_return,     std::string_view{ "\0\0\0\r", 4 }         },
		GetEolTestParam{ CODE_UTF32BE, EEolType::next_line,           std::string_view{ "\0\0\0\x85", 4 }       },
		GetEolTestParam{ CODE_UTF32BE, EEolType::line_separator,      std::string_view{ "\0\0\x20\x28", 4 }     },
		GetEolTestParam{ CODE_UTF32BE, EEolType::paragraph_separator, std::string_view{ "\0\0\x20\x29", 4 }     },

		// ── UTF-8: Unicode 拡張改行を UTF-8 バイト列で返す
		// U+0085 (NEL) = C2 85（2 バイト）
		// U+2028 (LS)  = E2 80 A8（3 バイト）
		// U+2029 (PS)  = E2 80 A9（3 バイト）
		GetEolTestParam{ CODE_UTF8,    EEolType::none,                {}             },
		GetEolTestParam{ CODE_UTF8,    EEolType::cr_and_lf,           "\r\n"         },
		GetEolTestParam{ CODE_UTF8,    EEolType::line_feed,           "\n"           },
		GetEolTestParam{ CODE_UTF8,    EEolType::carriage_return,     "\r"           },
		GetEolTestParam{ CODE_UTF8,    EEolType::next_line,           "\xC2\x85"     },
		GetEolTestParam{ CODE_UTF8,    EEolType::line_separator,      "\xE2\x80\xA8" },
		GetEolTestParam{ CODE_UTF8,    EEolType::paragraph_separator, "\xE2\x80\xA9" },

		// ── CESU-8: BMP 内の改行文字なので UTF-8 と同一バイト列
		GetEolTestParam{ CODE_CESU8,   EEolType::none,                {}             },
		GetEolTestParam{ CODE_CESU8,   EEolType::cr_and_lf,           "\r\n"         },
		GetEolTestParam{ CODE_CESU8,   EEolType::line_feed,           "\n"           },
		GetEolTestParam{ CODE_CESU8,   EEolType::carriage_return,     "\r"           },
		GetEolTestParam{ CODE_CESU8,   EEolType::next_line,           "\xC2\x85"     },
		GetEolTestParam{ CODE_CESU8,   EEolType::line_separator,      "\xE2\x80\xA8" },
		GetEolTestParam{ CODE_CESU8,   EEolType::paragraph_separator, "\xE2\x80\xA9" }
	)
);

} // namespace convert

namespace window {

// =========================================================================
// UnicodeToHex テスト（CMainStatusBar::UnicodeToHex 回帰テスト）
//
// CMainStatusBar::UnicodeToHex() はステータスバーに表示する
// "カーソル位置文字のコードポイント / エンコード値" を生成する関数。
//
// 動作概要:
//   1. 指定された文字コードタイプでキャレット位置の文字を変換
//   2. 変換結果を16進数文字列として返す
//   3. 変換できない場合は "U+XXXX" 形式のコードポイントで代替表示
//   4. CommonSetting_Statusbar の各フラグが ON なら、常に "U+XXXX" 形式を返す
//
// テスト設計について:
//   既存テストでは ESettingType::Default（全フラグ OFF）と
//   ESettingType::DispCodepoint（全フラグ一括 ON）の二択しかないため、
//   各フラグの独立した動作確認には別途 MakeStatusbar() ベースのテストが必要。
//   （→ namespace window 末尾の追加ブロック参照）
//
// 既知のバグ（コメント 👈バグ で注記）:
//   - 空文字列入力時に終端 NUL にアクセスしてしまう（バッファオーバーリード）
//   - サロゲートペア文字を SJIS/JIS/EUC で変換しようとしてコードポイント表示にならない
//   - 結合文字（半濁点など）が捨てられる
//   - IVS の基底文字がコードポイント表示されない
//   - Latin-1 が m_bDispUniInSjis フラグを流用している
// =========================================================================

//! 表示設定種別（全フラグ OFF / 全フラグ ON の二択）
enum class ESettingType : int8_t {
	Default,		//!< 全フラグ OFF: エンコーディング固有の16進値を表示
	DispCodepoint	//!< 全フラグ ON:  常に "U+XXXX" コードポイント形式で表示
};

//! Google Test に ESettingType を出力させるためのカスタム PrintTo
void PrintTo(ESettingType eSettingType, std::ostream* os)
{
	switch (eSettingType) {
	case ESettingType::Default:       *os << "Default"; break;
	case ESettingType::DispCodepoint: *os << "DispCodepoint"; break;
	default:
		*os << std::format("ESettingType({})", static_cast<uint16_t>(eSettingType));
		break;
	}
}

/**
 * @brief UnicodeToHex テストのパラメーター
 *
 * @param eSettingType  表示設定（全 OFF / 全 ON）
 * @param eCodeType     文字コードセット種別
 * @param caretChars    キャレット位置の文字列
 * @param expected      期待される16進表示文字列
 */
using UnicoceToHexTestParam = std::tuple<ESettingType, ECodeType, std::wstring, std::wstring>;

/**
 * @brief UnicodeToHex テストフィクスチャ
 *
 * sStatusbar0: 全フラグ OFF（ESettingType::Default に対応）
 * sStatusbar1: 全フラグ ON（ESettingType::DispCodepoint に対応）
 */
struct UnicodeToHexTest : public ::testing::TestWithParam<UnicoceToHexTestParam> {
	// 全フラグ OFF: エンコーディング固有の16進値を返す
	const CommonSetting_Statusbar sStatusbar0{
		false,	// m_bDispUniInSjis
		false,	// m_bDispUniInJis
		false,	// m_bDispUniInEuc
		false,	// m_bDispUtf8Codepoint
		false	// m_bDispSPCodepoint
	};

	// 全フラグ ON: 常に "U+XXXX" コードポイント形式を返す
	const CommonSetting_Statusbar sStatusbar1{
		true,	// m_bDispUniInSjis
		true,	// m_bDispUniInJis
		true,	// m_bDispUniInEuc
		true,	// m_bDispUtf8Codepoint
		true	// m_bDispSPCodepoint
	};
};

TEST_P(UnicodeToHexTest, test)
{
	const auto  eSettingType = std::get<0>(GetParam());
	const auto  eCodeType    = std::get<1>(GetParam());
	const auto& caretChars   = std::get<2>(GetParam());
	const auto& expected     = std::get<3>(GetParam());

	CommonSetting_Statusbar sStatusbar{ eSettingType == ESettingType::Default ? sStatusbar0 : sStatusbar1 };

	EXPECT_THAT(CMainStatusBar::UnicodeToHex(eCodeType, caretChars, sStatusbar), StrEq(expected));
}

INSTANTIATE_TEST_SUITE_P(UnicodeToHexCases
	, UnicodeToHexTest
	, ::testing::Values(
		// ────────────────────────────────────────────────────────────────
		// ASCII 文字「J」(U+004A) — デフォルト設定
		//
		// SJIS/JIS/EUC/UTF-8/CESU-8 は ASCII コードポイント値（4A）を返す。
		// UTF-16/UTF-32/UTF-7 は常に "U+XXXX" 形式で返す（Unicode 系）。
		// Latin-1 だけ小文字 "4a" を返す（実装上の仕様と思われる）。
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"J", L"4A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"J", L"4A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"J", L"4A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16LE, L"J", L"U+004A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16BE, L"J", L"U+004A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32LE, L"J", L"U+004A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32BE, L"J", L"U+004A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF8,    L"J", L"4A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF7,    L"J", L"U+004A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_CESU8,   L"J", L"4A" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_LATIN1,  L"J", L"4a" },	// Latin-1 は英字が小文字

		// ────────────────────────────────────────────────────────────────
		// 空文字列 L"" — バッファオーバーリードバグの記録
		//
		// 空文字列を渡すと wstring.size() == 0 だが実装が data()[0]（終端 NUL）に
		// アクセスしてしまう。結果として "00" や "U+0000" が返るが、
		// 本来はアクセスしてはならない。このテストはバグを文書化するものであり、
		// バグ修正後は期待値を変更する必要がある。
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"", L"00" },		// 👈バグ: 終端 NUL アクセス
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"", L"00" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"", L"00" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16LE, L"", L"U+0000" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16BE, L"", L"U+0000" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32LE, L"", L"U+0000" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32BE, L"", L"U+0000" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF8,    L"", L"00" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF7,    L"", L"U+0000" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_CESU8,   L"", L"00" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_LATIN1,  L"", L"00" },		// 👈バグ

		// ────────────────────────────────────────────────────────────────
		// コードページ非サポート文字「鷗」(U+9DD7) — フォールバック挙動
		//
		// SJIS（CP932）/JIS（CP932 ベース）/EUC-JP のいずれも
		// 「鷗」(JIS 第4水準) を持たないため変換失敗。
		// フォールバックとして "U+9DD7" が返る。
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"鷗", L"U+9DD7" },	// CP932 変換不可
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"鷗", L"U+9DD7" },	// JIS X 0212 非対応
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"鷗", L"U+9DD7" },	// EUC 補助漢字非対応

		// ────────────────────────────────────────────────────────────────
		// エラーバイナリ（Lonely Surrogate \xDCEF）
		//
		// LoadFromCode が変換できないバイトを DC00+バイト値 のサロゲートとして
		// 格納した場合の表示を確認する。
		// SJIS/EUC/UTF-8/CESU-8 では "?EF"（バイト値部分のみ16進表示）。
		// Unicode 系では "U+DCEF"（サロゲート代替値をそのままコードポイント表示）。
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"\xDCEF", L"?EF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"\xDCEF", L"U+DCEF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"\xDCEF", L"?EF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16LE, L"\xDCEF", L"U+DCEF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16BE, L"\xDCEF", L"U+DCEF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32LE, L"\xDCEF", L"U+DCEF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32BE, L"\xDCEF", L"U+DCEF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF8,    L"\xDCEF", L"?EF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF7,    L"\xDCEF", L"U+DCEF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_CESU8,   L"\xDCEF", L"?EF" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_LATIN1,  L"\xDCEF", L"?ef" },	// Latin-1 は英字小文字

		// ────────────────────────────────────────────────────────────────
		// ひらがな「あ」(U+3042) — 各コードタイプの固有 Hex 確認
		//
		// SJIS=82A0, JIS=2422（区点 04-02）, EUC=A4A2
		// UTF-8=E38182（3 バイト）, UTF-7/Unicode 系=U+3042
		// Latin-1 は変換不可なので "U+3042"
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"あ", L"82A0" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"あ", L"2422" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"あ", L"A4A2" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16LE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16BE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32LE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32BE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF8,    L"あ", L"E38182" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF7,    L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_CESU8,   L"あ", L"E38182" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_LATIN1,  L"あ", L"U+3042" },	// 変換不可

		// ────────────────────────────────────────────────────────────────
		// ひらがな「あ」— コードポイント表示モード（全フラグ ON）
		//
		// すべてのコードタイプで統一して "U+3042" が返ることを確認。
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_SJIS,    L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_JIS,     L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_EUC,     L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16LE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16BE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32LE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32BE, L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF8,    L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF7,    L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_CESU8,   L"あ", L"U+3042" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_LATIN1,  L"あ", L"U+3042" },

		// ────────────────────────────────────────────────────────────────
		// サロゲートペア: 絵文字「🚹」(U+1F6B9: Men's Room) = D83D DEB9
		//
		// SJIS/JIS/EUC は BMP 外文字を表現できないため、変換失敗。
		// 本来なら "U+1F6B9" と表示すべきだが、現実装は UTF-16 のサロゲート
		// ペアバイト列 "D83DDEB9" をそのまま返す（バグ）。
		//
		// UTF-8 は正しく 4 バイト変換: F0 9F 9A B9 → "F09F9AB9"
		// CESU-8 はサロゲートペアを個別に 3 バイトずつ: EDA0BD EDBAB9 = 6 バイト
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"\U0001F6B9", L"D83DDEB9" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"\U0001F6B9", L"D83DDEB9" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"\U0001F6B9", L"D83DDEB9" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16LE, L"\U0001F6B9", L"D83DDEB9" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16BE, L"\U0001F6B9", L"D83DDEB9" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32LE, L"\U0001F6B9", L"D83DDEB9" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32BE, L"\U0001F6B9", L"D83DDEB9" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF8,    L"\U0001F6B9", L"F09F9AB9" },		// 4 バイト正常変換
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF7,    L"\U0001F6B9", L"D83DDEB9" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_CESU8,   L"\U0001F6B9", L"EDA0BDEDBAB9" },	// 6 バイト（サロゲート個別変換）
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_LATIN1,  L"\U0001F6B9", L"D83DDEB9" },		// 👈バグ

		// コードポイント表示モード: すべて "U+1F6B9"
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_SJIS,    L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_JIS,     L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_EUC,     L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16LE, L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16BE, L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32LE, L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32BE, L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF8,    L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF7,    L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_CESU8,   L"\U0001F6B9", L"U+1F6B9" },
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_LATIN1,  L"\U0001F6B9", L"U+1F6B9" },

		// ────────────────────────────────────────────────────────────────
		// 結合文字「ぽ」= ひらがな「ほ」(U+307B) + 結合文字半濁点 (U+309A)
		//
		// Unicode の結合文字（Combining Character）は視覚的には 1 文字だが、
		// コードポイント的には 2 文字（基底文字 + 結合文字）。
		// 現実装は 1 コードユニットしか処理せず結合文字を捨てる（バグ）。
		// SJIS/JIS/EUC は結合文字を使わず合成済み文字「ぽ」を直接表現するが、
		// 実装は結合文字を認識せずに「ほ」だけ変換してしまう（バグ）。
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"ぽ", L"82D9" },		// 👈バグ: 結合文字を無視
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"ぽ", L"245B" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"ぽ", L"A4DB" },		// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16LE, L"ぽ", L"U+307B" },	// 👈バグ: 結合文字が捨てられる
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16BE, L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32LE, L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32BE, L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF8,    L"ぽ", L"E381BB" },	// 👈バグ: 結合文字が捨てられる
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF7,    L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_CESU8,   L"ぽ", L"E381BB" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_LATIN1,  L"ぽ", L"U+307B" },	// 👈バグ

		// コードポイント表示モード: 結合文字が捨てられる問題は変わらない
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_SJIS,    L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_JIS,     L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_EUC,     L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16LE, L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16BE, L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32LE, L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32BE, L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF8,    L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF7,    L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_CESU8,   L"ぽ", L"U+307B" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_LATIN1,  L"ぽ", L"U+307B" },	// 👈バグ

		// ────────────────────────────────────────────────────────────────
		// IVS（Ideographic Variation Sequence）:
		// 「葛」(U+845B) + 異体字セレクタ U+E0100（DB40 DD00）
		//
		// IVS は JIS X 0213 にも JIS X 0212 にも存在しない異体字表現。
		// 例: 葛城市の「葛」（下がヒ）vs 葛飾区の「葛」（下がヒ以外）
		//
		// SJIS/JIS/EUC では IVS セレクタを表現できないため、
		// 基底文字だけが見えてしまう（バグ: 異体字セレクタが無視される）。
		// UTF-16 では "845B, DB40DD00" のようにカンマ区切りで
		// 3 コードユニット分が連結して表示される。
		// コードポイント表示モードでも基底文字が "U+845B" でなく
		// そのままコードポイント未表示になる（バグ）。
		// ────────────────────────────────────────────────────────────────
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_SJIS,    L"\x845B\U000E0100", L"8A8B" },			// 👈バグ: IVS セレクタ無視
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_JIS,     L"\x845B\U000E0100", L"336B" },			// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_EUC,     L"\x845B\U000E0100", L"B3EB" },			// 👈バグ
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16LE, L"\x845B\U000E0100", L"845B, DB40DD00" },	// 3 コードユニット
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF16BE, L"\x845B\U000E0100", L"845B, DB40DD00" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32LE, L"\x845B\U000E0100", L"845B, DB40DD00" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF32BE, L"\x845B\U000E0100", L"845B, DB40DD00" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF8,    L"\x845B\U000E0100", L"E8919BF3A08480" },	// 7 バイト
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_UTF7,    L"\x845B\U000E0100", L"845B, DB40DD00" },
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_CESU8,   L"\x845B\U000E0100", L"E8919BEDAD80EDB480" },	// 9 バイト
		UnicoceToHexTestParam{ ESettingType::Default,       CODE_LATIN1,  L"\x845B\U000E0100", L"845B, DB40DD00" },

		// コードポイント表示モード: 基底文字が "U+845B" でなく "845B" のまま（バグ）
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_SJIS,    L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_JIS,     L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_EUC,     L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16LE, L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF16BE, L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32LE, L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF32BE, L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF8,    L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_UTF7,    L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_CESU8,   L"\x845B\U000E0100", L"845B, U+E0100" },	// 👈バグ
		UnicoceToHexTestParam{ ESettingType::DispCodepoint, CODE_LATIN1,  L"\x845B\U000E0100", L"845B, U+E0100" }	// 👈バグ
	)
);

// =========================================================================
// 以下のブロックは参照ファイルから抽出・追記された追加テストケースです
// 参照コミット: 275a8767ea9577bc60391ab13f5ec1dca17daa05
//
// 既存の UnicodeToHexTest（ESettingType による全フラグ一括 ON/OFF）では
// 検証できない「個別フラグ制御」「CODE_UNICODE / CODE_UNICODEBE」
// 「サロゲートペア U+20000」「IVS '森'+E0100」を網羅する。
// =========================================================================

/**
 * @brief ステータスバー設定を生成するヘルパー関数
 *
 * 引数で各フラグを個別に ON/OFF できる。
 * 既存テストの ESettingType 方式は「全フラグ一括 ON」か「全フラグ一括 OFF」
 * の二択しかなく、"特定フラグだけ ON にしたとき挙動がどう変わるか" を
 * 個別に保証できない。このヘルパーはその欠点を補う。
 *
 * Windows の BOOL は int 型だが、true/false から明示的にキャストすることで
 * 値の意図を読み手に伝える。
 */
static CommonSetting_Statusbar MakeStatusbar(
	bool dispUniInSjis = false, bool dispUniInJis  = false,
	bool dispUniInEuc  = false, bool dispUtf8Cp   = false,
	bool dispSPCp      = false
)
{
	CommonSetting_Statusbar s{};
	s.m_bDispUniInSjis     = dispUniInSjis ? TRUE : FALSE;
	s.m_bDispUniInJis      = dispUniInJis  ? TRUE : FALSE;
	s.m_bDispUniInEuc      = dispUniInEuc  ? TRUE : FALSE;
	s.m_bDispUtf8Codepoint = dispUtf8Cp   ? TRUE : FALSE;
	s.m_bDispSPCodepoint   = dispSPCp     ? TRUE : FALSE;
	return s;
}

/**
 * @brief UnicodeToHex 追加テスト用パラメーター型
 *
 * CommonSetting_Statusbar を直接埋め込むことで、テストデータ側で
 * 各フラグの ON/OFF を自由に組み合わせられる。
 * 既存の UnicoceToHexTestParam（全 ON / 全 OFF の二択）との違いに注意。
 * ※既存型名の "Unicoce" はタイポだが、別名として共存させる。
 */
using UnicodeToHexTestParamEx = std::tuple<ECodeType, CommonSetting_Statusbar, std::wstring, std::wstring>;

/**
 * @brief UnicodeToHex 追加テストフィクスチャ
 *
 * 既存の UnicodeToHexTest（ESettingType ベース）と同じ対象関数
 * CMainStatusBar::UnicodeToHex を、より細粒度なパラメーター制御で再検証する。
 * Google Test の名前衝突を避けるため UnicodeToHexTestEx とした。
 */
struct UnicodeToHexTestEx : public ::testing::TestWithParam<UnicodeToHexTestParamEx> {};

TEST_P(UnicodeToHexTestEx, DoConvert)
{
	const auto  eCodeType  = std::get<0>(GetParam());
	const auto& sStatusbar = std::get<1>(GetParam());
	const auto& wide       = std::get<2>(GetParam());
	const auto& expected   = std::get<3>(GetParam());
	EXPECT_EQ(expected, CMainStatusBar::UnicodeToHex(eCodeType, wide, sStatusbar));
}

INSTANTIATE_TEST_SUITE_P(
	UnicodeToHexCasesEx, UnicodeToHexTestEx,
	::testing::Values(

		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		// [1] ASCII 'A' — 11 種の文字コードタイプでデフォルト設定を検証
		//
		// 全フラグ OFF 時、SJIS/JIS/EUC/UTF-8/CESU-8/Latin-1 はコードページ上の
		// 16 進値をそのまま返し、Unicode 系コードタイプ（UTF-7 / UTF-16 / UTF-32）
		// は常に "U+XXXX" 形式で返すことを確認する。
		// CODE_UNICODE（= UTF-16LE の別名）と CODE_UNICODEBE（= UTF-16BE の別名）は
		// 既存テストで CODE_UTF16LE/BE として検証済みだが、
		// エイリアスとして独立した実装パスを持つ可能性があるため別途検証する。
		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		UnicodeToHexTestParamEx{ CODE_SJIS,      MakeStatusbar(), L"A", L"41"      },
		UnicodeToHexTestParamEx{ CODE_JIS,       MakeStatusbar(), L"A", L"41"      },
		UnicodeToHexTestParamEx{ CODE_EUC,       MakeStatusbar(), L"A", L"41"      },
		UnicodeToHexTestParamEx{ CODE_UTF8,      MakeStatusbar(), L"A", L"41"      },
		UnicodeToHexTestParamEx{ CODE_CESU8,     MakeStatusbar(), L"A", L"41"      },
		UnicodeToHexTestParamEx{ CODE_LATIN1,    MakeStatusbar(), L"A", L"41"      },
		UnicodeToHexTestParamEx{ CODE_UTF7,      MakeStatusbar(), L"A", L"U+0041"  },
		UnicodeToHexTestParamEx{ CODE_UNICODE,   MakeStatusbar(), L"A", L"U+0041"  },	// UTF-16LE 別名
		UnicodeToHexTestParamEx{ CODE_UNICODEBE, MakeStatusbar(), L"A", L"U+0041"  },	// UTF-16BE 別名
		UnicodeToHexTestParamEx{ CODE_UTF32LE,   MakeStatusbar(), L"A", L"U+0041"  },
		UnicodeToHexTestParamEx{ CODE_UTF32BE,   MakeStatusbar(), L"A", L"U+0041"  },

		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		// [2] ASCII 'A' — 各コードタイプに対応したフラグを個別 ON にした場合
		//
		// 既存テストの ESettingType::DispCodepoint は全フラグを一括 ON するため、
		// 「どのフラグが実際に効いているか」を切り分けられない。
		// ここでは対応フラグ 1 つだけを ON にして各実装パスを個別に検証する。
		// Latin-1 には専用フラグが存在せず、実装が m_bDispUniInSjis を流用している
		// 設計上の問題がある（バグとして認識済み）。
		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		UnicodeToHexTestParamEx{ CODE_SJIS,   MakeStatusbar(true),                   L"A", L"U+0041" },	// m_bDispUniInSjis のみ ON
		UnicodeToHexTestParamEx{ CODE_JIS,    MakeStatusbar(false,true),             L"A", L"U+0041" },	// m_bDispUniInJis のみ ON
		UnicodeToHexTestParamEx{ CODE_EUC,    MakeStatusbar(false,false,true),       L"A", L"U+0041" },	// m_bDispUniInEuc のみ ON
		UnicodeToHexTestParamEx{ CODE_UTF8,   MakeStatusbar(false,false,false,true), L"A", L"U+0041" },	// m_bDispUtf8Codepoint のみ ON
		UnicodeToHexTestParamEx{ CODE_CESU8,  MakeStatusbar(false,false,false,true), L"A", L"U+0041" },	// m_bDispUtf8Codepoint のみ ON
		UnicodeToHexTestParamEx{ CODE_LATIN1, MakeStatusbar(true),                   L"A", L"U+0041" },	// 👈バグ: CLatin1 が m_bDispUniInSjis を流用

		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		// [3] ひらがな「あ」(U+3042) — 各コードタイプの固有 Hex を検証
		//
		// SJIS = 82A0（Shift-JIS 2 バイト）、JIS = 2422（JIS X 0208 区点 04-02）、
		// EUC = A4A2（EUC-JP 2 バイト）、UTF-8 = E38182（3 バイト）の各表現を確認。
		// Latin-1 は U+3042 を表現できないため UTF-16BE のバイト値そのまま
		// "U+3042" を返すことに注意（変換失敗時の Windows CP フォールバック挙動）。
		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		UnicodeToHexTestParamEx{ CODE_SJIS,   MakeStatusbar(), L"あ", L"82A0"   },
		UnicodeToHexTestParamEx{ CODE_JIS,    MakeStatusbar(), L"あ", L"2422"   },
		UnicodeToHexTestParamEx{ CODE_EUC,    MakeStatusbar(), L"あ", L"A4A2"   },
		UnicodeToHexTestParamEx{ CODE_UTF8,   MakeStatusbar(), L"あ", L"E38182" },
		UnicodeToHexTestParamEx{ CODE_CESU8,  MakeStatusbar(), L"あ", L"E38182" },
		UnicodeToHexTestParamEx{ CODE_LATIN1, MakeStatusbar(), L"あ", L"U+3042" },	// 変換不可 → フォールバック

		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		// [4] ひらがな「あ」— 対応フラグ個別 ON でコードポイント表示に切り替わることを検証
		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		UnicodeToHexTestParamEx{ CODE_SJIS,  MakeStatusbar(true),                   L"あ", L"U+3042" },
		UnicodeToHexTestParamEx{ CODE_JIS,   MakeStatusbar(false,true),             L"あ", L"U+3042" },
		UnicodeToHexTestParamEx{ CODE_EUC,   MakeStatusbar(false,false,true),       L"あ", L"U+3042" },
		UnicodeToHexTestParamEx{ CODE_UTF8,  MakeStatusbar(false,false,false,true), L"あ", L"U+3042" },

		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		// [5] サロゲートペア U+20000（CJK 拡張 B 先頭文字、D840 DC00）
		//
		// m_bDispSPCodepoint を ON にすると "U+20000" と表示されることを確認。
		// CODE_UNICODE でも同様に制御できることを確認する。
		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		UnicodeToHexTestParamEx{ CODE_SJIS,
			MakeStatusbar(),
			std::wstring{ L"\xD840\xDC00", 2 }, L"D840DC00" },
		UnicodeToHexTestParamEx{ CODE_SJIS,
			MakeStatusbar(false,false,false,false,true),
			std::wstring{ L"\xD840\xDC00", 2 }, L"U+20000"  },
		UnicodeToHexTestParamEx{ CODE_UTF8,
			MakeStatusbar(),
			std::wstring{ L"\xD840\xDC00", 2 }, L"F0A08080" },
		UnicodeToHexTestParamEx{ CODE_UTF8,
			MakeStatusbar(false,false,false,true,true),
			std::wstring{ L"\xD840\xDC00", 2 }, L"U+20000"  },
		UnicodeToHexTestParamEx{ CODE_UNICODE,
			MakeStatusbar(),
			std::wstring{ L"\xD840\xDC00", 2 }, L"D840DC00" },
		UnicodeToHexTestParamEx{ CODE_UNICODE,
			MakeStatusbar(false,false,false,false,true),
			std::wstring{ L"\xD840\xDC00", 2 }, L"U+20000"  },

		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		// [6] IVS: '森'(U+68EE) + 異体字セレクタ U+E0100（DB40 DD00）
		//
		// CODE_UNICODE のデフォルト: "68EE, DB40DD00"（3 コードユニット連結）
		// m_bDispSPCodepoint ON: "68EE, U+E0100"（セレクタをコードポイント表示）
		// UTF-8: 基底文字 E6A3AE(3B) + セレクタ F3A08480(4B) = 7 バイト
		// ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
		UnicodeToHexTestParamEx{ CODE_UNICODE,
			MakeStatusbar(),
			std::wstring{ L"森\xDB40\xDD00", 3 }, L"68EE, DB40DD00" },
		UnicodeToHexTestParamEx{ CODE_UNICODE,
			MakeStatusbar(false,false,false,false,true),
			std::wstring{ L"森\xDB40\xDD00", 3 }, L"68EE, U+E0100"  },
		UnicodeToHexTestParamEx{ CODE_UTF8,
			MakeStatusbar(),
			std::wstring{ L"森\xDB40\xDD00", 3 }, L"E6A3AEF3A08480" }
	)
);

} // namespace window

