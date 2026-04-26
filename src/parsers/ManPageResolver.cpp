#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QDebug>
#include "ManPageResolver.h"


//--------------------------------------------------------------------------------
auto ManPageResolver::discover(const QStringList& dirs) -> QStringList
{
    static const QStringList filters {
        "*.1",    "*.2",    "*.3",    "*.4",    "*.5",
        "*.6",    "*.7",    "*.8",    "*.9",
        "*.1.gz", "*.2.gz", "*.3.gz", "*.4.gz", "*.5.gz",
        "*.6.gz", "*.7.gz", "*.8.gz", "*.9.gz",
    };

    QStringList paths;
    for (const QString& dir : dirs) {
        QDirIterator it(dir, filters, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            paths << it.next();
    }
    return paths;
}


//--------------------------------------------------------------------------------
auto ManPageResolver::renderToText(const QString& filePath) -> QString
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);

    if (filePath.endsWith(".gz")) {
        const QString quoted = "'" + QString(filePath).replace("'", "'\\''") + "'";
        const QString cmd = "gunzip -c " + quoted + " | groff -T ascii -man - 2>/dev/null";
        proc.start("sh", QStringList() << "-c" << cmd);
    } else {
        proc.start("groff", QStringList() << "-T" << "ascii" << "-man" << filePath);
    }

    if (!proc.waitForStarted(5000)) {
        qCritical() << "ManPageResolver: failed to start groff — is it installed? (pacman -S groff)";
        return {};
    }
    if (!proc.waitForFinished(30000)) {
        qWarning() << "ManPageResolver::renderToText(): groff timed out for" << filePath;
        proc.kill();
        return {};
    }

    QString text = QString::fromLocal8Bit(proc.readAllStandardOutput());

    // Strip overstrike bold/underline: groff -T ascii emits char\bchar sequences.
    static const QRegularExpression overstrikeRe(R"(.\x08)");
    text.remove(overstrikeRe);

    return text;
}


//--------------------------------------------------------------------------------
auto ManPageResolver::splitSections(const QString& text)
    -> QVector<QPair<QString, QString>>
{
    // Section headers are lines containing only uppercase letters, digits, spaces,
    // underscores, and hyphens — and at least 2 characters long — at column 0.
    static const QRegularExpression headerRe(R"(^([A-Z][A-Z0-9 _\-]{1,})$)");

    QVector<QPair<QString, QString>> sections;
    QString currentHeader;
    QStringList currentLines;

    for (const QString& line : text.split('\n')) {
        const QRegularExpressionMatch m = headerRe.match(line);
        if (m.hasMatch()) {
            if (!currentHeader.isEmpty() || !currentLines.isEmpty()) {
                sections.append({currentHeader, currentLines.join('\n').trimmed()});
            }
            currentHeader = m.captured(1).trimmed();
            currentLines.clear();
        } else {
            currentLines << line;
        }
    }
    if (!currentHeader.isEmpty() || !currentLines.isEmpty())
        sections.append({currentHeader, currentLines.join('\n').trimmed()});

    return sections;
}


//--------------------------------------------------------------------------------
auto ManPageResolver::fileToChunks(const QString& filePath) -> QVector<Parser::Chunk>
{
    const QString text = renderToText(filePath);
    if (text.isEmpty()) {
        qWarning() << "ManPageResolver::fileToChunks(): empty render for" << filePath;
        return {};
    }

    // Derive a human-readable page name from the file path.
    QString baseName = QFileInfo(filePath).fileName();
    // Strip known suffixes: .1.gz → strip .gz first, then .1
    if (baseName.endsWith(".gz"))
        baseName.chop(3);
    const int dot = baseName.lastIndexOf('.');
    if (dot > 0)
        baseName = baseName.left(dot);

    const auto sections = splitSections(text);

    QVector<Parser::Chunk> chunks;
    for (const auto& [header, body] : sections) {
        if (body.length() < 30)
            continue; // skip near-empty sections

        const QString label = header.isEmpty() ? baseName : baseName + " " + header;

        if (body.length() <= MaxSectionChars) {
            Parser::Chunk chunk;
            chunk.embedText = label + ": " + body.simplified();
            chunk.content   = label + "\n\n" + body;
            chunks << chunk;
        } else {
            // Split long sections on blank lines into sub-chunks.
            const QStringList paragraphs = body.split(QRegularExpression(R"(\n{2,})"),
                                                       Qt::SkipEmptyParts);
            QString accum;
            int subIdx = 1;
            for (const QString& para : paragraphs) {
                if (!accum.isEmpty() && accum.length() + para.length() > MaxSectionChars) {
                    const QString subLabel = label + " (" + QString::number(subIdx++) + ")";
                    Parser::Chunk chunk;
                    chunk.embedText = subLabel + ": " + accum.simplified();
                    chunk.content   = subLabel + "\n\n" + accum.trimmed();
                    chunks << chunk;
                    accum.clear();
                }
                if (!accum.isEmpty()) accum += "\n\n";
                accum += para;
            }
            if (!accum.trimmed().isEmpty()) {
                const QString subLabel = subIdx > 1
                    ? label + " (" + QString::number(subIdx) + ")"
                    : label;
                Parser::Chunk chunk;
                chunk.embedText = subLabel + ": " + accum.simplified();
                chunk.content   = subLabel + "\n\n" + accum.trimmed();
                chunks << chunk;
            }
        }
    }

    return chunks;
}
