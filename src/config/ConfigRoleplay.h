#ifndef ROLEPLAYLLMCONFIG_H
#define ROLEPLAYLLMCONFIG_H

#include <QString>
#include "ConfigGenerator.h"

struct ConfigRoleplay {
    QString characterName = "Survivor";
    QString characterBackground = "PLACEHOLDER BACKGROUND";
    ConfigGenerator generatorConfig;
    bool isValid() { return !characterName.isEmpty() && !characterBackground.isEmpty(); }
};

#endif // ROLEPLAYLLMCONFIG_H
