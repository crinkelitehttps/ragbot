#ifndef CONFIGGENERATOR_H
#define CONFIGGENERATOR_H

#include <QDebug>
#include <QString>

struct ConfigGenerator {
    QString dbName {};
    QString basePath {};
    QString modelName {};
    int timeout { 60000 };
    bool isImmediate {};
    bool isValid() 
    { 
        if (isImmediate) {
            return true;
        }
        return !basePath.isEmpty()
            && !modelName.isEmpty();
    }
};

#endif //CONFIGGENERATOR_H
