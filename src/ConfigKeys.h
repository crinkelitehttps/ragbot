#pragma once
#include <QLatin1String>

namespace ConfigKeys {

    // ── Root section keys ─────────────────────────────────────────────────────
    inline const QLatin1String Embedder        { "embedder"        };
    inline const QLatin1String Reranker        { "reranker"        };
    inline const QLatin1String Researcher      { "researcher"      };
    inline const QLatin1String Roleplayer      { "roleplayer"      };
    inline const QLatin1String ConversationsDb { "conversationsDb" };

    // ── Shared subkeys ────────────────────────────────────────────────────────
    inline const QLatin1String Generator { "generator" };
    inline const QLatin1String Enabled   { "enabled"   };

    // ── Embedder ──────────────────────────────────────────────────────────────
    inline const QLatin1String DbName              { "name"                };
    inline const QLatin1String Files               { "files"               };
    inline const QLatin1String ParserType          { "parserType"          };
    inline const QLatin1String TopK                { "topK"                };
    inline const QLatin1String SimilarityThreshold { "similarityThreshold" };
    inline const QLatin1String SkipIndex           { "skipIndex"           };

    // ── Reranker ──────────────────────────────────────────────────────────────
    inline const QLatin1String TopN { "topN" };

    // ── Researcher ────────────────────────────────────────────────────────────
    inline const QLatin1String Instruction { "instruction" };

    // ── Roleplayer ────────────────────────────────────────────────────────────
    inline const QLatin1String CharacterName       { "characterName"       };
    inline const QLatin1String CharacterBackground { "characterBackground" };

    // ── Generator block ───────────────────────────────────────────────────────
    inline const QLatin1String Backend        { "backend"        };
    inline const QLatin1String ModelName      { "modelName"      };
    inline const QLatin1String Timeout        { "timeout"        };
    inline const QLatin1String BasePath       { "basePath"       };
    inline const QLatin1String RemotePath     { "remotePath"     }; // deprecated alias
    inline const QLatin1String ModelPath      { "modelPath"      };
    inline const QLatin1String Temperature    { "temperature"    };
    inline const QLatin1String TopP           { "topP"           };
    inline const QLatin1String RepeatPenalty  { "repeatPenalty"  };
    inline const QLatin1String MaxTokens      { "maxTokens"      };
    inline const QLatin1String EnableThinking { "enableThinking" };

    // ── Backend values ────────────────────────────────────────────────────────
    inline const QLatin1String BackendEmbedded { "embedded" };
    inline const QLatin1String BackendNetwork  { "network"  };

    // ── Parser type values ────────────────────────────────────────────────────
    inline const QLatin1String ParserCddaJson { "cdda_json" };
    inline const QLatin1String ParserManPage  { "man_page"  };

} // namespace ConfigKeys
