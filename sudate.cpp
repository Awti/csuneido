// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// Suneido Date class

// interface loosely based on Java Date, Calendar, & SimpleDateFormat

#include "sudate.h"
#include "suclass.h"
#include "date.h"
#include "interp.h"
#include "symbols.h"
#include "suboolean.h"
#include "sustring.h"
#include "sunumber.h"
#include "suobject.h"
#include "pack.h"
#include <cctype>
#include "func.h" // for argseach
#include "ostreamstr.h"
#include "cmpic.h"
#include "gc.h"
#include <algorithm>
using std::min;
using std::max;

static int ord = ::order("Date");

void* SuDate::operator new(size_t n) {
	return ::operator new(n, noptrs);
}

int SuDate::order() const {
	return ord;
}

bool SuDate::eq(const SuValue& y) const {
	if (y.order() == ord) {
		const SuDate& yd = static_cast<const SuDate&>(y);
		return date == yd.date && time == yd.time;
	} else
		return false;
}

bool SuDate::lt(const SuValue& y) const {
	int yo = y.order();
	if (yo == ord) {
		return *this < static_cast<const SuDate&>(y);
	} else
		return ord < yo;
}

bool SuDate::operator<(const SuDate& d) const {
	return date < d.date || (date == d.date && time < d.time);
}

void SuDate::out(Ostream& out) const {
	DateTime dt(date, time);
	out.fill('0');
	out << '#' << setw(4) << dt.year << setw(2) << dt.month << setw(2)
		<< dt.day;
	if (dt.hour || dt.minute || dt.second || dt.millisecond) {
		out << '.' << setw(2) << dt.hour << setw(2) << dt.minute;
		if (dt.second || dt.millisecond) {
			out << setw(2) << dt.second;
			if (dt.millisecond)
				out << setw(3) << dt.millisecond;
		}
	}
	out.fill(' ');
}

#define METHOD(fn) \
	{ #fn, &SuDate::fn }

SuDate::Mfn SuDate::method(Value member) {
	using Method = std::pair<Value, SuDate::Mfn>;
	static Method methods[]{METHOD(FormatEn), METHOD(Plus), METHOD(MinusDays),
		METHOD(MinusSeconds), METHOD(Year), METHOD(Month), METHOD(Day),
		METHOD(Hour), METHOD(Minute), METHOD(Second), METHOD(Millisecond),
		METHOD(WeekDay)};

	for (auto [name, meth] : methods)
		if (name == member)
			return meth;
	return nullptr;
}

Value SuDate::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	argseach(nargs, nargnames, argnames, each);
	if (auto meth = method(member))
		return (this->*meth)(nargs, nargnames, argnames, each);
	static UserDefinedMethods udm("Dates");
	if (Value c = udm(member))
		return c.call(self, member, nargs, nargnames, argnames, each);
	method_not_found("Date", member);
}

static void adjust(int& n, int& n2, int range) {
	if (n < 0 || n >= range) {
		n2 += n / range;
		if ((n %= range) < 0) {
			n += range;
			--n2;
		}
	}
}

static void normalize(DateTime& dt) {
	adjust(dt.second, dt.minute, 60);
	adjust(dt.minute, dt.hour, 60);
	adjust(dt.hour, dt.day, 24);
	--dt.month;
	adjust(dt.month, dt.year, 12);
	++dt.month;
	if (dt.day <= 0 || dt.day > 28) {
		int days = dt.day - 1;
		dt.day = 1;
		dt.add_days(days);
	}
}

SuDate::SuDate() {
	DateTime dt;
	date = dt.date();
	time = dt.time();
}

SuDate::SuDate(int d, int t) : date(d), time(t) {
	DateTime(d, t); // validate
}

Value SuDate::instantiate(
	short nargs, short nargnames, short* argnames, int each) {
	if (nargs == 0) {
		return new SuDate();
	} else if (nargs == 1 && nargnames == 0) {
		if (val_cast<SuDate*>(ARG(0)))
			return ARG(0);
		else {
			auto s = ARG(0).to_gcstr();
			if (s[0] == '#') {
				Value x = SuDate::literal(s.str());
				return x ? x : SuFalse;
			} else
				return parse(s.str());
		}
	} else if (nargs == 2 && nargnames == 0) {
		return parse(ARG(0).str(), ARG(1).str());
	} else if (nargnames == nargs) {
		static short year = ::symnum("year");
		static short month = ::symnum("month");
		static short day = ::symnum("day");
		static short hour = ::symnum("hour");
		static short minute = ::symnum("minute");
		static short second = ::symnum("second");
		static short millisecond = ::symnum("millisecond");
		DateTime dt;
		for (int i = 0; i < nargs; ++i) {
			int n = ARG(i).integer();
			if (argnames[i] == year)
				dt.year = n;
			else if (argnames[i] == month)
				dt.month = n;
			else if (argnames[i] == day)
				dt.day = n;
			else if (argnames[i] == hour)
				dt.hour = n;
			else if (argnames[i] == minute)
				dt.minute = n;
			else if (argnames[i] == second)
				dt.second = n;
			else if (argnames[i] == millisecond)
				dt.millisecond = n;
		}
		normalize(dt);
		if (!dt.valid())
			return SuFalse;

		return new SuDate(dt.date(), dt.time());
	} else
		except("usage: Date() or Date(string) or "
			   "Date(year:,month:,day:,hour:,minute:,second:");
}

const char* monthnames[] = {"January", "February", "March", "April", "May",
	"June", "July", "August", "September", "October", "November", "December"};
const char* weekday[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
	"Friday", "Saturday"};

enum { YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, UNK };
int minval[] = {0, 1, 1, 0, 0, 0};
int maxval[] = {3000, 12, 31, 59, 59, 59};

static bool ampm_ahead(const char* s) {
	if (*s == ' ')
		++s;
	return (tolower(s[0]) == 'a' || tolower(s[0]) == 'p') &&
		tolower(s[1]) == 'm';
}

static int char2digit(char c) {
	if (!isdigit(c))
		return -1;
	return c - '0';
}

static int get2digit(const char* s) {
	return char2digit(s[0]) * 10 + char2digit(s[1]);
}

static int get3digit(const char* s) {
	return char2digit(s[0]) * 100 + get2digit(s + 1);
}

static int get4digit(const char* s) {
	return 100 * get2digit(s) + get2digit(s + 2);
}

Value SuDate::parse(const char* s, const char* order) {
	const int NOTSET = 9999;
	DateTime dt(9999, 0, 0, NOTSET, NOTSET, NOTSET, 0);

	const char* date_patterns[] = {"", // set to system default
		"md", "dm", "dmy", "mdy", "ymd"};

	char syspat[4];
	{
		int i = 0;
		char prev = 0;
		for (auto o = order; *o && i < 3; prev = *o, ++o)
			if (*o != prev && (*o == 'y' || *o == 'M' || *o == 'd'))
				syspat[i++] = tolower(*o);
		syspat[i] = 0;
		if (i != 3)
			except("invalid date format: '" << order << "'");
		date_patterns[0] = syspat;

		// swap month-day patterns if system setting is day first
		for (i = 0; i < 3; ++i)
			if (syspat[i] == 'm')
				break;
			else if (syspat[i] == 'd')
				std::swap(date_patterns[1], date_patterns[2]);
	}

	// scan
	const int MAXTOKENS = 20;
	int type[MAXTOKENS];
	int tokens[MAXTOKENS];
	int ntokens = 0;
	bool got_time = false;
	std::fill(type, type + MAXTOKENS, (int) UNK);
	char prev = 0;
	while (*s) {
		if (ntokens >= MAXTOKENS)
			return SuFalse;
		if (isalpha(*s)) {
			char buf[80];
			char* dst = buf;
			*dst++ = toupper(*s);
			for (++s; isalpha(*s); ++s) {
				if (dst >= buf + sizeof buf)
					return SuFalse;
				*dst++ = tolower(*s);
			}
			verify(dst < buf + sizeof buf);
			*dst = 0;
			int i;
			for (i = 0; i < 12; ++i)
				if (has_prefix(monthnames[i], buf))
					break;
			if (i < 12) {
				type[ntokens] = MONTH;
				tokens[ntokens] = i + 1;
				++ntokens;
			} else if (0 == strcmp(buf, "Am") || 0 == strcmp(buf, "Pm")) {
				if (buf[0] == 'P') {
					if (dt.hour < 12)
						dt.hour += 12;
				} else { // (buf[0] == 'A')
					if (dt.hour == 12)
						dt.hour = 0;
					if (dt.hour > 12)
						return SuFalse;
				}
			} else {
				// ignore days of week
				for (i = 0; i < 7; ++i)
					if (has_prefix(weekday[i], buf))
						break;
				if (i >= 7)
					return SuFalse;
			}
		} else if ('0' <= *s && *s <= '9') {
			// can't use isdigit because it returns true for non-digits
			auto t = s;
			char* end;
			int n = strtoul(s, &end, 10);
			s = end;
			verify(s > t);
			if (s - t == 6 || s - t == 8) {
				if (s - t == 6) { // date with no separators with yy
					tokens[ntokens++] = get2digit(t);
					tokens[ntokens++] = get2digit(t += 2);
					tokens[ntokens++] = get2digit(t + 2);
				} else if (s - t == 8) { // date with no separators with yyyy
					for (int i = 0; i < 3; t += syspat[i] == 'y' ? 4 : 2, ++i)
						tokens[ntokens++] =
							syspat[i] == 'y' ? get4digit(t) : get2digit(t);
				}
				if (*s == '.') {
					++s;
					t = s;
					strtoul(s, &end, 10);
					s = end;
					if (s - t == 4 || s - t == 6 || s - t == 9) {
						dt.hour = get2digit(t);
						t += 2;
						dt.minute = get2digit(t);
						t += 2;
						if (s - t >= 2) {
							dt.second = get2digit(t);
							t += 2;
							if (s - t == 3)
								dt.millisecond = get3digit(t);
						}
					}
				}
			} else if (prev == ':' || *s == ':' || ampm_ahead(s)) { // time
				got_time = true;
				if (dt.hour == NOTSET)
					dt.hour = n;
				else if (dt.minute == NOTSET)
					dt.minute = n;
				else if (dt.second == NOTSET)
					dt.second = n;
				else
					return SuFalse;
			} else { // date
				tokens[ntokens] = n;
				if (prev == '\'')
					type[ntokens] = YEAR;
				++ntokens;
			}
		} else
			prev = *s++; // ignore
	}
	if (dt.hour == NOTSET)
		dt.hour = 0;
	if (dt.minute == NOTSET)
		dt.minute = 0;
	if (dt.second == NOTSET)
		dt.second = 0;

	// search for date match
	const char** pat;
	const char** end = date_patterns + sizeof date_patterns / sizeof(char*);
	for (pat = date_patterns; pat < end; ++pat) {
		// try one pattern
		auto p = *pat;
		int t;
		for (t = 0; *p && t < ntokens; ++p, ++t) {
			int part;
			if (*p == 'y')
				part = YEAR;
			else if (*p == 'm')
				part = MONTH;
			else if (*p == 'd')
				part = DAY;
			else
				unreachable();
			if ((type[t] != UNK && type[t] != part) ||
				tokens[t] < minval[part] || tokens[t] > maxval[part])
				break;
		}
		// stop at first match
		if (!*p && t == ntokens)
			break;
	}

	DateTime now;

	if (pat < end) {
		// use match
		int t = 0;
		for (auto p = *pat; *p; ++p, ++t) {
			if (*p == 'y')
				dt.year = tokens[t];
			else if (*p == 'm')
				dt.month = tokens[t];
			else if (*p == 'd')
				dt.day = tokens[t];
			else
				unreachable();
		}
	} else if (got_time && ntokens == 0) {
		dt.year = now.year;
		dt.month = now.month;
		dt.day = now.day;
	} else
		return SuFalse; // no match

	if (dt.year == 9999) {
		if (dt.month >= max(now.month - 5, 1) &&
			dt.month <= min(now.month + 6, 12))
			dt.year = now.year;
		else if (now.month < 6)
			dt.year = now.year - 1;
		else
			dt.year = now.year + 1;
	} else if (dt.year < 100) {
		int thisyr = now.year;
		dt.year += thisyr - thisyr % 100;
		if (dt.year - thisyr > 20)
			dt.year -= 100;
	}
	if (!dt.valid())
		return SuFalse;
	return new SuDate(dt.date(), dt.time());
}

inline void add(std::vector<char>& dst, const char* s) {
	dst.insert(dst.end(), s, s + strlen(s));
}

inline void add(std::vector<char>& dst, const char* s, int n) {
	dst.insert(dst.end(), s, s + n);
}

static SuString* format(int date, int time, const char* fmt) {
	DateTime dt(date, time);
	std::vector<char> dst;
	dst.reserve(strlen(fmt));
	for (auto f = fmt; *f; ++f) {
		int n = 1;
		if (isalpha(*f))
			for (char c = *f; f[1] == c; ++f)
				++n;
		switch (*f) {
		case 'y': {
			int yr = dt.year;
			if (n >= 4)
				dst.push_back('0' + yr / 1000);
			if (n >= 3)
				dst.push_back('0' + (yr % 1000) / 100);
			if (n >= 2 || (yr % 100) > 9)
				dst.push_back('0' + (yr % 100) / 10);
			dst.push_back('0' + yr % 10);
			break;
		}
		case 'M':
			if (n > 3)
				add(dst, monthnames[dt.month - 1]);
			else if (n == 3)
				add(dst, monthnames[dt.month - 1], 3);
			else {
				if (n >= 2 || dt.month > 9)
					dst.push_back('0' + dt.month / 10);
				dst.push_back('0' + dt.month % 10);
			}
			break;
		case 'd':
			if (n > 3)
				add(dst, weekday[dt.day_of_week()]);
			else if (n == 3)
				add(dst, weekday[dt.day_of_week()], 3);
			else {
				if (n >= 2 || dt.day > 9)
					dst.push_back('0' + dt.day / 10);
				dst.push_back('0' + dt.day % 10);
			}
			break;
		case 'h': // 12 hour
		{
			int hr = dt.hour % 12;
			if (hr == 0)
				hr = 12;
			if (n >= 2 || hr > 9)
				dst.push_back('0' + hr / 10);
			dst.push_back('0' + hr % 10);
			break;
		}
		case 'H': // 24 hour
			if (n >= 2 || dt.hour > 9)
				dst.push_back('0' + dt.hour / 10);
			dst.push_back('0' + dt.hour % 10);
			break;
		case 'm':
			if (n >= 2 || dt.minute > 9)
				dst.push_back('0' + dt.minute / 10);
			dst.push_back('0' + dt.minute % 10);
			break;
		case 's':
			if (n >= 2 || dt.second > 9)
				dst.push_back('0' + dt.second / 10);
			dst.push_back('0' + dt.second % 10);
			break;
		case 'a':
			dst.push_back(dt.hour < 12 ? 'a' : 'p');
			if (n > 1)
				dst.push_back('m');
			break;
		case 'A':
		case 't':
			dst.push_back(dt.hour < 12 ? 'A' : 'P');
			if (n > 1)
				dst.push_back('M');
			break;
#define LOCALE_STR_DELIMITER '\''
		case '\'':
			f++;
			while (*f && (*f != LOCALE_STR_DELIMITER))
				dst.push_back(*f++);
			break;
		case '\\':
			dst.push_back(*++f);
			break;
		default:
			while (--n >= 0)
				dst.push_back(*f);
		}
	}
	dst.push_back(0);
	return SuString::noalloc(&dst[0], dst.size() - 1);
}

Value SuDate::FormatEn(
	short nargs, short nargnames, short* argnames, int each) {
	if (nargs != 1)
		except("usage: date.Format(format)");
	argseach(nargs, nargnames, argnames, each);
	return format(date, time, ARG(0).str());
}

Value SuDate::Plus(short nargs, short nargnames, short* argnames, int each) {
	static short sn_years = ::symnum("years");
	static short sn_months = ::symnum("months");
	static short sn_days = ::symnum("days");
	static short sn_hours = ::symnum("hours");
	static short sn_minutes = ::symnum("minutes");
	static short sn_seconds = ::symnum("seconds");
	static short sn_milliseconds = ::symnum("milliseconds");
	const char* usage =
		"usage: Plus(years:, months:, days:, hours:, minutes:, seconds:, "
		"milliseconds:)";

	if (nargs != nargnames)
		except(usage);
	bool ok = false;
	int years = 0, months = 0, days = 0, hours = 0, minutes = 0, seconds = 0,
		milliseconds = 0;
	for (int i = 0; i < nargs; ++i) {
		int n = ARG(i).integer();
		if (argnames[i] == sn_years) {
			years = n;
			ok = true;
		} else if (argnames[i] == sn_months) {
			months = n;
			ok = true;
		} else if (argnames[i] == sn_days) {
			days = n;
			ok = true;
		} else if (argnames[i] == sn_hours) {
			hours = n;
			ok = true;
		} else if (argnames[i] == sn_minutes) {
			minutes = n;
			ok = true;
		} else if (argnames[i] == sn_seconds) {
			seconds = n;
			ok = true;
		} else if (argnames[i] == sn_milliseconds) {
			milliseconds = n;
			ok = true;
		}
	}
	if (!ok)
		except(usage);
	DateTime dt(date, time);
	dt.plus(years, months, days, hours, minutes, seconds, milliseconds);
	if (!dt.valid())
		except("bad date");
	return new SuDate(dt.date(), dt.time());
}

Value SuDate::MinusDays(
	short nargs, short nargnames, short* argnames, int each) {
	if (nargs != 1 || nargnames != 0)
		except("usage: date.Minus(date)");
	DateTime dt1(date, time);
	SuDate* d2 = force<SuDate*>(ARG(0));
	DateTime dt2(d2->date, d2->time);
	return dt1.minus_days(dt2);
}

int64_t SuDate::minus_ms(SuDate* d1, SuDate* d2) {
	DateTime dt1(d1->date, d1->time);
	DateTime dt2(d2->date, d2->time);
	if (dt1.minus_days(dt2) > 50 * 365) // 50 years
		except("date.MinusSeconds interval too large");
	return dt1.minus_milliseconds(dt2);
}

Value SuDate::MinusSeconds(
	short nargs, short nargnames, short* argnames, int each) {
	if (nargs != 1 || nargnames != 0)
		except("usage: date.MinusSeconds(date)");
	int64_t ms = minus_ms(this, force<SuDate*>(ARG(0)));
	return SuNumber::from_double(double(ms) / 1000);
}

Value SuDate::Year(short nargs, short nargnames, short* argnames, int each) {
	NOARGS("date.Year()");
	DateTime dt(date, time);
	return dt.year;
}

Value SuDate::Month(short nargs, short nargnames, short* argnames, int each) {
	NOARGS("date.Month()");
	DateTime dt(date, time);
	return dt.month;
}

Value SuDate::Day(short nargs, short nargnames, short* argnames, int each) {
	NOARGS("date.Day()");
	DateTime dt(date, time);
	return dt.day;
}

Value SuDate::Hour(short nargs, short nargnames, short* argnames, int each) {
	NOARGS("date.Hour()");
	DateTime dt(date, time);
	return dt.hour;
}

Value SuDate::Minute(short nargs, short nargnames, short* argnames, int each) {
	NOARGS("date.Minute()");
	DateTime dt(date, time);
	return dt.minute;
}

Value SuDate::Second(short nargs, short nargnames, short* argnames, int each) {
	NOARGS("date.Second()");
	DateTime dt(date, time);
	return dt.second;
}

Value SuDate::Millisecond(
	short nargs, short nargnames, short* argnames, int each) {
	NOARGS("date.Millisecond()");
	DateTime dt(date, time);
	return dt.millisecond;
}

Value SuDate::WeekDay(short nargs, short nargnames, short* argnames, int each) {
	if (nargs != 0 && nargs != 1)
		except("usage: date.WeekDay(firstDay = 'Sun')");
	int i = 0;
	if (nargs == 1) {
		if (auto s = ARG(0).str_if_str()) {
			for (i = 0; i < 7; ++i)
				if (0 == memcmpic(s, weekday[i], strlen(s)))
					break;
			if (i >= 7)
				except("usage: date.WeekDay( <day of week> )");
		} else
			i = ARG(0).integer();
	}
	DateTime dt(date, time);
	return (dt.day_of_week() - i + 7) % 7;
}

/* static */ Value SuDate::literal(const char* s) {
	if (*s == '#')
		++s;
	const char* t = strchr(s, '.');
	int sn = strlen(s);
	int tn = 0;
	if (t != NULL) {
		sn = t - s;
		tn = strlen(++t);
	}
	if (sn != 8 || (tn != 0 && tn != 4 && tn != 6 && tn != 9))
		return Value();

	int year = get4digit(s);
	int month = get2digit(s + 4);
	int day = get2digit(s + 6);

	int hour = (tn >= 2 ? get2digit(t) : 0);
	int minute = (tn >= 4 ? get2digit(t + 2) : 0);
	int second = (tn >= 6 ? get2digit(t + 4) : 0);
	int millisecond = (tn >= 9 ? get3digit(t + 6) : 0);

	DateTime dt(year, month, day, hour, minute, second, millisecond);
	if (!dt.valid())
		return Value();
	return new SuDate(dt.date(), dt.time());
}

void SuDate::pack(char* buf) const {
	*buf++ = PACK_DATE;
	int n = date;
	buf[3] = (char) n;
	buf[2] = (char) (n >>= 8);
	buf[1] = (char) (n >>= 8);
	buf[0] = (char) (n >> 8);
	n = time;
	buf[7] = (char) n;
	buf[6] = (char) (n >>= 8);
	buf[5] = (char) (n >>= 8);
	buf[4] = (char) (n >> 8);
}

SuDate* SuDate::unpack(const gcstring& s) {
	verify(s.size() == 9);
	auto p = (const unsigned char*) s.ptr();
	verify(*p == PACK_DATE);
	++p;

	int date = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	int time = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];

	return new SuDate(date, time);
}

SuDate& SuDate::increment() {
	DateTime dt(date, time);
	dt.plus(0, 0, 0, 0, 0, 0, 1);
	date = dt.date();
	time = dt.time();
	return *this;
}

/*static*/ SuDate* SuDate::timestamp() {
	static SuDate prev;
	SuDate* ts = new SuDate;
	if (*ts <= prev)
		*ts = prev.increment();
	else
		prev = *ts;
	return ts;
}

Value SuDateClass::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	static Value Begin("Begin");
	static Value End("End");

	argseach(nargs, nargnames, argnames, each);
	if (member == INSTANTIATE || member == CALL)
		return SuDate::instantiate(nargs, nargnames, argnames, each);
	if (member == Begin) {
		static Value begin = SuDate::literal("#17000101");
		NOARGS("Date.Begin()");
		return begin;
	}
	if (member == End) {
		static Value end = SuDate::literal("#30000101");
		NOARGS("Date.End()");
		return end;
	}
	method_not_found("Date", member);
}

void SuDateClass::out(Ostream& os) const {
	os << "Date /* builtin class */";
}

const char* SuDateClass::type() const {
	return "BuiltinClass";
}

//-------------------------------------------------------------------

#include "testing.h"

TEST(sudate_packing) {
	Value x = SuDate::parse("2000/3/4 12:34:56");
	SuDate* date = force<SuDate*>(x);
	verify(date->packsize() == 9);
	char buf[10];
	buf[9] = 123;
	date->pack(buf);
	verify(buf[9] == 123);
	gcstring s = gcstring::noalloc(buf, 9);
	SuDate* date2 = SuDate::unpack(s);
	verify(date->eq(*date2));
}

TEST(sudate_ostream) {
	Value now = new SuDate;
	OstreamStr os;
	os << now;
	Value d = SuDate::parse(os.str());
	assert_eq(now, d);

	gcstring s = "#20031218.151501002";
	Value ds = SuDate::parse(s.str());
	os.clear();
	os << ds;
	assert_eq(s, os.str());
}

TEST(sudate_literal) {
	auto s = "#19990101";
	Value x = SuDate::literal(s);
	OstreamStr os;
	os << x;
	assert_eq(gcstring(s), gcstring(os.str()));

	verify(!SuDate::literal("#200901011"));
	verify(!SuDate::literal("#20090101.1"));
}
