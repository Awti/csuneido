// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#define _CRT_NONSTDC_NO_WARNINGS

#include "dll.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "ostreamfile.h"
#include "win.h"
#include "trace.h"
#include <cstring> // for _stricmp

#ifdef __GNUC__
#define _stricmp stricmp
#endif

struct WinLib {
	WinLib() {
	}
	explicit WinLib(char* s) : name(dupstr(s)), lib(LoadLibrary(name)) {
	}
	~WinLib() {
		if (lib)
			FreeLibrary(lib);
	}
	void* GetProcAddress(char* procname) const {
		return (void*) ::GetProcAddress(lib, procname);
	}
	explicit operator bool() const {
		return lib;
	}

	char* name = nullptr;
	HMODULE lib = nullptr;
};

static WinLib& loadlib(char* name) {
	const int NLIBS = 30;
	static WinLib libs[NLIBS]; // TODO change to vector (or hash map)
	static int nlibs = 0;
	int i = 0;
	for (; i < nlibs; ++i)
		if (0 == _stricmp(libs[i].name, name))
			return libs[i];
	if (nlibs >= NLIBS)
		except("can't load " << name << " - too many dll's loaded");
	new (libs + nlibs++) WinLib(name);
	return libs[i];
}

static OstreamFile& log() {
	static OstreamFile ofs("dll.log");
	return ofs;
}

Dll::Dll(short rt, char* library, char* name, TypeItem* p, short* ns, short n)
	: params(p, n), rtype(rt), trace(false) {
	nparams = n;
	dll = globals(library);
	fn = globals(name);
	locals = dup(ns, nparams);

	WinLib& hlibrary = loadlib(library);
	if (!hlibrary)
		except("can't LoadLibrary " << library);
	if (0 == (pfn = hlibrary.GetProcAddress(name))) {
		char uname[64];
		verify(strlen(name) < 63);
		strcpy(uname, name);
		strcat(uname, "A");
		if (0 == (pfn = hlibrary.GetProcAddress(uname)))
			except("can't get " << library << ":" << name << " or " << uname);
	}
	// log() << this << endl;
}

void Dll::out(Ostream& os) const {
	if (named.num)
		os << named.name() << " /* " << named.lib << " dll " << pfn << " */";
	else {
		os << "dll " << (rtype ? globals(rtype) : "void") << " " << globals(dll)
		   << ":" << globals(fn);
		os << '(';
		for (int i = 0; i < nparams; ++i) {
			if (i > 0)
				os << ", ";
			params.items[i].out(os);
			verify(locals);
			os << " " << symstr(locals[i]);
		}
		os << ')';
	}
}

const int maxbuf = 1024;

Value Dll::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	static Value Trace("Trace");
	if (member == Trace) {
		trace = true;
		return Value();
	}
	if (member != CALL)
		return Func::call(self, member, nargs, nargnames, argnames, each);
	args(nargs, nargnames, argnames, each);
	Framer frame(this, self);

	char buf[maxbuf];
	char* dst = buf;
	const int64_t MARKER = 0xaa55aa55aa55aa55;
	char buf2[32000]; // for stuff passed by pointer
	char* dst2 = buf2;
	char* lim2 = buf2 + sizeof buf2 - sizeof(MARKER);

	const int params_size = params.size();
	if (params_size > maxbuf)
		except("dll arguments too big");
	Value* args = GETSP() - nparams + 1;
	params.putall(dst, dst2, lim2, args);
	verify(dst == buf + params_size);
	*((int64_t*) dst2) = MARKER;

	if (trace) {
		log() << named.name() << endl << "    ";
		for (void** p = (void**) buf; p < (void**) dst; ++p)
			log() << *p << " ";
		log() << endl << "    ";
		for (char* s = buf2; s < dst2; ++s)
			log() << hex << (uint32_t)(uint8_t) *s << dec << " ";
		log() << endl;
	}

	int save_trace_level = trace_level;
	TRACE_CLEAR();

	int result, result2;
	int nlongs = (dst - buf) / 4;
#ifdef _MSC_VER
	if (nlongs)
		__asm {
			mov ecx, nlongs
			mov ebx, dst
	NEXT:	sub ebx,4
			push dword ptr [ebx]
			loop NEXT
		}
		// ReSharper disable once CppDeclaratorNeverUsed
		uint32_t f = (uint32_t) pfn;
	__asm
	{
		mov eax,f
		call eax
		mov result,eax
		mov result2,edx
	}
#elif defined(__GNUC__)
	int dummy;
	if (nlongs)
		asm volatile("1:\n\t"
					 "sub $4, %%ebx\n\t"
					 "pushl (%%ebx)\n\t"
					 "loop 1b\n\t"
					 : "=b"(dummy)           // outputs
					 : "c"(nlongs), "b"(dst) // inputs
		);
	asm volatile("call *%%eax\n\t"
				 : "=a"(result), "=d"(result2) // outputs
				 : "a"(pfn)                    // inputs
				 : "memory"                    // clobbers
	);
	// WARNING: should be more clobbers but not sure what???
#else
#warning "replacement for inline assembler required"
#endif

	verify(*((int64_t*) dst2) == MARKER);

	trace_level = save_trace_level;

	if (trace) {
		log() << "    => " << hex << result << dec << endl;
		for (char* s = buf2; s < dst2; ++s)
			log() << hex << (uint32_t)(uint8_t) *s << dec << " ";
	}

	// update SuObject args passed by pointer
	const char* src = buf;
	params.getall(src, args);

	return rtype ? force<Type*>(globals[rtype])->result(result2, result)
				 : Value();
}
