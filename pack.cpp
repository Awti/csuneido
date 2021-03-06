// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "pack.h"
#include "gcstring.h"
#include "sustring.h"
#include "suboolean.h"
#include "sunumber.h"
#include "sudate.h"
#include "suobject.h"
#include "surecord.h"

Value unpack(const char* buf, int len) {
	return unpack(gcstring::noalloc(buf, len));
}

Value unpack(const gcstring& s) {
	if (s.size() == 0)
		return SuEmptyString;
	switch (s[0]) {
	case PACK_FALSE:
		return SuFalse;
	case PACK_TRUE:
		return SuTrue;
	case PACK_MINUS:
	case PACK_PLUS:
		return SuNumber::unpack(s);
	case PACK_STRING:
		return SuString::unpack(s);
	case PACK_DATE:
		return SuDate::unpack(s);
	case PACK_OBJECT:
		return SuObject::unpack(s);
	case PACK_RECORD:
		return SuRecord::unpack(s);
	default:
		unreachable();
	}
}

gcstring unpack_gcstr(const gcstring& s) {
	return s[0] == PACK_STRING ? s.substr(1) : unpack(s).gcstr();
}

// string ===========================================================

int packstrsize(const char* s) {
	return strlen(s) + 1;
}

int packstr(char* buf, const char* s) {
	int n = strlen(s) + 1;
	memcpy(buf, s, n);
	return n;
}

const char* unpackstr(const char*& buf) {
	const char* s = buf;
	buf += strlen(s) + 1;
	return s;
}

// int =============================================================
/*
size_t packsizeint(int n) {
	if (n == 0)
		return 1;
	if (n < 0)
		n = -n;
	//	while (n % 10000 == 0)
	//		n /= 10000;
	if (n < 10000)
		return 4;
	if (n < 100000000)
		return 6;
	return 8;
}

void packint(char* buf, int n) {
	buf[0] = n < 0 ? PACK_MINUS : PACK_PLUS;
	if (n == 0)
		return;
	int e = 0;
	//	for (; n % 10000 == 0; n /= 10000)
	//		++e;
	uint32_t x;
	if (n > 0) {
		if (n < 10000) {
			buf[1] = char(e + 1 ^ 0x80); // exponent
			buf[2] = n >> 8;
			buf[3] = n;
		} else if (n < 100000000) {
			buf[1] = char(e + 2 ^ 0x80); // exponent
			x = n / 10000;
			buf[2] = x >> 8;
			buf[3] = x;
			x = n % 10000;
			buf[4] = x >> 8;
			buf[5] = x;
		} else {
			buf[1] = char(e + 3 ^ 0x80); // exponent
			x = n / 100000000;
			buf[2] = x >> 8;
			buf[3] = x;
			n %= 100000000;
			x = n / 10000;
			buf[4] = x >> 8;
			buf[5] = x;
			x = n % 10000;
			buf[6] = x >> 8;
			buf[7] = x;
		}
	} else { // negative
		n = -n;
		if (n < 10000) {
			buf[1] = ~char(e + 1 ^ 0x80); // exponent
			buf[2] = ~(n >> 8);
			buf[3] = ~n;
		} else if (n < 100000000) {
			buf[1] = ~char(e + 2 ^ 0x80); // exponent
			x = n / 10000;
			buf[2] = ~(x >> 8);
			buf[3] = ~x;
			x = n % 10000;
			buf[4] = ~(x >> 8);
			buf[5] = ~x;
		} else {
			buf[1] = ~char(e + 3 ^ 0x80); // exponent
			x = n / 100000000;
			buf[2] = ~(x >> 8);
			buf[3] = ~x;
			n %= 100000000;
			x = n / 10000;
			buf[4] = ~(x >> 8);
			buf[5] = ~x;
			x = n % 10000;
			buf[6] = ~(x >> 8);
			buf[7] = ~x;
		}
	}
}

// unsigned, min coef
uint64_t unpackcoef(const uint8_t* buf, int sz) {
	uint64_t n = 0;
	if (buf[0] == PACK_PLUS) {
		if (sz >= 4)
			n = (buf[2] << 8) | buf[3];
		if (sz >= 6)
			n = 10000 * n + ((buf[4] << 8) | buf[5]);
		if (sz >= 8)
			n = 10000 * n + ((buf[6] << 8) | buf[7]);
		if (sz >= 10)
			n = 10000 * n + ((buf[8] << 8) | buf[9]);
	} else { // PACK_MINUS
		if (sz >= 4)
			n = (uint16_t) ~((buf[2] << 8) + buf[3]);
		if (sz >= 6)
			n = 10000 * n + (uint16_t) ~((buf[4] << 8) | buf[5]);
		if (sz >= 8)
			n = 10000 * n + (uint16_t) ~((buf[6] << 8) | buf[7]);
		if (sz >= 10)
			n = 10000 * n + (uint16_t) ~((buf[8] << 8) | buf[9]);
	}
	return n;
}

int unpackint(const gcstring& s) {
	int sz = s.size();
	if (sz <= 2)
		return 0;
	verify(sz == 4 || sz == 6 || sz == 8);
	auto buf = (const uint8_t*) s.ptr();
	verify(buf[0] == PACK_PLUS || buf[0] == PACK_MINUS);
	bool minus = buf[0] == PACK_MINUS;
	int e = uint8_t(buf[1]);
	if (minus)
		e = uint8_t(~e);
	e = int8_t(e ^ 0x80) - (sz - 2) / 2;
	int n = unpackcoef(buf, sz);
	for (; e > 0; --e)
		n *= 10000;
	return minus ? -n : n;
}
*/
// test =============================================================

#include "testing.h"
#include <malloc.h>

static void testpack(int x) {
	char buf[80];
	packint(buf, x);
	gcstring s = gcstring::noalloc(buf, packsizeint(x));
	int y = unpackint(s);
	assert_eq(x, y);
	Value num = ::unpack(s);
	assert_eq(x, num.integer());
	assert_eq(packsizeint(x), num.packsize());
	char buf2[80];
	num.pack(buf2);
	verify(0 == memcmp(buf, buf2, packsizeint(x)));
}
TEST(pack_int) {
	testpack(0);
	testpack(1);
	testpack(-1);
	testpack(1234);
	testpack(-1234);
	testpack(12345678);
	testpack(-12345678);
	testpack(INT_MAX);
	testpack(INT_MIN + 1); // no positive equivalent to INT_MIN
}

#include "porttest.h"
#include "compile.h"
#include "ostreamstr.h"

PORTTEST(compare_packed) {
	int n = args.size();
	for (int i = 0; i < n; ++i) {
		Value x = constant(args[i].str());
		gcstring px = x.pack();
		for (int j = i + 1; j < n; ++j) {
			Value y = constant(args[j].str());
			gcstring py = y.pack();
			if (!(px < py) || (py < px))
				return OSTR("\n\t" << x << " <=> " << y);
		}
	}
	return nullptr;
}
