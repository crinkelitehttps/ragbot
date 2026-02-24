#ifndef CONFIGEMBED_H
#define CONFIGEMBED_H

#include <QString>
#include "ConfigGenerator.h"

struct ConfigEmbed {
    QString dbName;
    QString sourceFiles;
    ConfigGenerator generatorConfig;

    bool isValid() { return !dbName.isEmpty() && generatorConfig.isValid(); }
};

#endif // CONFIGEMBED_H
