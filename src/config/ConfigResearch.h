#ifndef CONFIGRESEARCH_H
#define CONFIGRESEARCH_H

#include <QString>
#include <QDebug>
#include "ConfigGenerator.h"

struct ConfigResearch {
    QString instruction = "PLACEHOLDER_RESEARCH_INTRUCTION";
    ConfigGenerator generatorConfig;
};

#endif //CONFIGRESEARCH_H 
