#ifndef ROLEPLAYCONFIG_H
#define ROLEPLAYCONFIG_H

#include <QString>
#include <QFile>
#include <QIODevice>
#include <QDebug>

// Configuration for roleplay
struct RoleplayConfig {
    bool enabled = true;
    QString characterName = "Survivor";
    QString characterBackground = "";
    QString baseUrl = "http://192.168.0.97:8080/upstream/llama-3.2-8b-instruct";
    QString model = "llama-3.2-8b-instruct";
    
    bool loadFromFile(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open character background file:" << filePath;
            return false;
        }
        characterBackground = QString::fromUtf8(file.readAll());
        file.close();
        return !characterBackground.isEmpty();
    }
};

#endif // ROLEPLAYCONFIG_H
