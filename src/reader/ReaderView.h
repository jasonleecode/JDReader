#pragma once

#include "WebBridge.h"

#include <QUrl>
#include <QWidget>
#include <QtWebEngineWidgets/QWebEngineView>

class QWebEngineProfile;
class QWebEnginePage;
class QWebChannel;

class ReaderView : public QWidget {
    Q_OBJECT
public:
    explicit ReaderView(QWidget *parent = nullptr);

    void load(const QUrl &url);
    // Navigate directly via bookId (known numeric id)
    void openBook(const QString &bookId);
    // Navigate by title: go to bookshelf then auto-click the matching book card
    void openBookByTitle(const QString &title);

    // Debug helpers (accessible from Developer menu)
    void runDomDump();
    void triggerBookshelfScrape();

    // Accessors for current reader state
    QString currentBookId()      const { return m_bookId; }
    QString currentBookTitle()   const { return m_bookTitle; }
    QString currentChapterId()   const { return m_chapterId; }
    QString currentChapterTitle()const { return m_chapterTitle; }

    QWebEngineView *webView() { return m_view; }

public slots:
    void setDarkMode(bool dark);

signals:
    void bookshelfDataReady(const QString &booksJson);
    void readerStateChanged(const QString &bookId, const QString &bookTitle,
                            const QString &chapterId, const QString &chapterTitle);
    void bookIdCaptured(const QString &bookId, const QString &title);
    void loadingStarted();
    void loadingFinished();

private slots:
    void onLoadFinished(bool ok);
    void onBridgeBookshelfLoaded(const QString &json);
    void onBridgeReaderStateChanged(const QString &bookId,
                                     const QString &bookTitle,
                                     const QString &chapterId,
                                     const QString &chapterTitle);

private:
    void setupProfile();
    void injectScripts();
    void autoClickBookByTitle(const QString &title);

    QWebEngineView    *m_view    = nullptr;
    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage    *m_page    = nullptr;
    QWebChannel       *m_channel = nullptr;
    WebBridge         *m_bridge  = nullptr;

    bool    m_darkMode    = false;
    QString m_pendingBookTitle;   // set by openBookByTitle, consumed in onLoadFinished
    QString m_bookId;
    QString m_bookTitle;
    QString m_chapterId;
    QString m_chapterTitle;

    static constexpr const char *JD_HOME = "https://ebooks.jd.com/";
};
