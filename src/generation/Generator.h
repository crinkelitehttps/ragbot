#ifndef GENERATOR_H
#define GENERATOR_H
#include <QByteArray>
#include "../config/ConfigGenerator.h"

class Generator 
{
public:
    virtual ~Generator() = default;
    virtual bool isValid() = 0;
    virtual QByteArray generate(QByteArray question) = 0;

protected:
    Generator(ConfigGenerator generatorConfig) {}
};

#endif // GENERATOR_H
