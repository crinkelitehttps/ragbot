// Modified to use local llama-swap embedding server
#ifndef ROLEPLAYER_H
#define ROLEPLAYER_H

#include "../config/ConfigRoleplay.h"
#include "../db/EmbeddingDatabase.h"
#include "llama.h"

class Roleplayer
{
public:
    Roleplayer(const ConfigRoleplay &roleplayerConfig) : m_config(roleplayerConfig) {};

private:
    ConfigRoleplay m_config;
    EmbeddingDatabase m_embed_db;
};

#endif // ROLEPLAYER_H
