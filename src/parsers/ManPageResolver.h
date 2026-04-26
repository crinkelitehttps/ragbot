#ifndef MANPAGERESOLVER_H
#define MANPAGERESOLVER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include "Parser.h"

class ManPageResolver
{
public:
    // Discover all man page files (*.1 … *.8, *.1.gz … *.8.gz) under dirs.
    static auto discover(const QStringList& dirs) -> QStringList;

    // Render a single man page file to plain ASCII via groff.
    // Handles .gz automatically. Returns empty string on failure.
    static auto renderToText(const QString& filePath) -> QString;

    // Split rendered text into sections; returns {sectionName, sectionBody} pairs.
    static auto splitSections(const QString& text)
        -> QVector<QPair<QString, QString>>;

    // Full pipeline: render → split → produce one Chunk per section.
    // Long sections (>4000 chars) are further split on blank lines.
    static auto fileToChunks(const QString& filePath) -> QVector<Parser::Chunk>;

private:
    static constexpr int MaxSectionChars { 4000 };
};

#endif // MANPAGERESOLVER_H
