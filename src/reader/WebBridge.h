#pragma once

#include <QObject>
#include <QString>

// Exposed to JavaScript via QWebChannel as object named "bridge".
// JS calls its slots; C++ listens to its signals.
class WebBridge : public QObject {
    Q_OBJECT
public:
    explicit WebBridge(QObject *parent = nullptr);

public slots:
    // Called by JS when the bookshelf page is rendered.
    // booksJson: JSON array of { bookId, title, coverUrl }
    void onBookshelfLoaded(const QString &booksJson);

    // Called by JS whenever the reader URL / chapter changes.
    void onReaderStateChanged(const QString &bookId,
                              const QString &bookTitle,
                              const QString &chapterId,
                              const QString &chapterTitle);

    // Called by JS when clicking a book card captures its bookId from URL change.
    void onBookIdCaptured(const QString &bookId, const QString &title);

    // Debug: JS dumps a DOM summary so we can identify correct selectors.
    void onDomDump(const QString &summary);

signals:
    void bookshelfLoaded(const QString &booksJson);
    void readerStateChanged(const QString &bookId,
                            const QString &bookTitle,
                            const QString &chapterId,
                            const QString &chapterTitle);
    void bookIdCaptured(const QString &bookId, const QString &title);
};
