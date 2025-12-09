#include "LogHighlighter.hpp"

#include "utils/QvHelpers.hpp"

#include <qnamespace.h>
#include <qregularexpression.h>

#define TO_EOL "(([\\s\\S]*)|([\\d\\D]*)|([\\w\\W]*))$"

namespace Qv2ray::ui
{
    SyntaxHighlighter::SyntaxHighlighter(bool darkMode, QTextDocument *parent) : QSyntaxHighlighter(parent)
    {
        setupUi(darkMode);
        setupRule();
    }

    void SyntaxHighlighter::setDarkMode(bool darkMode)
    {
        setupUi(darkMode);
        setupRule();
        rehighlight();
    }

    void SyntaxHighlighter::setupUi(bool darkMode)
    {
        const QColor darkGreenColor(10, 180, 0);

        tcpudpFormat.setFontWeight(QFont::Bold);

        debugFormat.setForeground(Qt::darkGray);

        warningFormat.setFontWeight(QFont::Bold);

        acceptedFormat.setForeground(darkGreenColor);
        acceptedFormat.setFontItalic(true);
        acceptedFormat.setFontWeight(QFont::Bold);

        rejectedFormat.setFontWeight(QFont::Bold);
        rejectedFormat.setBackground(Qt::red);
        rejectedFormat.setForeground(Qt::white);
        rejectedFormat.setFontItalic(true);
        rejectedFormat.setFontWeight(QFont::Bold);

        failedFormat.setFontWeight(QFont::Bold);
        failedFormat.setBackground(Qt::red);
        failedFormat.setForeground(Qt::white);

        if (darkMode)
        {
            tcpudpFormat.setForeground(QColor(0, 200, 230));
            ipHostFormat.setForeground(Qt::yellow);
            warningFormat.setForeground(QColor(255, 160, 15));
            dateFormat.setForeground(Qt::cyan);
            timeFormat.setForeground(Qt::cyan);
            infoFormat.setForeground(Qt::lightGray);
            v2rayComponentFormat.setForeground(darkGreenColor);
            qvAppLogFormat.setForeground(Qt::cyan);
            qvAppDebugLogFormat.setForeground(Qt::yellow);
        }
        else
        {
            ipHostFormat.setForeground(Qt::black);
            ipHostFormat.setFontWeight(QFont::Bold);
            warningFormat.setForeground(Qt::white);
            tcpudpFormat.setForeground(QColor(0, 52, 130));
            warningFormat.setBackground(QColor(255, 160, 15));
            dateFormat.setForeground(Qt::darkCyan);
            timeFormat.setForeground(Qt::darkCyan);
            infoFormat.setForeground(Qt::darkCyan);
            v2rayComponentFormat.setForeground(Qt::darkYellow);
            qvAppLogFormat.setForeground(Qt::darkCyan);
            qvAppDebugLogFormat.setForeground(Qt::darkYellow);
        }
    }

    void SyntaxHighlighter::setupRule()
    {
        highlightingRules.emplaceBack(QRegularExpression("tcp"), tcpudpFormat);
        highlightingRules.emplaceBack(QRegularExpression("udp"), tcpudpFormat);
        highlightingRules.emplaceBack(QRegularExpression("\\d\\d\\d\\d/\\d\\d/\\d\\d"), dateFormat);
        highlightingRules.emplaceBack(QRegularExpression("\\d\\d:\\d\\d:\\d\\d"), timeFormat);
        highlightingRules.emplaceBack(QRegularExpression("\\[[Dd]ebug\\]" TO_EOL), debugFormat);
        highlightingRules.emplaceBack(QRegularExpression("\\[[Ii]nfo\\]" TO_EOL), infoFormat);
        // IP IPv6 Host;
        highlightingRules.emplaceBack(QRegularExpression(REGEX_IPV4_ADDR ":" REGEX_PORT_NUMBER, QRegularExpression::ExtendedPatternSyntaxOption),
                                      ipHostFormat);
        highlightingRules.emplaceBack(QRegularExpression(REGEX_IPV6_ADDR ":" REGEX_PORT_NUMBER, QRegularExpression::ExtendedPatternSyntaxOption),
                                      ipHostFormat);
        highlightingRules.emplaceBack(QRegularExpression("([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}(/|):" REGEX_PORT_NUMBER,
                                                         QRegularExpression::PatternOption::ExtendedPatternSyntaxOption),
                                      ipHostFormat);
        //
        highlightingRules.emplaceBack(QRegularExpression("\\saccepted\\s"), acceptedFormat);
        highlightingRules.emplaceBack(QRegularExpression("\\srejected\\s" TO_EOL), rejectedFormat);
        highlightingRules.emplaceBack(QRegularExpression(QRegularExpression(R"( (\w+\/)+\w+: )")), v2rayComponentFormat);
        highlightingRules.emplaceBack(QRegularExpression(QRegularExpression("\\[[Ww]arning\\]" TO_EOL)), warningFormat);
        highlightingRules.emplaceBack(QRegularExpression(QRegularExpression("failed")), failedFormat);
        highlightingRules.emplaceBack(QRegularExpression(QRegularExpression("\\[[A-Z]*\\]:")), qvAppLogFormat);
        highlightingRules.emplaceBack(QRegularExpression(QRegularExpression(R"( \[\w+\] )")), qvAppDebugLogFormat);
    }

    void SyntaxHighlighter::highlightBlock(const QString &text)
    {
        for (const HighlightingRule &rule : std::as_const(highlightingRules))
        {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);

            while (matchIterator.hasNext())
            {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }

        setCurrentBlockState(0);
    }
} // namespace Qv2ray::ui
