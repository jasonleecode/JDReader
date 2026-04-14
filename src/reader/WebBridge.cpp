#include "WebBridge.h"
#include <QDebug>

WebBridge::WebBridge(QObject *parent) : QObject(parent) {}

void WebBridge::onBookshelfLoaded(const QString &booksJson) {
    qDebug() << "[Bridge] bookshelf loaded, json length:" << booksJson.size();
    emit bookshelfLoaded(booksJson);
}

void WebBridge::onReaderStateChanged(const QString &bookId,
                                      const QString &bookTitle,
                                      const QString &chapterId,
                                      const QString &chapterTitle) {
    qDebug() << "[Bridge] reader state:" << bookTitle << "/" << chapterTitle;
    emit readerStateChanged(bookId, bookTitle, chapterId, chapterTitle);
}

void WebBridge::onBookIdCaptured(const QString &bookId, const QString &title) {
    qDebug() << "[Bridge] bookId captured:" << bookId << "for:" << title;
    emit bookIdCaptured(bookId, title);
}

void WebBridge::onDomDump(const QString &summary) {
    // Print to terminal so the developer can copy-paste selector info
    fprintf(stderr, "\n===== DOM DUMP =====\n%s\n====================\n\n",
            qPrintable(summary));
    fflush(stderr);
}
