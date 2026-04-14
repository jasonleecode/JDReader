#pragma once

#include "storage/BookmarkManager.h"

#include <QTreeWidget>
#include <QWidget>

class BookshelfPanel : public QWidget {
    Q_OBJECT
public:
    explicit BookshelfPanel(BookmarkManager *bm, QWidget *parent = nullptr);

    void refreshBookmarks();
    void updateBookId(const QString &bookId, const QString &title);

public slots:
    void onBookshelfDataReady(const QString &booksJson);

signals:
    // bookId is a real numeric id — navigate directly
    void openBookRequested(const QString &bookId);
    // bookId unknown — navigate by title (auto-click in bookshelf page)
    void openBookByTitleRequested(const QString &title);
    void openBookmarkRequested(const Bookmark &bm);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onContextMenuRequested(const QPoint &pos);

private:
    void buildTree();
    QTreeWidgetItem *m_shelfRoot    = nullptr;
    QTreeWidgetItem *m_bookmarkRoot = nullptr;

    QTreeWidget     *m_tree = nullptr;
    BookmarkManager *m_bm   = nullptr;
};
