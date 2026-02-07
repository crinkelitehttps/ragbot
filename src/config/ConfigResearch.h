#ifndef CONFIGRESEARCH_H
#define CONFIGRESEARCH_H

#include <QString>
#include <QDebug>
#include "LLMConfig.h"

struct ConfigResearch {
    QString instruction = "PLACEHOLDER_RESEARCH_INTRUCTION";
    LLMConfig llmConfig;
};

#endif //CONFIGRESEARCH_H 
