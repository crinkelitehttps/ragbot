#ifndef CONVERSATIONTURN_H
#define CONVERSATIONTURN_H

#include "compat/Types.h"

struct ConversationTurn {
    rb::String question;
    rb::String researchAnswer;
    rb::String roleplayAnswer;
};

#endif // CONVERSATIONTURN_H
