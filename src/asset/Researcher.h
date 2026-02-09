// Modified to use local llama-swap embedding server
#ifndef RESEARCHER_H
#define RESEARCHER_H

#include "../config/ConfigResearch.h"
#include "../db/EmbeddingDatabase.h"

class Researcher
{
public:
    Researcher(const ConfigResearch &researcherConfig)
       : m_config(researcherConfig) 
    {
        qDebug() << "Research()";
    };

private:
    ConfigResearch m_config;
};

#endif // RESEARCHER_H
