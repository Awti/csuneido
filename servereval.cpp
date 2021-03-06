// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "interp.h"
#include "func.h"
#include "globals.h"
#include "suobject.h"
#include "dbms.h"
#include "sustring.h"

class ServerEval : public Func {
public:
	ServerEval() {
		named.num = globals("ServerEval");
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
};

Value su_ServerEval() {
	return new ServerEval;
}

Value ServerEval::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (nargs < 1)
		except("usage: ServerEval(function_name [, args ...])");
	Value* args = GETSP() - nargs + 1;
	SuObject* ob;
	if (each >= 0) {
		verify(nargs == 1 && nargnames == 0);
		ob = args[0].object()->slice(each);
	} else {
		// create an object from the args
		ob = new SuObject;
		// convert args to members
		short unamed = nargs - nargnames;
		// un-named
		int i;
		for (i = 0; i < unamed; ++i)
			ob->add(args[i]);
		// named
		verify(i >= nargs || argnames);
		for (int j = 0; i < nargs; ++i, ++j)
			ob->put(symbol(argnames[j]), args[i]);
	}
	return dbms()->exec(ob);
}

Value exec(Value x) {
	SuObject* ob = force<SuObject*>(x);
	gcstring fname = ob->get(0).gcstr();
	int i = fname.find('.');
	gcstring g;
	Value m;
	if (i == -1) {
		g = fname;
		m = CALL;
	} else {
		g = fname.substr(0, i);
		m = Value(fname.substr(i + 1).str());
	}
	Value f = globals[g.str()];
	KEEPSP
	PUSH(ob);
	return f.call(f, m, 1, 0, 0, 1); // nargs=1, each=1 => f(@+1 ob)
}
