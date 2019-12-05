// zlib open source license
//
// Copyright (c) 2018 to 2019 David Forsgren Piuva
// 
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 
//    2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 
//    3. This notice may not be removed or altered from any source
//    distribution.

#include "InputEvent.h"

using namespace dsr;

inline String dsr::getName(DsrKey v) {
	if (v == DsrKey_Unhandled) {
		return U"Other";
	} else if (v == DsrKey_Escape) {
		return U"Escape";
	} else if (v == DsrKey_F1) {
		return U"F1";
	} else if (v == DsrKey_F2) {
		return U"F2";
	} else if (v == DsrKey_F3) {
		return U"F3";
	} else if (v == DsrKey_F4) {
		return U"F4";
	} else if (v == DsrKey_F5) {
		return U"F5";
	} else if (v == DsrKey_F6) {
		return U"F6";
	} else if (v == DsrKey_F7) {
		return U"F7";
	} else if (v == DsrKey_F8) {
		return U"F8";
	} else if (v == DsrKey_F9) {
		return U"F9";
	} else if (v == DsrKey_F10) {
		return U"F10";
	} else if (v == DsrKey_F11) {
		return U"F11";
	} else if (v == DsrKey_F12) {
		return U"F12";
	} else if (v == DsrKey_Pause) {
		return U"Pause";
	} else if (v == DsrKey_Space) {
		return U"Space";
	} else if (v == DsrKey_Tab) {
		return U"Tab";
	} else if (v == DsrKey_Return) {
		return U"Return";
	} else if (v == DsrKey_BackSpace) {
		return U"BackSpace";
	} else if (v == DsrKey_LeftShift) {
		return U"LeftShift";
	} else if (v == DsrKey_RightShift) {
		return U"RightShift";
	} else if (v == DsrKey_LeftControl) {
		return U"LeftControl";
	} else if (v == DsrKey_RightControl) {
		return U"RightControl";
	} else if (v == DsrKey_LeftAlt) {
		return U"LeftAlt";
	} else if (v == DsrKey_RightAlt) {
		return U"RightAlt";
	} else if (v == DsrKey_Delete) {
		return U"Delete";
	} else if (v == DsrKey_LeftArrow) {
		return U"LeftArrow";
	} else if (v == DsrKey_RightArrow) {
		return U"RightArrow";
	} else if (v == DsrKey_UpArrow) {
		return U"UpArrow";
	} else if (v == DsrKey_DownArrow) {
		return U"DownArrow";
	} else if (v == DsrKey_0) {
		return U"0";
	} else if (v == DsrKey_1) {
		return U"1";
	} else if (v == DsrKey_2) {
		return U"2";
	} else if (v == DsrKey_3) {
		return U"3";
	} else if (v == DsrKey_4) {
		return U"4";
	} else if (v == DsrKey_5) {
		return U"5";
	} else if (v == DsrKey_6) {
		return U"6";
	} else if (v == DsrKey_7) {
		return U"7";
	} else if (v == DsrKey_8) {
		return U"8";
	} else if (v == DsrKey_9) {
		return U"9";
	} else if (v == DsrKey_A) {
		return U"A";
	} else if (v == DsrKey_B) {
		return U"B";
	} else if (v == DsrKey_C) {
		return U"C";
	} else if (v == DsrKey_D) {
		return U"D";
	} else if (v == DsrKey_E) {
		return U"E";
	} else if (v == DsrKey_F) {
		return U"F";
	} else if (v == DsrKey_G) {
		return U"G";
	} else if (v == DsrKey_H) {
		return U"H";
	} else if (v == DsrKey_I) {
		return U"I";
	} else if (v == DsrKey_J) {
		return U"J";
	} else if (v == DsrKey_K) {
		return U"K";
	} else if (v == DsrKey_L) {
		return U"L";
	} else if (v == DsrKey_M) {
		return U"M";
	} else if (v == DsrKey_N) {
		return U"N";
	} else if (v == DsrKey_O) {
		return U"O";
	} else if (v == DsrKey_P) {
		return U"P";
	} else if (v == DsrKey_Q) {
		return U"Q";
	} else if (v == DsrKey_R) {
		return U"R";
	} else if (v == DsrKey_S) {
		return U"S";
	} else if (v == DsrKey_T) {
		return U"T";
	} else if (v == DsrKey_U) {
		return U"U";
	} else if (v == DsrKey_V) {
		return U"V";
	} else if (v == DsrKey_W) {
		return U"W";
	} else if (v == DsrKey_X) {
		return U"X";
	} else if (v == DsrKey_Y) {
		return U"Y";
	} else if (v == DsrKey_Z) {
		return U"Z";
	} else {
		return U"Invalid virtual key code";
	}
}

String& dsr::string_toStreamIndented(String& target, const DsrKey& source, const ReadableString& indentation) {
	string_append(target, indentation, getName(source));
	return target;
}
