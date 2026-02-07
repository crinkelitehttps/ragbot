#ifndef ROLEPLAYLLMCONFIG_H
#define ROLEPLAYLLMCONFIG_H

#include <QString>
#include <QFile>
#include <QIODevice>
#include <QDebug>
#include "LLMConfig.h"

struct ConfigRoleplay {

    bool isValid() { 
        return !characterBackground.isEmpty()
            && !characterBackground.isEmpty() 
            && llmConfig.isValid(); 
    }

    QString characterName = "Survivor";
    QString characterBackground = "PLACEHOLDER BACKGROUND";
    LLMConfig llmConfig;
};

#endif // ROLEPLAYLLMCONFIG_H
