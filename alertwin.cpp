// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "alertout.h"
#include "win.h"

void alertout(char* s) {
	MessageBox(nullptr, s, "Suneido", MB_TASKMODAL);
}
