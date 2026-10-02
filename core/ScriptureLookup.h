#pragma once
#include "BibleManager.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QRegularExpression>
#include <functional>

class ScriptureLookup : public QObject {
public:
  using Callback = std::function<void(std::vector<BibleVerse>, QString)>;
  explicit ScriptureLookup(QObject *parent = nullptr) : QObject(parent), net(this) {}
  void cancel() { ++generation; }
  void search(const QString &description, const QString &version, Callback callback) {
    const int request = ++generation;
    const QString prompt = "Find up to 5 Bible verses matching this description. Return ONLY a JSON array of objects with book (canonical English name), chapter (integer), verse (integer). Do not return verse text. If uncertain return []. Description: " + description;
    send(prompt, version, request, std::move(callback), true);
  }
private:
  QNetworkAccessManager net;
  int generation = 0;
  void send(const QString &prompt, const QString &version, int request, Callback callback, bool tryGroq) {
    QSettings settings;
    const QString key = settings.value("AI/GroqApiKey").toString();
    const bool groq = tryGroq && !key.isEmpty();
    QJsonObject body;
    QNetworkRequest req{QUrl(groq ? "https://api.groq.com/openai/v1/chat/completions" : "https://abytrivia.pythonanywhere.com/api/ai/chat")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(30000);
    if (groq) {
      req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
      body["model"] = "openai/gpt-oss-120b";
      body["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", prompt}}};
      body["max_tokens"] = 800;
    } else {
      body["message"] = prompt;
      body["history"] = QJsonArray{};
    }
    body["temperature"] = 0.1;
    auto *reply = net.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, prompt, version, request, callback, groq]() {
      reply->deleteLater();
      if (request != generation) return;
      const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
      QString response;
      if (reply->error() == QNetworkReply::NoError) {
        if (groq) response = obj.value("choices").toArray().first().toObject().value("message").toObject().value("content").toString();
        else if (obj.value("success").toBool()) response = obj.value("response").toString();
      }
      if (response.isEmpty()) {
        if (groq) { send(prompt, version, request, callback, false); return; }
        callback({}, "AI lookup unavailable. Try a reference or keyword, or retry later.");
        return;
      }
      const int start = response.indexOf('['), end = response.lastIndexOf(']');
      const auto doc = QJsonDocument::fromJson(response.mid(start, end - start + 1).toUtf8());
      if (start < 0 || !doc.isArray()) {
        callback({}, "AI could not identify a valid scripture reference. Try a clearer description.");
        return;
      }
      std::vector<BibleVerse> verses;
      QStringList seen;
      auto &bible = BibleManager::instance();
      for (const auto &entry : doc.array()) {
        const auto ref = entry.toObject();
        const QString book = BibleManager::normalizeBookName(ref.value("book").toString());
        const int chapter = ref.value("chapter").toInt(), verse = ref.value("verse").toInt();
        const QString id = book + QString(" %1:%2").arg(chapter).arg(verse);
        if (chapter < 1 || verse < 1 || seen.contains(id)) continue;
        const QString text = bible.getVerseText(book, chapter, verse, version);
        if (text.isEmpty()) continue;
        seen.append(id);
        verses.push_back({book, chapter, verse, text, version});
        if (verses.size() == 5) break;
      }
      callback(verses, verses.empty() ? "No matching references could be verified in " + version + ". Try a more specific description." : QString());
    });
  }
};
