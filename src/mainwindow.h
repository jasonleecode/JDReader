#pragma once

#include "reader/ReaderView.h"
#include "sidebar/BookshelfPanel.h"
#include "storage/BookmarkManager.h"

#include <QLabel>
#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onReaderStateChanged(const QString &bookId, const QString &bookTitle,
                               const QString &chapterId, const QString &chapterTitle);
    void onOpenBookRequested(const QString &bookId);
    void onOpenBookmarkRequested(const Bookmark &bm);

    void saveBookmark();
    void addNote();
    void toggleSidebar();
    void toggleReadingMode();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupShortcuts();
    void applySystemTheme();

    QSplitter       *m_splitter     = nullptr;
    BookshelfPanel  *m_shelf        = nullptr;
    ReaderView      *m_reader       = nullptr;
    BookmarkManager *m_bm           = nullptr;

    QToolBar *m_toolbar    = nullptr;
    QLabel   *m_statusLabel = nullptr;

    bool m_readingMode = false;
    bool m_darkMode    = false;
};
