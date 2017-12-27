#include <uvm/uvm_common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <uvm/ldebug.h>
#include <uvm/lobject.h>
#include <uvm/lopcodes.h>
#include <uvm/lundump.h>

#include <uvm/uvm_compat.h>
#include <uvm/uvm_proto_info.h>

namespace uvm {
	namespace decompile {

		const char* operators[UNUM_OPCODES];
		int priorities[UNUM_OPCODES];

		Inst extractInstruction(Instruction i) {
			Inst inst;
			inst.op = GET_OPCODE(i);
			inst.a = GETARG_A(i);
			inst.b = GETARG_B(i);
			inst.c = GETARG_C(i);
			inst.bx = GETARG_Bx(i);
			inst.sbx = GETARG_sBx(i);
			inst.ax = GETARG_Ax(i);
			return inst;
		}

		Instruction assembleInstruction(Inst inst) {
			Instruction i;
			switch (getOpMode(inst.op)) {
			case iABC:
				i = CREATE_ABC(inst.op, inst.a, inst.b, inst.c);
				break;
			case iABx:
				i = CREATE_ABx(inst.op, inst.a, inst.bx);
				break;
			case iAsBx:
				i = CREATE_ABx(inst.op, inst.a, inst.sbx);
				break;
#if LUA_VERSION_NUM == 502 || LUA_VERSION_NUM == 503
			case iAx:
				i = CREATE_Ax(inst.op, inst.ax);
				break;
#endif
			}
			return i;
		}

		void InitOperators() {
			int i;
			for (i = 0; i < UNUM_OPCODES; i++) {
				operators[i] = " ";
				priorities[i] = 0;
			}
			operators[UOP_POW] = "^"; priorities[UOP_POW] = 1;
			operators[UOP_NOT] = "not "; priorities[UOP_NOT] = 2;
			operators[UOP_LEN] = "#"; priorities[UOP_LEN] = 2;
			operators[UOP_UNM] = "-"; priorities[UOP_UNM] = 2;
			operators[UOP_MUL] = "*"; priorities[UOP_MUL] = 3;
			operators[UOP_DIV] = "/"; priorities[UOP_DIV] = 3;
			operators[UOP_MOD] = "%"; priorities[UOP_MOD] = 3;
			operators[UOP_ADD] = "+"; priorities[UOP_ADD] = 4;
			operators[UOP_SUB] = "-"; priorities[UOP_SUB] = 4;
			operators[UOP_CONCAT] = ".."; priorities[UOP_CONCAT] = 5;

			operators[UOP_BNOT] = "~"; priorities[UOP_BNOT] = 2;
			operators[UOP_IDIV] = "//"; priorities[UOP_IDIV] = 3;
			operators[UOP_SHL] = "<<"; priorities[UOP_SHL] = 6;
			operators[UOP_SHR] = ">>"; priorities[UOP_SHR] = 6;
			operators[UOP_BAND] = "&"; priorities[UOP_BAND] = 7;
			operators[UOP_BXOR] = "~"; priorities[UOP_BXOR] = 8;
			operators[UOP_BOR] = "|"; priorities[UOP_BOR] = 9;
		}

		char* convertToUpper(const char* str) {
			char *newstr, *p;
			p = newstr = strdup(str);
			while (*p++ = toupper(*p));
			return newstr;
		}

		int getEncoding(const char* enc) {
			int ret = 0;
			char* src = convertToUpper(enc);
			if (strcmp(src, "ASCII") == 0) {
				ret = ASCII;
			}
			else if (strcmp(src, "GB2312") == 0) {
				ret = GB2312;
			}
			else 	if (strcmp(src, "GBK") == 0) {
				ret = GBK;
			}
			else 	if (strcmp(src, "GB18030") == 0) {
				ret = GB18030;
			}
			else 	if (strcmp(src, "BIG5") == 0) {
				ret = BIG5;
			}
			else 	if (strcmp(src, "UTF8") == 0) {
				ret = UTF8;
			}
			free(src);
			return ret;
		}


		int isUTF8(const unsigned char* buff, int size) {
			int utf8length;
			const unsigned char* currchr = buff;
			if (*currchr < 0x80) { // (10000000): 1 byte ASCII
				utf8length = 1;
			}
			else if (*currchr < 0xC0) { // (11000000): invalid
				utf8length = 0;
				return utf8length;
			}
			else if (*currchr < 0xE0) { // (11100000): 2 byte
				utf8length = 2;
			}
			else if (*currchr < 0xF0) { // (11110000): 3 byte
				utf8length = 3;
			}
			else if (*currchr < 0xF8) { // (11111000): 4 byte
				utf8length = 4;
			}
			else if (*currchr < 0xFC) { // (11111100): 5 byte
				utf8length = 5;
			}
			else if (*currchr < 0xFE) { // (11111110): 6 byte
				utf8length = 6;
			}
			else { // invalid
				utf8length = 0;
				return utf8length;
			}
			if (utf8length > size) {
				utf8length = 0;
				return utf8length;
			}
			currchr++;
			while (currchr < buff + utf8length) {
				if ((*currchr & 0xC0) != 0x80) { // 10xxxxxx
					utf8length = 0;
					return utf8length;
				}
				currchr++;
			}
			return utf8length;
		}

		// PrintString from luac is not 8-bit clean
		std::string DecompileString(const TValue* o) {
			int i, utf8length;
			TString* ts = rawtsvalue(o);
			const unsigned char* s = (const unsigned char*)getstr(ts);
			int len = (int)LUA_STRLEN(ts);
			char* ret = (char*)calloc(len * 4 + 3, sizeof(char));
			int p = 0;
			ret[p++] = '"';
			for (i = 0; i < len; i++, s++) {
				switch (*s) {
				case '\a':
					ret[p++] = '\\';
					ret[p++] = 'a';
					break;
				case '\b':
					ret[p++] = '\\';
					ret[p++] = 'b';
					break;
				case '\f':
					ret[p++] = '\\';
					ret[p++] = 'f';
					break;
				case '\n':
					ret[p++] = '\\';
					ret[p++] = 'n';
					break;
				case '\r':
					ret[p++] = '\\';
					ret[p++] = 'r';
					break;
				case '\t':
					ret[p++] = '\\';
					ret[p++] = 't';
					break;
				case '\v':
					ret[p++] = '\\';
					ret[p++] = 'v';
					break;
				case '\\':
					ret[p++] = '\\';
					ret[p++] = '\\';
					break;
				case '"':
					ret[p++] = '\\';
					ret[p++] = '"';
					break;
				case '\'':
					ret[p++] = '\\';
					ret[p++] = '\'';
					break;
				default:
					if (*s >= 0x20 && *s < 0x7F) {
						ret[p++] = *s;
					}
					else if (string_encoding == GB2312 && i + 1 < len
						&& *s >= 0xA1 && *s <= 0xF7
						&& *(s + 1) >= 0xA1 && *(s + 1) <= 0xFE
						) {
						ret[p++] = *s;
						i++; s++;
						ret[p++] = *s;
					}
					else if ((string_encoding == GBK || string_encoding == GB18030) && i + 1 < len
						&& *s >= 0x81 && *s <= 0xFE
						&& *(s + 1) >= 0x40 && *(s + 1) <= 0xFE && *(s + 1) != 0x7F
						) {
						ret[p++] = *s;
						i++; s++;
						ret[p++] = *s;
					}
					else if (string_encoding == GB18030 && i + 3 < len
						&& *s >= 0x81 && *s <= 0xFE
						&& *(s + 1) >= 0x30 && *(s + 1) <= 0x39
						&& *(s + 2) >= 0x81 && *(s + 2) <= 0xFE
						&& *(s + 3) >= 0x30 && *(s + 3) <= 0x39
						) {
						ret[p++] = *s;
						i++; s++;
						ret[p++] = *s;
						i++; s++;
						ret[p++] = *s;
						i++; s++;
						ret[p++] = *s;
					}
					else if (string_encoding == BIG5 && i + 1 < len
						&& *s >= 0x81 && *s <= 0xFE
						&& ((*(s + 1) >= 0x40 && *(s + 1) <= 0x7E) || (*(s + 1) >= 0xA1 && *(s + 1) <= 0xFE))
						) {
						ret[p++] = *s;
						i++; s++;
						ret[p++] = *s;
					}
					else if (string_encoding == UTF8 && (utf8length = isUTF8(s, len - i)) > 1) {
						int j;
						ret[p++] = *s;
						for (j = 0; j < utf8length - 1; j++) {
							i++; s++;
							ret[p++] = *s;
						}
					}
					else {
						char* pos = &(ret[p]);
						sprintf(pos, "\\%03d", *s);
						p += (int)strlen(pos);
					}
					break;
				}
			}
			ret[p++] = '"';
			ret[p] = '\0';
			std::string ret_str(ret);
			free(ret);
			return ret_str;
		}

		std::string DecompileConstant(const Proto* f, int i) {
			const TValue* o = &f->k[i];
			switch (ttype(o)) {
			case LUA_TBOOLEAN:
				return bvalue(o) ? "true" : "false";
			case LUA_TNIL:
				return "nil";
			case LUA_TNUMFLT:
			{
				char ret[128];
				memset(ret, 0x0, sizeof(ret));
				sprintf(ret, LUA_NUMBER_FMT, fltvalue(o));
				ret[127] = '\0';
				return ret;
			}
			case LUA_TNUMINT:
			{
				char ret[128];
				memset(ret, 0x0, sizeof(ret));
				sprintf(ret, LUA_INTEGER_FMT, ivalue(o));
				ret[127] = '\0';
				return ret;
			}
			case LUA_TSHRSTR:
			case LUA_TLNGSTR:
				return DecompileString(o);
			default:
				return "Unknown_Type_Error";
			}
		}

	}
}