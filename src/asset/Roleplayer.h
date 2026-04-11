// Modified to use local llama-swap embedding server
#ifndef ROLEPLAYER_H
#define ROLEPLAYER_H

#include <QJsonObject>

class Roleplayer
{
public:
    Roleplayer(const QJsonObject& config) 
    {
        Q_UNUSED(config)
    };
};

#endif // ROLEPLAYER_H
