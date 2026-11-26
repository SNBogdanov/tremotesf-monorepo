// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ansitext.h"

#include <array>

#include <QColor>
#include <QList>
#include <QRegularExpression>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace tremotesf::scripts {
    namespace {
        const QRegularExpression ansiPattern(uR"(\x1B\[([0-9;]*)m)"_s);

        QColor colorForIndex(int index, bool bright) {
            static const std::array<const char*, 8>
                normal{"#000000", "#EF5350", "#66BB6A", "#FFCA28", "#42A5F5", "#AB47BC", "#26C6DA", "#E0E0E0"};
            static const std::array<const char*, 8>
                brightColors{"#9E9E9E", "#FF8A80", "#A5D6A7", "#FFE082", "#90CAF9", "#CE93D8", "#80DEEA", "#FFFFFF"};
            const auto& colors = bright ? brightColors : normal;
            return QColor(QString::fromLatin1(colors[static_cast<size_t>(qBound(0, index, 7))]));
        }

        QColor color256(int index) {
            index = qBound(0, index, 255);
            if (index < 8) {
                return colorForIndex(index, false);
            }
            if (index < 16) {
                return colorForIndex(index - 8, true);
            }
            if (index < 232) {
                const int value = index - 16;
                const int red = value / 36;
                const int green = (value / 6) % 6;
                const int blue = value % 6;
                const auto channel = [](int component) { return component == 0 ? 0 : 55 + component * 40; };
                return QColor(channel(red), channel(green), channel(blue));
            }
            const int gray = 8 + (index - 232) * 10;
            return QColor(gray, gray, gray);
        }

        QString escape(const QString& text) { return text.toHtmlEscaped(); }
    }

    QString ansiToHtml(const QString& text) {
        QString html = "<pre style=\"margin:0; white-space:pre-wrap;\">"_L1;
        QColor color{};
        bool bold = false;
        qsizetype last = 0;

        const auto appendText = [&](const QString& part) {
            if (part.isEmpty()) {
                return;
            }
            QString style{};
            if (color.isValid()) {
                style += "color:"_L1 + color.name() + u';';
            }
            if (bold) {
                style += "font-weight:bold;"_L1;
            }
            if (style.isEmpty()) {
                html += escape(part);
            } else {
                html += "<span style=\""_L1 + style + "\">"_L1 + escape(part) + "</span>"_L1;
            }
        };

        auto it = ansiPattern.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            appendText(text.mid(last, match.capturedStart() - last));

            const auto codeStrings = match.captured(1).split(u';');
            QList<int> codes{};
            for (const auto& codeString : codeStrings) {
                bool ok = false;
                const int code = codeString.toInt(&ok);
                if (ok) {
                    codes.push_back(code);
                }
            }
            if (codes.isEmpty()) {
                codes.push_back(0);
            }

            for (qsizetype i = 0; i < codes.size(); ++i) {
                const int code = codes[i];
                if (code == 0) {
                    color = QColor{};
                    bold = false;
                } else if (code == 1) {
                    bold = true;
                } else if (code == 22) {
                    bold = false;
                } else if (code == 39) {
                    color = QColor{};
                } else if (code >= 30 && code <= 37) {
                    color = colorForIndex(code - 30, false);
                } else if (code >= 90 && code <= 97) {
                    color = colorForIndex(code - 90, true);
                } else if (code == 38 && i + 1 < codes.size()) {
                    if (codes[i + 1] == 5 && i + 2 < codes.size()) {
                        color = color256(codes[i + 2]);
                        i += 2;
                    } else if (codes[i + 1] == 2 && i + 4 < codes.size()) {
                        color = QColor(codes[i + 2], codes[i + 3], codes[i + 4]);
                        i += 4;
                    }
                }
            }
            last = match.capturedEnd();
        }
        appendText(text.mid(last));
        html += "</pre>"_L1;
        return html;
    }
}
