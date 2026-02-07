#ifndef CONFIGEMBED_H
#define CONFIGEMBED_H

#include <QString>
#include "LLMConfig.h"

struct ConfigEmbed {
    QString dbName;
    LLMConfig llmConfig;
    bool isValid() { return dbName.isEmpty() && llmConfig.isValid(); }
};

#endif // CONFIGEMBED_H
