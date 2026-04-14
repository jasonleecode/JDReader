#include "BookshelfPanel.h"

#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>

// Role constants for item data
static constexpr int RoleType      = Qt::UserRole;     // "book" | "bookmark"
static constexpr int RoleBookId    = Qt::UserRole + 1;
static constexpr int RoleItemId    = Qt::UserRole + 2; // bookmark DB id
static constexpr int RoleChapterId = Qt::UserRole + 3;

BookshelfPanel::BookshelfPanel(BookmarkManager *bm, QWidget *parent)
    : QWidget(parent), m_bm(bm) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *title = new QLabel(tr("书架"), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(14);
    layout->addWidget(m_tree);

    buildTree();

    connect(m_tree, &QTreeWidget::itemClicked,
            this,   &BookshelfPanel::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this,   &BookshelfPanel::onContextMenuRequested);
}

void BookshelfPanel::buildTree() {
    m_tree->clear();

    m_shelfRoot = new QTreeWidgetItem(m_tree, {tr("我的书架")});
    m_shelfRoot->setFlags(m_shelfRoot->flags() & ~Qt::ItemIsSelectable);

    m_bookmarkRoot = new QTreeWidgetItem(m_tree, {tr("我的书签")});
    m_bookmarkRoot->setFlags(m_bookmarkRoot->flags() & ~Qt::ItemIsSelectable);

    m_tree->expandAll();
    refreshBookmarks();
}

void BookshelfPanel::onBookshelfDataReady(const QString &booksJson) {
    if (!m_shelfRoot) return;

    const QJsonArray arr =
        QJsonDocument::fromJson(booksJson.toUtf8()).array();
    if (arr.isEmpty()) return;

    // Remove previous children
    while (m_shelfRoot->childCount() > 0)
        delete m_shelfRoot->takeChild(0);

    for (const QJsonValue &v : arr) {
        const QJsonObject obj    = v.toObject();
        const QString bookId     = obj["bookId"].toString();
        const QString title      = obj["title"].toString();
        const QString cover      = obj["coverUrl"].toString();
        const QString author     = obj["author"].toString();

        if (title.isEmpty()) continue;

        // Display: show a "◌" indicator when bookId is not yet known
        const QString display = bookId.isEmpty()
            ? QString("◌ %1").arg(title)
            : title;

        auto *item = new QTreeWidgetItem(m_shelfRoot, {display});
        item->setToolTip(0, bookId.isEmpty()
            ? tr("%1\n（在书架中点击该书后 ID 将自动记录）").arg(title)
            : title);
        item->setData(0, RoleType,   "book");
        item->setData(0, RoleBookId, bookId);
        item->setData(0, Qt::UserRole + 6, title); // raw title without prefix

        // Persist to DB
        m_bm->upsertBook(bookId, title, cover, author);
    }

    m_shelfRoot->setExpanded(true);
}

void BookshelfPanel::updateBookId(const QString &bookId, const QString &title) {
    if (!m_shelfRoot || bookId.isEmpty() || title.isEmpty()) return;

    // Update DB
    m_bm->updateBookIdByTitle(title, bookId);

    // Update sidebar item: find by raw title, replace "◌ title" with plain title
    for (int i = 0; i < m_shelfRoot->childCount(); ++i) {
        QTreeWidgetItem *item = m_shelfRoot->child(i);
        const QString rawTitle = item->data(0, Qt::UserRole + 6).toString();
        if (rawTitle == title && item->data(0, RoleBookId).toString().isEmpty()) {
            item->setText(0, title);
            item->setToolTip(0, title);
            item->setData(0, RoleBookId, bookId);
            break;
        }
    }
}

void BookshelfPanel::refreshBookmarks() {
    if (!m_bookmarkRoot) return;

    while (m_bookmarkRoot->childCount() > 0)
        delete m_bookmarkRoot->takeChild(0);

    for (const Bookmark &bm : m_bm->allBookmarks()) {
        const QString label = bm.chapterTitle.isEmpty()
                                  ? bm.bookTitle
                                  : QString("%1 · %2").arg(bm.bookTitle, bm.chapterTitle);
        auto *item = new QTreeWidgetItem(m_bookmarkRoot, {label});
        item->setToolTip(0, label);
        item->setData(0, RoleType,      "bookmark");
        item->setData(0, RoleBookId,    bm.bookId);
        item->setData(0, RoleItemId,    bm.id);
        item->setData(0, RoleChapterId, bm.chapterId);
        item->setData(0, Qt::UserRole + 4, bm.bookTitle);
        item->setData(0, Qt::UserRole + 5, bm.chapterTitle);
    }

    m_bookmarkRoot->setExpanded(true);
}

void BookshelfPanel::onItemClicked(QTreeWidgetItem *item, int /*column*/) {
    if (!item) return;
    const QString type = item->data(0, RoleType).toString();

    if (type == "book") {
        const QString bookId   = item->data(0, RoleBookId).toString();
        const QString rawTitle = item->data(0, Qt::UserRole + 6).toString();

        if (!bookId.isEmpty() && !bookId.startsWith("title:")) {
            // Real bookId captured — navigate directly
            emit openBookRequested(bookId);
        } else {
            // No ID yet — tell ReaderView to auto-click by title in bookshelf
            emit openBookByTitleRequested(rawTitle);
        }
    } else if (type == "bookmark") {
        Bookmark bm;
        bm.id           = item->data(0, RoleItemId).toInt();
        bm.bookId       = item->data(0, RoleBookId).toString();
        bm.bookTitle    = item->data(0, Qt::UserRole + 4).toString();
        bm.chapterId    = item->data(0, RoleChapterId).toString();
        bm.chapterTitle = item->data(0, Qt::UserRole + 5).toString();
        emit openBookmarkRequested(bm);
    }
}

void BookshelfPanel::onContextMenuRequested(const QPoint &pos) {
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item) return;

    const QString type = item->data(0, RoleType).toString();
    if (type != "bookmark") return;

    QMenu menu(this);
    QAction *del = menu.addAction(tr("删除书签"));
    if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == del) {
        const int id = item->data(0, RoleItemId).toInt();
        m_bm->removeBookmark(id);
        refreshBookmarks();
    }
}
