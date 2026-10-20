#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QTextDocumentFragment>
#include <QRegularExpression>
#include <functional>
#include <algorithm>

class LyricFormatter : public QObject {
public:
  explicit LyricFormatter(QObject *parent) : QObject(parent), net(this) {}
  QString swahiliText;
  static QStringList words(QString text) {
    text = QTextDocumentFragment::fromHtml(text).toPlainText();
    QStringList tokens;
    auto matches = QRegularExpression("[\\p{L}\\p{N}]+", QRegularExpression::UseUnicodePropertiesOption).globalMatch(text.toCaseFolded());
    while (matches.hasNext()) tokens << matches.next().captured();
    std::sort(tokens.begin(), tokens.end());
    return tokens;
  }
  static bool keepsPhrases(const QString &source, const QString &formatted) {
    // Reject fragmented AI output even when its word inventory is correct.
    const auto lines = [](const QString &value) {
      return value.split('\n', Qt::SkipEmptyParts);
    };
    const int before = lines(source).size(), after = lines(formatted).size();
    return after <= qMax(before * 2, 4);
  }
  static QString shortSections(const QString &text) {
    QStringList sections;
    for (const auto &block : text.split(QRegularExpression("\\n[ \\t]*\\n"), Qt::SkipEmptyParts)) {
      QStringList pair;
      for (const auto &line : block.split('\n', Qt::SkipEmptyParts)) {
        pair << line.trimmed();
        if (pair.size() == 2) { sections << pair.join('\n'); pair.clear(); }
      }
      if (!pair.isEmpty()) sections << pair.join('\n');
    }
    return sections.join("\n\n");
  }
  void format(const QString &source, std::function<void(QString, QString)> done, bool fallback = false) {
    const QString key = QSettings().value("AI/GroqApiKey").toString();
    const bool groq = !fallback && !key.isEmpty();
    const QString prompt = QStringLiteral(R"PROMPT(Format the existing song for projection. Detect paired languages, especially Swahili and English. Keep each complete lyric phrase on one line. In lyrics, put each Swahili line immediately followed by its existing English translation, then a blank line between translation pairs. NEVER collect Swahili and English into separate language blocks. Preserve the order of the Swahili lines and match each to its supplied translation. Keep original stanza boundaries, headings, repeat instructions and every supplied word and repetition. Never split phrases into isolated words, translate, complete, invent or omit lyrics. Decode HTML entities and remove markdown asterisks, backslash line breaks and parentheses wrapping translations only.
For swahili, remove the English translations and group related Swahili lyric lines into two-line sections separated by blank lines. If no Swahili exists, keep the supplied language in both fields without inventing Swahili. For single-language lyrics, group complete lyric lines into short two-line sections.
Example bilingual lyrics layout:
Zaidi, zaidi
More, more

Nimfahamu Yesu
More about Jesus

Nijue pendo lake na
More of His love and

Wokovu wake kamili
More of His saving fullness see

Return ONLY JSON {"lyrics":"formatted text","swahili":"Swahili only text"}.
Lyrics:
)PROMPT") + source;
    QNetworkRequest req{QUrl(groq ? "https://api.groq.com/openai/v1/chat/completions" : "https://abytrivia.pythonanywhere.com/api/ai/chat")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json"); req.setTransferTimeout(60000);
    QJsonObject body;
    if (groq) {
      req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
      body = {{"model", "openai/gpt-oss-120b"}, {"messages", QJsonArray{QJsonObject{{"role","user"},{"content",prompt}}}}, {"max_tokens", 6000}};
    } else body = {{"message",prompt},{"history",QJsonArray{}}};
    body["temperature"] = 0;
    auto *reply = net.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [=] {
      const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
      const bool failed = reply->error() != QNetworkReply::NoError; reply->deleteLater();
      QString response = groq ? obj["choices"].toArray().first().toObject()["message"].toObject()["content"].toString() : obj["response"].toString();
      if (failed || response.isEmpty()) {
        if (groq) { format(source, done, true); return; }
        done({}, "AI formatting unavailable. Your lyrics were kept."); return;
      }
      const int first = response.indexOf('{'), last = response.lastIndexOf('}');
      const QString text = QJsonDocument::fromJson(response.mid(first,last-first+1).toUtf8()).object()["lyrics"].toString().trimmed();
      if (text.isEmpty() || words(source) != words(text) || !keepsPhrases(source, text)) {
        done({}, "AI changed words or fragmented the lyrics. Your original lyrics were kept."); return;
      }
      const QString sw = QJsonDocument::fromJson(response.mid(first,last-first+1).toUtf8()).object()["swahili"].toString().trimmed();
      const auto allWords = words(text), swWords = words(sw);
      if (sw.isEmpty() || !std::includes(allWords.begin(), allWords.end(), swWords.begin(), swWords.end())) {
        done({}, "AI could not safely separate the languages. Your lyrics were kept."); return;
      }
      swahiliText = shortSections(sw);
      done(shortSections(text), {});
    });
  }
private:
  QNetworkAccessManager net;
};
