#ifndef GENERATORIMMEDIATE_H
#define GENERATORIMMEDIATE_H
#include <QByteArray>

#include "Generator.h"
#include "config/ConfigGenerator.h"
#include <QDir>

#include "llama.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "common.h"
#pragma GCC diagnostic pop

class GeneratorImmediate : public virtual Generator 
{
public:
    GeneratorImmediate(ConfigGenerator generatorConfig);

    void hello() override;
    bool isValid() override ( return generatorConfig.);
    
    llama_context *m_embedCtx;
    llama_model *m_embedModel;
};

#endif // GENERATORIMMEDIATE_H
