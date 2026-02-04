#ifndef REMOTELLMCONFIG_H
#define REMOTELLMCONFIG_H

#include <QString>

// Configuration for remote LLM
struct RemoteLLMConfig {
    bool enabled = false;
    QString baseUrl = "http://192.168.0.97:8080/upstream/llama-3.2-8b-instruct";
    QString model = "llama-3.2-8b-instruct";
    int timeout = 60000;
};

#endif // REMOTELLMCONFIG_H
