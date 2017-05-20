/*--------------------------------------------------------------------------
**
**  Copyright (c) 2003-2011, Tom Hunter
**
**  Name: console.cpp
**
**  Description:
**      CDC 6600 character set conversions.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License version 3 as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License version 3 for more details.
**
**  You should have received a copy of the GNU General Public License
**  version 3 along with this program in file "license-gpl-3.0.txt".
**  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
**
**--------------------------------------------------------------------------
*/

/*
**  -------------
**  Include Files
**  -------------
*/
#include "stdafx.h"

/*
**  --------------------------------------
**  Public character set conversion tables
**  --------------------------------------
*/
const u8 asciiToCdc[256] =
{
	/* 000- */  0,      0,      0,      0,      0,      0,      0,      0,
	/* 010- */  0,      0,      0,      0,      0,      0,      0,      0,
	/* 020- */  0,      0,      0,      0,      0,      0,      0,      0,
	/* 030- */  0,      0,      0,      0,      0,      0,      0,      0,
	/* 040- */  055,    066,    064,    060,    053,    063,    067,    070,
	/* 050- */  051,    052,    047,    045,    056,    046,    057,    050,
	/* 060- */  033,    034,    035,    036,    037,    040,    041,    042,
	/* 070- */  043,    044,    0,      077,    072,    054,    073,    071,
	/* 100- */  074,    001,    002,    003,    004,    005,    006,    007,
	/* 110- */  010,    011,    012,    013,    014,    015,    016,    017,
	/* 120- */  020,    021,    022,    023,    024,    025,    026,    027,
	/* 130- */  030,    031,    032,    061,    075,    062,    076,    065,
	/* 140- */  0,      001,    002,    003,    004,    005,    006,    007,
	/* 150- */  010,    011,    012,    013,    014,    015,    016,    017,
	/* 160- */  020,    021,    022,    023,    024,    025,    026,    027,
	/* 170- */  030,    031,    032,    061,    0,      0,      0,      0
};

const char cdcToAscii[64] =
{
	/* 00-07 */ ':',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
	/* 10-17 */ 'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
	/* 20-27 */ 'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
	/* 30-37 */ 'X',    'Y',    'Z',    '0',    '1',    '2',    '3',    '4',
	/* 40-47 */ '5',    '6',    '7',    '8',    '9',    '+',    '-',    '*',
	/* 50-57 */ '/',    '(',    ')',    '$',    '=',    ' ',    ',',    '.',
	/* 60-67 */ '#',    '[',    ']',    '%',    '"',	'_',	'!',	'&',
	/* 70-77 */ '\'',   '?',    '<',    '>',    '@',    '\\',   '^',    ';'
};

const u8 asciiToConsole[256] =
{
	/* 00-07 */ 0,      0,      0,      0,      0,      0,      0,      0,
	/* 08-0F */ 061,    0,      060,    0,      0,      060,    0,      0,
	/* 10-17 */ 0,      0,      0,      0,      0,      0,      0,      0,
	/* 18-1F */ 0,      0,      0,      0,      0,      0,      0,      0,
	/* 20-27 */ 062,    0,      0,      0,      0,      0,      0,      0,
	/* 28-2F */ 051,    052,    047,    045,    056,    046,    057,    050,
	/* 30-37 */ 033,    034,    035,    036,    037,    040,    041,    042,
	/* 38-3F */ 043,    044,    0,      0,      0,      054,    0,      0,
	/* 40-47 */ 0,      01,     02,     03,     04,     05,     06,     07,
	/* 48-4F */ 010,    011,    012,    013,    014,    015,    016,    017,
	/* 50-57 */ 020,    021,    022,    023,    024,    025,    026,    027,
	/* 58-5F */ 030,    031,    032,    053,    0,      055,    0,      0,
	/* 60-67 */ 0,      01,     02,     03,     04,     05,     06,     07,
	/* 68-6F */ 010,    011,    012,    013,    014,    015,    016,    017,
	/* 70-77 */ 020,    021,    022,    023,    024,    025,    026,    027,
	/* 78-7F */ 030,    031,    032,    0,      0,      0,      0,      0
};

const char consoleToAscii[64] =
{
	/* 00-07 */ 0,      'A',    'B',    'C',    'D',    'E',    'F',    'G',
	/* 10-17 */ 'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
	/* 20-27 */ 'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
	/* 30-37 */ 'X',    'Y',    'Z',    '0',    '1',    '2',    '3',    '4',
	/* 40-47 */ '5',    '6',    '7',    '8',    '9',    '+',    '-',    '*',
	/* 50-57 */ '/',    '(',    ')',    ' ',    '=',    ' ',    ',',    '.',
	/* 60-67 */  0,      0,      0,      0,      0,      0,      0,      0,
	/* 70-77 */  0,      0,      0,      0,      0,      0,      0,      0
};

const PpWord asciiTo026[256] =
{
	/*                                                                         */
	/* 000- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*                                                                         */
	/* 010- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*                                                                         */
	/* 020- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*                                                                         */
	/* 030- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*          space   !       "       #       $       %       &       '      */
	/* 040- */  0,      03000,  00042,  01012,  02102,  00012,  01006,  02022,
	/*          (       )       *       +       ,       -       .       /      */
	/* 050- */  01042,  04042,  02042,  04000,  01102,  02000,  04102,  01400,
	/*          0       1       2       3       4       5       6       7      */
	/* 060- */  01000,  00400,  00200,  00100,  00040,  00020,  00010,  00004,
	/*          8       9       :       ;       <       =       >       ?      */
	/* 070- */  00002,  00001,  00202,  04006,  05000,  00102,  02006,  02012,
	/*          @       A       B       C       D       E       F       G      */
	/* 100- */  00022,  04400,  04200,  04100,  04040,  04020,  04010,  04004,
	/*          H       I       J       K       L       M       N       O      */
	/* 110- */  04002,  04001,  02400,  02200,  02100,  02040,  02020,  02010,
	/*          P       Q       R       S       T       U       V       W      */
	/* 120- */  02004,  02002,  02001,  01200,  01100,  01040,  01020,  01010,
	/*          X       Y       Z       [       \       ]       ^       _      */
	/* 130- */  01004,  01002,  01001,  00006,  04022,  01202,  04012,  01022,
	/*          `       a       b       c       d       e       f       g      */
	/* 140- */  0,      04400,  04200,  04100,  04040,  04020,  04010,  04004,
	/*          h       i       j       k       l       m       n       o      */
	/* 150- */  04002,  04001,  02400,  02200,  02100,  02040,  02020,  02010,
	/*          p       q       r       s       t       u       v       w      */
	/* 160- */  02004,  02002,  02001,  01200,  01100,  01040,  01020,  01010,
	/*          x       y       z       {       |       }       ~              */
	/* 170- */  01004,  01002 , 01001,  0,      0,      00017,  00007,  0
};


const PpWord asciiTo029[256] =
{
	/*                                                                         */
	/* 000- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*                                                                         */
	/* 010- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*                                                                         */
	/* 020- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*                                                                         */
	/* 030- */  0,      0,      0,      0,      0,      0,      0,      0,
	/*          space   !       "       #       $       %       &       '      */
	/* 040- */  0,      04006,  00006,  00102,  02102,  01042,  04000,  00022,
	/*          (       )       *       +       ,       -       .       /      */
	/* 050- */  04022,  02022,  02042,  04012,  01102,  02000,  04102,  01400,
	/*          0       1       2       3       4       5       6       7      */
	/* 060- */  01000,  00400,  00200,  00100,  00040,  00020,  00010,  00004,
	/*          8       9       :       ;       <       =       >       ?      */
	/* 070- */  00002,  00001,  00202,  02012,  04042,  00012,  01012,  01006,
	/*          @       A       B       C       D       E       F       G      */
	/* 100- */  00042,  04400,  04200,  04100,  04040,  04020,  04010,  04004,
	/*          H       I       J       K       L       M       N       O      */
	/* 110- */  04002,  04001,  02400,  02200,  02100,  02040,  02020,  02010,
	/*          P       Q       R       S       T       U       V       W      */
	/* 120- */  02004,  02002,  02001,  01200,  01100,  01040,  01020,  01010,
	/*          X       Y       Z       [       \       ]       ^       _      */
	/* 130- */  01004,  01002,  01001,  04202,  01202,  02202,  02006,  01022,
	/*          `       a       b       c       d       e       f       g      */
	/* 140- */  0,      04400,  04200,  04100,  04040,  04020,  04010,  04004,
	/*          h       i       j       k       l       m       n       o      */
	/* 150- */  04002,  04001,  02400,  02200,  02100,  02040,  02020,  02010,
	/*          p       q       r       s       t       u       v       w      */
	/* 160- */  02004,  02002,  02001,  01200,  01100,  01040,  01020,  01010,
	/*          x       y       z       {       |       }       ~              */
	/* 170- */  01004,  01002 , 01001,  0,      0,      00017,  00007,  0
};

/*
**  077 is & but is also used as filler for unused codes
*/
const u8 asciiToBcd[256] =
{
	/*                                                                         */
	/* 000- */  077,    077,    077,    077,    077,    077,    077,    077,
	/*                                                                         */
	/* 010- */  077,    077,    077,    077,    077,    077,    077,    077,
	/*                                                                         */
	/* 020- */  077,    077,    077,    077,    077,    077,    077,    077,
	/*                                                                         */
	/* 030- */  077,    077,    077,    077,    077,    077,    077,    077,
	/*          space   !       "       #       $       %       &       '      */
	/* 040- */  060,    052,    014,    076,    053,    016,    077,    055,
	/*          (       )       *       +       ,       -       .       /      */
	/* 050- */  074,    034,    054,    020,    073,    040,    033,    061,
	/*          0       1       2       3       4       5       6       7      */
	/* 060- */  000,    001,    002,    003,    004,    005,    006,    007,
	/*          8       9       :       ;       <       =       >       ?      */
	/* 070- */  010,    011,    012,    037,    032,    013,    057,    056,
	/*          @       A       B       C       D       E       F       G      */
	/* 100- */  015,    021,    022,    023,    024,    025,    026,    027,
	/*          H       I       J       K       L       M       N       O      */
	/* 110- */  030,    031,    041,    042,    043,    044,    045,    046,
	/*          P       Q       R       S       T       U       V       W      */
	/* 120- */  047,    050,    051,    062,    063,    064,    065,    066,
	/*          X       Y       Z       [       \       ]       ^       _      */
	/* 130- */  067,    070,    071,    017,    035,    072,    036,    075,
	/*          `       a       b       c       d       e       f       g      */
	/* 140- */  077,    021,    022,    023,    024,    025,    026,    027,
	/*          h       i       j       k       l       m       n       o      */
	/* 150- */  030,    031,    041,    042,    043,    044,    045,    046,
	/*          p       q       r       s       t       u       v       w      */
	/* 160- */  047,    050,    051,    062,    063,    064,    065,    066,
	/*          x       y       z       {       |       }       ~              */
	/* 170- */  067,    070,    071,    077,    077,    077,    077,    077
};

const char bcdToAscii[64] =
{
	/* 00-07 */     '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
	/* 10-17 */     '8',    '9',    ':',    '=',    '"',    '@',    '%',    '[',
	/* 20-27 */     '+',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
	/* 30-37 */     'H',    'I',    '<',    '.',    ')',    '\\',    '^',    ';',
	/* 40-47 */     '-',    'J',    'K',    'L',    'M',    'N',    'O',    'P',
	/* 50-57 */     'Q',    'R',    '!',    '$',    '*',    0x27,    '?',    '>',
	/* 60-67 */     ' ',    '/',    'S',    'T',    'U',    'V',    'W',    'X',
	/* 70-77 */     'Y',    'Z',    ']',    ',',    '(',    '_',    '#',    '&'
};

const char extBcdToAscii[64] = {
	/* 00-07 */     ':',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
	/* 10-17 */     '8',    '9',    '0',    '=',    '"',    '@',    '%',    '[',
	/* 20-27 */     ' ',    '/',    'S',    'T',    'U',    'V',    'W',    'X',
	/* 30-37 */     'Y',    'Z',    ']',    ',',    '(',    '_',    '#',    '&',
	/* 40-47 */     '-',    'J',    'K',    'L',    'M',    'N',    'O',    'P',
	/* 50-57 */     'Q',    'R',    '!',    '$',    '*',   '\'',    '?',    '>',
	/* 60-67 */     '+',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
	/* 70-77 */     'H',    'I',    '<',    '.',    ')',   '\\',    '^',    ';'
};

/* Plato keycodes translation */

/* Note that we have valid entries (entry != -1) here only for those keys
** where we do not need to do anything unusual for Shift.  For example,
** "space" is not decoded here because Shift-Space gets a different code.
*/
const i8 asciiToPlato[128] =
{
	/*                                                                         */
	/* 000- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*                                                                         */
	/* 010- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*                                                                         */
	/* 020- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*                                                                         */
	/* 040- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*          space   !       "       #       $       %       &       '      */
	/* 040- */ -1,      0176,   0177,  -1,      0044,   0045,  -1,      0047,
	/*          (       )       *       +       ,       -       .       /      */
	/* 050- */  0051,   0173,   0050,   0016,   0137,   0017,   0136,   0135,
	/*          0       1       2       3       4       5       6       7      */
	/* 060- */  0000,   0001,   0002,   0003,   0004,   0005,   0006,   0007,
	/*          8       9       :       ;       <       =       >       ?      */
	/* 070- */  0010,   0011,   0174,   0134,   0040,   0133,   0041,   0175,
	/*          @       A       B       C       D       E       F       G      */
	/* 100- */ -1,      0141,   0142,   0143,   0144,   0145,   0146,   0147,
	/*          H       I       J       K       L       M       N       O      */
	/* 110- */  0150,   0151,   0152,   0153,   0154,   0155,   0156,   0157,
	/*          P       Q       R       S       T       U       V       W      */
	/* 120- */  0160,   0161,   0162,   0163,   0164,   0165,   0166,   0167,
	/*          X       Y       Z       [       \       ]       ^       _      */
	/* 130- */  0170,   0171,   0172,   0042,  -1,      0043,  -1,      0046,
	/*          `       a       b       c       d       e       f       g      */
	/* 140- */ -1,      0101,   0102,   0103,   0104,   0105,   0106,   0107,
	/*          h       i       j       k       l       m       n       o      */
	/* 150- */  0110,   0111,   0112,   0113,   0114,   0115,   0116,   0117,
	/*          p       q       r       s       t       u       v       w      */
	/* 160- */  0120,   0121,   0122,   0123,   0124,   0125,   0126,   0127,
	/*          x       y       z       {       |       }       ~              */
	/* 170- */  0130,   0131,   0132,  -1,     -1,     -1,     -1,     -1
};

/* Keycode translation for ALT-keypress */
const i8 altKeyToPlato[128] =
{
	/*                                                                         */
	/* 000- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*                                                                         */
	/* 010- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*                                                                         */
	/* 020- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*                                                                         */
	/* 040- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*          space   !       "       #       $       %       &       '      */
	/* 040- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*          (       )       *       +       ,       -       .       /      */
	/* 050- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*          0       1       2       3       4       5       6       7      */
	/* 060- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*          8       9       :       ;       <       =       >       ?      */
	/* 070- */ -1,     -1,     -1,     -1,     -1,      0015,  -1,     -1,
	/*          @       A       B       C       D       E       F       G      */
	/* 100- */ -1,      0062,   0070,   0073,   0071,   0067,   0064,  -1,
	/*          H       I       J       K       L       M       N       O      */
	/* 110- */  0065,  -1,     -1,     -1,      0075,   0064,   0066,  -1,
	/*          P       Q       R       S       T       U       V       W      */
	/* 120- */ -1,      0074,   0063,   0072,   0062,  -1,     -1,     -1,
	/*          X       Y       Z       [       \       ]       ^       _      */
	/* 130- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
	/*          `       a       b       c       d       e       f       g      */
	/* 140- */ -1,      0022,   0030,   0033,   0031,   0027,   0064,  -1,
	/*          h       i       j       k       l       m       n       o      */
	/* 150- */  0025,  -1,     -1,     -1,      0035,   0024,   0026,  -1,
	/*          p       q       r       s       t       u       v       w      */
	/* 160- */ -1,      0034,   0023,   0032,   0062,  -1,     -1,     -1,
	/*          x       y       z       {       |       }       ~              */
	/* 170- */ -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1
};

/*---------------------------  End Of File  ------------------------------*/

