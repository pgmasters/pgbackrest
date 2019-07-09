/***********************************************************************************************************************************
Command Control
***********************************************************************************************************************************/
#ifndef COMMAND_CONTROL_CONTROL_H
#define COMMAND_CONTROL_CONTROL_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
String *lockStopFileName(const String *stanza);
bool lockStopTest(bool stanzaStopExpected);

#endif
