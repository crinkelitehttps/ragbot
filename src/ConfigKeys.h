#pragma once
#include "compat/Types.h"

namespace ConfigKeys {

    // ── Root section keys ─────────────────────────────────────────────────────
    inline const rb::String Embedder        { "embedder"        };
    inline const rb::String Reranker        { "reranker"        };
    inline const rb::String Researcher      { "researcher"      };
    inline const rb::String Roleplayer      { "roleplayer"      };
    inline const rb::String ConversationsDb { "conversationsDb" };

    // ── Shared subkeys ────────────────────────────────────────────────────────
    inline const rb::String Generator { "generator" };
    inline const rb::String Enabled   { "enabled"   };

    // ── Embedder ──────────────────────────────────────────────────────────────
    inline const rb::String DbName              { "name"                };
    inline const rb::String Files               { "files"               };
    inline const rb::String TopK                { "topK"                };
    inline const rb::String SimilarityThreshold { "similarityThreshold" };
    inline const rb::String SkipIndex           { "skipIndex"           };

    // ── Reranker ──────────────────────────────────────────────────────────────
    inline const rb::String TopN { "topN" };

    // ── Researcher ────────────────────────────────────────────────────────────
    inline const rb::String Instruction { "instruction" };

    // ── Roleplayer ────────────────────────────────────────────────────────────
    inline const rb::String CharacterName       { "characterName"       };
    inline const rb::String CharacterBackground { "characterBackground" };
    inline const rb::String AssetsDir           { "assetsDir"           };

    // ── Generator block ───────────────────────────────────────────────────────
    inline const rb::String Backend        { "backend"        };
    inline const rb::String ModelName      { "modelName"      };
    inline const rb::String Timeout        { "timeout"        };
    inline const rb::String BasePath       { "basePath"       };
    inline const rb::String RemotePath     { "remotePath"     }; // deprecated alias
    inline const rb::String ModelPath      { "modelPath"      };
    inline const rb::String Temperature    { "temperature"    };
    inline const rb::String TopP           { "topP"           };
    inline const rb::String RepeatPenalty  { "repeatPenalty"  };
    inline const rb::String MaxTokens      { "maxTokens"      };
    inline const rb::String EnableThinking { "enableThinking" };

    // ── Backend values ────────────────────────────────────────────────────────
    inline const rb::String BackendEmbedded { "embedded" };
    inline const rb::String BackendNetwork  { "network"  };

    // ── Platform values (optional; used with backend=network) ─────────────────
    inline const rb::String Platform           { "platform"    };
    inline const rb::String PlatformVastAi     { "vast.ai"     };
    inline const rb::String PlatformVastAiText { "vast.ai-text" };

} // namespace ConfigKeys
