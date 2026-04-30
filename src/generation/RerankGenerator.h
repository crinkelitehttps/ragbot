#ifndef RERANKGENERATOR_H
#define RERANKGENERATOR_H

#include "../compat/Types.h"

class RerankGenerator
{
public:
    virtual ~RerankGenerator() = default;
    virtual auto score(const rb::String& query,
                       const rb::Vector<rb::String>& documents) -> rb::Vector<float> = 0;
    virtual auto isValid() const -> bool = 0;
};

#endif // RERANKGENERATOR_H
