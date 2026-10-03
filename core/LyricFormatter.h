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
  void format(const QString &source, std::function<void(QString, QString)> done, bool fallback = false) {
    const QString key = QSettings().value("AI/GroqApiKey").toString();
    const bool groq = !fallback && !key.isEmpty();
    const QString prompt = "Format these existing lyrics. Detect paired languages (especially Swahili and English). Put each original line on one line, its existing translation immediately below, then a blank line between pairs. Keep one bilingual song. Preserve every supplied word, section heading, repeat instruction and occurrence exactly; do not translate, complete, invent or remove lyrics. Decode HTML entities and remove markdown asterisks, backslash line breaks and parentheses wrapping translations only. Single-language lyrics keep their sections. If no Swahili is present, return the original single-language lyrics in both fields; never fabricate Swahili. Also return swahili: the same song with English translation lines removed, keeping Swahili, headings and repeat instructions. Return ONLY JSON {\"lyrics\":\"formatted text\",\"swahili\":\"Swahili only text\"}. Lyrics:\n" + source;
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
      if (text.isEmpty() || words(source) != words(text)) {
        done({}, "AI changed or omitted words. Your original lyrics were kept."); return;
      }
      const QString sw = QJsonDocument::fromJson(response.mid(first,last-first+1).toUtf8()).object()["swahili"].toString().trimmed();
      const auto allWords = words(text), swWords = words(sw);
      if (sw.isEmpty() || !std::includes(allWords.begin(), allWords.end(), swWords.begin(), swWords.end())) {
        done({}, "AI could not safely separate the languages. Your lyrics were kept."); return;
      }
      swahiliText = sw;
      done(text, {});
    });
  }
private:
  QNetworkAccessManager net;
};
