#ifndef GENERATOR_H
#define GENERATOR_H
#include "config/ConfigGenerator.h"

class Generator 
{
public:
    virtual ~Generator() = default;
    virtual void hello() = 0;
    virtual bool isValid() = 0;
    virtual bool isImmediate() = 0;
protected:
    Generator(ConfigGenerator generatorConfig) {}
};

#endif // GENERATOR_H
