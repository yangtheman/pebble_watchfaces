/* FFI GENERATED FILE; DO NOT EDIT! */

#include "xsffi.h"

txAPI* XS = NULL;

extern int32_t getSteps();

static void xs_getSteps(txMachine* the) {
	int32_t result = getSteps();
	XS->fromInteger(the, mxResult, (txInteger)result);
}

void fxBuildFFI(txMachine* the, txAPI* api) {
	XS = api;
	XS->newHostFunction(the, xs_getSteps, 0, 0, 0);
	XS->push(the, mxThis);
	XS->defineID(the, XS->id(the, "getSteps"), 0, 0x0E);
	XS->pop(the);
}
