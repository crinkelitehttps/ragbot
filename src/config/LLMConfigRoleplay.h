#ifndef ROLEPLAYLLMCONFIG_H
#define ROLEPLAYLLMCONFIG_H

#include <QString>
#include <QFile>
#include <QIODevice>
#include <QDebug>
#include "LLMConfig.h"

// Configuration for roleplay
struct LLMConfigRoleplay: LLMConfig {

    QString characterName = "Survivor";
    QString characterBackground = "";
    
    bool loadFromFile(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "LLMConfigRolePlay Failed to open character background file:" << filePath;
            return false;
        }
        characterBackground = QString::fromUtf8(file.readAll());
        file.close();
        return !characterBackground.isEmpty();
    }
};

#endif // ROLEPLAYLLMCONFIG_H
