// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "record.h"
#include "sustring.h"
#include "pack.h"
#include "gc.h"
#include "mmfile.h"
#include <climits>
#include <algorithm>
using std::max;
using std::min;

// ReSharper disable CppSomeObjectMembersMightNotBeInitialized

template <class T>
struct RecRep {
	// TODO: convert type to enum
	short type;
	short n;  // current number of fields (starts at 0)
	T sz;     // total length of RecRep (also referenced as off[-1])
	T off[1]; // actual array size varies
	gcstring get(int i) const { // NOTE: no alloc - gcstring points to rep
		if (i >= n)
			return gcstring();
		size_t size = off[i - 1] - off[i];
		verify(size <= sz - off[i]);
		return gcstring::noalloc((char*) this + off[i], size);
	}
	size_t cursize() const {
		size_t base = sizeof(short) + sizeof(short) + // type & n
			sz - off[n - 1];                          // data
		size_t size = base + sizeof(char) * (n + 1);  // offsets
		if (size <= UCHAR_MAX)
			return size;
		size = base + sizeof(short) * (n + 1); // offsets
		if (size <= USHRT_MAX)
			return size;
		return base + sizeof(int) * (n + 1); // offsets
	}
	int avail() const {
		return off[n - 1] - (2 * sizeof(short) + (n + 2) * sizeof(T));
	}
	// n+2 instead of n+1 to allow for a new item's offset
	char* alloc(size_t len) {
		off[n] = off[n - 1] - len;
		return (char*) this + off[n++];
	}
	void add(const gcstring& s) {
		memcpy(alloc(s.size()), s.ptr(), s.size());
	}
	void truncate(int i) {
		if (i < n)
			n = i;
	}
};

struct DbRep {
	DbRep(Mmfile* mmf, Mmoffset off)
		: type('d'), offset(off), rec(mmf->adr(offset)) {
		verify(offset > 0);
	}

	short type; // has to match RecRep
	Mmoffset offset;
	Record rec;
};

Record::Record(size_t sz) // NOLINT
	: crep((RecRep<unsigned char>*) ::operator new(sz + 1, noptrs)) {
	init(sz);
	// ensure that final value is followed by nul
	reinterpret_cast<char*>(crep)[sz] = 0;
}

Record::Record(size_t sz, const void* buf) // NOLINT
	: crep((RecRep<unsigned char>*) buf) {
	init(sz);
}

void Record::init(size_t sz) {
	crep->n = 0;
	if (sz <= UCHAR_MAX) {
		crep->type = 'c';
		crep->sz = sz;
	} else if (sz <= USHRT_MAX) {
		srep->type = 's';
		srep->sz = sz;
	} else {
		lrep->type = 'l';
		lrep->sz = sz;
	}
}

// used by database
void Record::reuse(int n) {
	if (crep->type == 'd')
		dbrep->rec.crep->n = n;
	else
		crep->n = n;
}

Record::Record(const void* r) // NOLINT
	: crep((RecRep<unsigned char>*) r) {
	verify(!crep ||
		(crep->type == 'c' || crep->type == 's' ||
			crep->type == 'l')); // NOLINT
}

Record::Record(Mmfile* mmf, Mmoffset offset) // NOLINT
	: dbrep(new DbRep(mmf, offset)) {
}

void* Record::ptr() const {
	if (!crep)
		return nullptr;
	return (void*) (crep->type == 'd' ? dbrep->rec.crep : crep);
}

Mmoffset Record::off() const {
	verify(crep->type == 'd');
	verify(dbrep->offset >= 0);
	return dbrep->offset;
}

int Record::size() const {
	if (!crep)
		return 0;
	return crep->type == 'd' ? dbrep->rec.crep->n : crep->n;
}

#define CALL(meth) \
	Record r = crep->type == 'd' ? dbrep->rec : *this; \
	switch (r.crep->type) { \
	case 'c': \
		return r.crep->meth; \
	case 's': \
		return r.srep->meth; \
	case 'l': \
		return r.lrep->meth; \
	default: \
		error("invalid record"); \
	}

gcstring Record::getraw(int i) const {
	if (!crep)
		return gcstring();
	CALL(get(i));
}

Value Record::getval(int i) const {
	return ::unpack(getraw(i));
}

gcstring Record::getstr(int i) const {
	return unpack_gcstr(getraw(i));
}

int Record::getint(int i) const {
	return unpackint(getraw(i));
}

bool Record::hasprefix(Record r) {
	if (r.size() == 0)
		return true;
	for (int i = 0; getraw(i) == r.getraw(i);)
		if (++i >= r.size())
			return true;
	return false;
}

bool Record::prefixgt(Record r) {
	gcstring x, y;
	int n = min(size(), r.size());
	for (int i = 0; i < n; ++i)
		if ((x = getraw(i)) != (y = r.getraw(i)))
			return x > y;
	// prefix equal
	return false;
}

size_t Record::cursize() const {
	if (!crep)
		return sizeof(RecRep<uint8_t>); // min RecRep size
	CALL(cursize());
}

size_t Record::bufsize() const {
	if (!crep)
		return sizeof(RecRep<uint8_t>); // min RecRep size
	CALL(sz);
}

int Record::avail() const {
	if (!crep)
		return -1; // -1 because you can't even add a 0 length item
	CALL(avail());
}

char* Record::alloc(size_t n) {
	if (avail() < (int) n)
		grow(n);
	CALL(alloc(n));
}

void Record::addnil() {
	alloc(0);
}

void Record::addmax() {
	*alloc(1) = 0x7f;
}

void Record::addraw(const gcstring& s) {
	memcpy(alloc(s.size()), s.ptr(), s.size());
}

void Record::addval(Value x) {
	x.pack(alloc(x.packsize()));
}

void Record::addval(const gcstring& s) {
	SuString str(s);
	addval(&str);
}

void Record::addval(const char* s) {
	addval(gcstring::noalloc(s));
}

void Record::addval(int n) {
	packint(alloc(packintsize(n)), n);
}

template <class Src, class Dst>
inline void repcopy(Src src, Dst dst) {
	// COULD: copy the data with one memcpy, then adjust offsets
	int n = src->n;
	for (int i = 0; i < n; ++i)
		dst->add(src->get(i));
}

void Record::copyrec(Record src, Record& dst) {
	if (!src.crep)
		return;
	verify(dst.crep);
	verify(dst.crep->type != 'd');
	if (src.crep->type == 'd')
		copyrec(src.dbrep->rec, dst);
	else if (src.crep->type == 'c') {
		if (dst.crep->type == 'c')
			repcopy(src.crep, dst.crep);
		else if (dst.crep->type == 's')
			repcopy(src.crep, dst.srep);
		else if (dst.crep->type == 'l')
			repcopy(src.crep, dst.lrep);
		else
			error("invalid record");
	} else if (src.srep->type == 's') {
		if (dst.crep->type == 'c')
			repcopy(src.srep, dst.crep);
		else if (dst.srep->type == 's')
			repcopy(src.srep, dst.srep);
		else if (dst.crep->type == 'l')
			repcopy(src.srep, dst.lrep);
		else
			error("invalid record");
	} else if (src.srep->type == 'l') {
		if (dst.crep->type == 'c')
			repcopy(src.lrep, dst.crep);
		else if (dst.crep->type == 's')
			repcopy(src.lrep, dst.srep);
		else if (dst.crep->type == 'l')
			repcopy(src.lrep, dst.lrep);
		else
			error("invalid record");
	} else
		error("invalid record");
}

void Record::grow(int need) {
	if (!crep) {
		Record dst(max(60, 2 * need));
		crep = dst.crep;
	} else {
		Record dst(2 * (cursize() + need));
		copyrec(*this, dst);
		crep = dst.crep;
	}
}

void* Record::copyto(void* buf) const {
	Record dst(cursize(), buf);
	copyrec(*this, dst);
	return buf;
}

void Record::moveto(Mmfile* mmf, Mmoffset offset) {
	copyto(mmf->adr(offset));
	dbrep = new DbRep(mmf, offset);
}

Record Record::dup() const {
	if (!crep)
		return Record();
	Record dst(cursize());
	copyrec(*this, dst);
	return dst;
}

void Record::truncate(int n) {
	if (!crep)
		return;
	switch (crep->type) {
	case 'c':
		crep->truncate(n);
		return;
	case 's':
		srep->truncate(n);
		return;
	case 'l':
		lrep->truncate(n);
		return;
	case 'd':
		error("dbrep truncate not allowed");
	default:
		error("invalid record");
	}
}

// MAYBE: rewrite op== and op< similar to copy

bool Record::operator==(Record r) const {
	if (crep == r.crep || (!crep && r.size() == 0) || (!r.crep && size() == 0))
		return true;
	else if (!crep || !r.crep)
		return false;

	if (size() != r.size())
		return false;
	for (int i = 0; i < size(); ++i)
		if (getraw(i) != r.getraw(i))
			return false;
	return true;
}

bool Record::operator<(Record r) const {
	gcstring x, y;
	int n = min(size(), r.size());
	for (int i = 0; i < n; ++i)
		if ((x = getraw(i)) != (y = r.getraw(i)))
			return x < y;
	// common fields are equal
	return size() < r.size();
}

#include <cctype>

Ostream& operator<<(Ostream& os, Record r) {
	os << '[';
	for (int i = 0; i < r.size(); ++i) {
		if (i > 0)
			os << ',';
		if (r.getraw(i) == "\x7f")
			os << "MAX";
		else
			os << r.getval(i);
	}
	return os << ']';
}

const int DB_BIT = 1 << MM_SHIFT;
const int DB_MASK = (1 << (MM_SHIFT + 1)) - 1;

Mmoffset Record::to_int64() const {
	if (!crep)
		return 0;
	switch (crep->type) {
	case 'c':
	case 's':
	case 'l':
		verify(((int) crep & DB_MASK) == 0);
		return reinterpret_cast<Mmoffset>(crep);
	case 'd':
		verify((dbrep->offset & DB_MASK) == 0);
		return dbrep->offset | DB_BIT;
	default:
		error("invalid record");
	}
}

Record Record::from_int64(Mmoffset n, Mmfile* mmf) {
	if (n & DB_BIT)
		return Record(mmf, n & ~DB_BIT);
	else
		return Record(reinterpret_cast<void*>(n));
}

inline int mmoffset_to_tagged_int(Mmoffset off) {
	return (off >> MM_SHIFT) | 1;
}

int Record::to_int() const {
	if (!crep)
		return 0;
	switch (crep->type) {
	case 'c':
	case 's':
	case 'l':
		verify(((int) crep & DB_MASK) == 0);
		return reinterpret_cast<int>(crep);
	case 'd':
		verify((dbrep->offset & DB_MASK) == 0);
		return mmoffset_to_tagged_int(dbrep->offset);
	default:
		error("invalid record");
	}
}

inline Mmoffset tagged_int_to_mmoffset(int n) {
	return static_cast<Mmoffset>(static_cast<unsigned int>(n & ~1)) << MM_SHIFT;
}

Record Record::from_int(int n, Mmfile* mmf) {
	if (n & 1)
		return Record(mmf, tagged_int_to_mmoffset(n));
	else
		return Record(reinterpret_cast<void*>(n));
}

Record const Record::empty;

// test =============================================================

#include "testing.h"

TEST(record_record) {
	Record r;

	r.addval("hello");
	verify(r.getstr(0) == "hello");
	r.addval("world");
	verify(r.getstr(0) == "hello");
	verify(r.getstr(1) == "world");

	void* buf = new char[32];
	(void) r.copyto(buf);
	Record r2(buf);
	verify(r2 == r);

	verify(r.dup() == r);

	r.addval(1234);
	verify(r.getint(2) == 1234);

	// add enough stuff to use all reps
	for (int i = 0; i < 1000; ++i)
		r.addval("123456789012345678901234567890123456789012345678901234567890"
				 "1234567890");
	verify(r.getstr(0) == "hello");
	verify(r.getstr(1) == "world");
	verify(r.getint(2) == 1234);

	Record big;
	big.addnil();
	const int bblen = 100000;
	char* bigbuf = salloc(bblen);
	memset(bigbuf, 0x77, bblen);
	big.addval(SuString::noalloc(bigbuf, bblen));
	int n = big.cursize();
	verify(100000 <= n && n <= 100050);
}

TEST(record_mmoffset) {
	Record r;
	int64_t n = 88888;
	r.addmmoffset(n);
	assert_eq(r.getmmoffset(0), n);
	n = 4 + (static_cast<int64_t>(1) << 32);
	int m = mmoffset_to_int(n);
	assert_eq(n, int_to_mmoffset(m));
	r.addmmoffset(n);
	assert_eq(r.getmmoffset(1), n);
}

TEST(record_to_from_int) {
	Record r;
	r.addval("hello");

	int64_t n64 = r.to_int64();
	Record r2 = Record::from_int64(n64, 0);
	assert_eq(r, r2);

	int n = r.to_int();
	Record r3 = Record::from_int(n, 0);
	assert_eq(r, r3);

	for (int64_t i = 1; i < 16; ++i) {
		Mmoffset mmo = i * 1024 * 1024 * 1024;
		n = mmoffset_to_tagged_int(mmo);
		assert_eq(tagged_int_to_mmoffset(n), mmo);
	}
}
