#ifndef LLMCONFIG_H
#define LLMCONFIG_H

#include <QString>

// TODO fix these defaults
// Configuration for remote LLM
struct LLMConfig {
    bool enabled = false;
    QString baseUrl = "http://192.168.0.97:8080/upstream/llama-3.2-8b-instruct";
    QString model = "llama-3.2-8b-instruct";
    int timeout = 60000;
};

#endif // LLMCONFIG_H
