// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "cmdlineoptions.h"
#include <cstdlib>
#include <cctype>
#include "alert.h"
#include "except.h"
#include "gcstring.h"
#include "errlog.h"

extern int su_port;
extern int dbserver_timeout;
extern bool is_client;

enum {
	PORT = AFTER_ACTIONS,
	UNATTENDED,
	LOCAL_LIBRARY,
	NO_EXCEPTION_HANDLING,
	NO_GARBAGE_COLLECTION,
	INSTALL_SERVICE,
	SERVICE,
	CHECK_START,
	COMPACT_EXIT,
	IGNORE_VERSION,
	IGNORE_CHECK,
	TIMEOUT,
	END_OF_OPTIONS
};

const char* CmdLineOptions::parse(const char* str) {
	char* end = nullptr; // used for strtol
	const char* error = nullptr;

	s = str;
	while (int opt = get_option()) {
		switch (opt) {
		// actions
		case DUMP:
		case LOAD:
			if (nullptr != (argstr = get_word()))
				argstr = strip_su(argstr);
			else
				argstr = "";
			break;
		case TEST:
		case BENCH:
			if (nullptr == (argstr = get_word()))
				argstr = ""; // all
			break;
		case SERVER:
			if (*s == '=') {
				++s;
				argstr = get_word();
			} else
				argstr = "Suneido";
			unattended = true;
			break;
		case CLIENT:
			if (nullptr == (argstr = get_word()))
				argstr = "127.0.0.1";
			break;
		case REPL:
			action = REPL;
			break;
		case INSTALL_SERVICE:
			install = s;
			break;
		case SERVICE:
			service = s;
			break;
		case CHECK:
		case REBUILD:
		case DBDUMP:
		case COMPACT:
		case VERSION:
		case UNINSTALL_SERVICE:
			break;
		// options
		case PORT:
			su_port = strtol(s, &end, 10);
			s = end;
			break;
		case UNATTENDED:
			unattended = true;
			break;
		case LOCAL_LIBRARY:
			local_library = true;
			break;
		case NO_EXCEPTION_HANDLING:
			no_exception_handling = true;
			break;
		case NO_GARBAGE_COLLECTION:
			break;
		case CHECK_START:
			check_start = true;
			break;
		case IGNORE_CHECK:
			ignore_check = true;
			break;
		case COMPACT_EXIT:
			compact_exit = true;
			break;
		case IGNORE_VERSION:
			ignore_version = true;
			break;
		case TIMEOUT: {
			int minutes = strtol(s, &end, 10);
			s = end;
			if (minutes > 0)
				dbserver_timeout = minutes;
			break;
		}
		case HELP:
			alert("options:\n"
				  "	-check\n"
				  "	-c[heck]s[tart]\n"
				  "	-r[ebuild]\n"
				  "	-compact\n"
				  "	-c[compact]e[xit]\n"
				  "	-d[ump] [tablename]\n"
				  "	-l[oad] [tablename]\n"
				  "	-t[est] [prefix]\n"
				  "	-b[ench] [prefix]\n"
				  "	-s[erver][=name]\n"
				  "	-c[lient] ipaddress\n"
				  "	-p[ort] #\n"
				  "	-u[nattended]\n"
				  "	-v[ersion]\n"
				  "	-l[ocal]l[ibrary]\n"
				  "	-e[xception]h[andling]\n"
				  "	-g[arbage]c[ollection]\n"
				  "	-i[nstall]s[ervice] [options]\n"
				  "	-u[ninstall]s[ervice]\n"
				  "	-t[ime]o[ut] minutes\n");
			exit(EXIT_SUCCESS);
		case END_OF_OPTIONS:
			break;
		default:
			unreachable();
		}
	}
	if (error) {
		if (action == CLIENT)
			is_client = true; // so errlog goes to the right place
		errlog(error);
	}
	return s;
}

// Note: must put options that are a prefix of other options last e.g. -check
// before -c
static struct {
	const char* str;
	int num;
} options[] = {
	{"-bench", BENCH},
	{"-b", BENCH},
	{"-dbdump", DBDUMP},
	{"-dump", DUMP},
	{"-d", DUMP},
	{"-locallibrary", LOCAL_LIBRARY},
	{"-ll", LOCAL_LIBRARY},
	{"-load", LOAD},
	{"-l", LOAD},
	{"-service", SERVICE},
	{"-server", SERVER},
	{"-s", SERVER},
	{"-compact", COMPACT},
	{"-compactexit", COMPACT_EXIT},
	{"-ce", COMPACT_EXIT},
	{"-check", CHECK},
	{"-checkstart", CHECK_START},
	{"-cs", CHECK_START},
	{"-client", CLIENT},
	{"-c", CLIENT},
	{"-eh", NO_EXCEPTION_HANDLING},
	{"-exceptionhandling", NO_EXCEPTION_HANDLING},
	{"-gc", NO_GARBAGE_COLLECTION},
	{"-garbagecollection", NO_GARBAGE_COLLECTION},
	{"-repl", REPL},
	{"-rebuild", REBUILD},
	{"-r", REBUILD},
	{"-timeout", TIMEOUT},
	{"-to", TIMEOUT},
	{"-test", TEST},
	{"-t", TEST},
	{"-port", PORT},
	{"-p", PORT},
	{"-help", HELP},
	{"-?", HELP},
	{"-h", HELP},
	{"-version", VERSION},
	{"-v", VERSION},
	{"-is", INSTALL_SERVICE},
	{"-installservice", INSTALL_SERVICE},
	{"-us", UNINSTALL_SERVICE},
	{"-uninstallservice", UNINSTALL_SERVICE},
	{"-unattended", UNATTENDED},
	{"-u", UNATTENDED},
	{"-ignoreversion", IGNORE_VERSION},
	{"-iv", IGNORE_VERSION},
	{"-ignorecheck", IGNORE_CHECK},
	{"--", END_OF_OPTIONS},
};

int CmdLineOptions::get_option() {
	skip_white();
	for (auto& option : options)
		if (has_prefix(s, option.str)) {
			s += strlen(option.str);
			if (option.num < AFTER_ACTIONS)
				set_action(option.num);
			return option.num;
		}
	return 0;
}

void CmdLineOptions::skip_white() {
	while (*s && isspace(*s))
		++s;
}

void CmdLineOptions::set_action(int a) {
	if (action)
		except("Please specify only one of: -dump, -load, -server, -client, "
			   "-check, -rebuild, -test, -bench, -installserver, "
			   "-uninstallserver");
	action = a;
}

const char* CmdLineOptions::get_word() {
	skip_white();
	if (*s == 0 || *s == '-' || *s == '+' || isspace(*s))
		return nullptr;
	auto start = s;
	while (*s && !isspace(*s))
		++s;
	int n = s - start;
	char* buf = salloc(n);
	memcpy(buf, start, n);
	buf[n] = 0;
	return buf;
}

const char* CmdLineOptions::strip_su(const char* file) {
	if (!has_suffix(file, ".su"))
		return file;
	char* s = dupstr(file);
	s[strlen(file) - 3] = 0;
	return s;
}
