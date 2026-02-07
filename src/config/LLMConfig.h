#ifndef LLMCONFIG_H
#define LLMCONFIG_H

#include <QString>

// Configuration for remote LLM
struct LLMConfig {
    QString baseUrl;
    QString model;
    int timeout;
    bool isValid() { return !baseUrl.isEmpty() && !model.isEmpty(); }
};

#endif // LLMCONFIG_H
