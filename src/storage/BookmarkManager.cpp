#include "BookmarkManager.h"
#include "Database.h"

#include <QDebug>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

BookmarkManager::BookmarkManager(QObject *parent) : QObject(parent) {}

// ── Bookmarks ─────────────────────────────────────────────────────────────────

bool BookmarkManager::addBookmark(const QString &bookId,
                                   const QString &bookTitle,
                                   const QString &chapterId,
                                   const QString &chapterTitle) {
    QSqlQuery q(Database::instance().db());
    q.prepare(
        "INSERT INTO bookmarks (book_id, book_title, chapter_id, chapter_title)"
        " VALUES (:bid, :bt, :cid, :ct)");
    q.bindValue(":bid", bookId);
    q.bindValue(":bt",  bookTitle);
    q.bindValue(":cid", chapterId);
    q.bindValue(":ct",  chapterTitle);
    if (!q.exec()) {
        qWarning() << "addBookmark failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool BookmarkManager::removeBookmark(int id) {
    QSqlQuery q(Database::instance().db());
    q.prepare("DELETE FROM bookmarks WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

QList<Bookmark> BookmarkManager::allBookmarks() const {
    QList<Bookmark> list;
    QSqlQuery q(Database::instance().db());
    q.prepare(
        "SELECT id, book_id, book_title, chapter_id, chapter_title, created_at"
        " FROM bookmarks ORDER BY created_at DESC");
    if (!q.exec()) return list;
    while (q.next()) {
        list.append({q.value(0).toInt(),
                     q.value(1).toString(),
                     q.value(2).toString(),
                     q.value(3).toString(),
                     q.value(4).toString(),
                     q.value(5).toString()});
    }
    return list;
}

// ── Notes ─────────────────────────────────────────────────────────────────────

bool BookmarkManager::addNote(const QString &bookId, const QString &bookTitle,
                               const QString &chapterId,
                               const QString &chapterTitle,
                               const QString &content) {
    QSqlQuery q(Database::instance().db());
    q.prepare(
        "INSERT INTO notes (book_id, book_title, chapter_id, chapter_title, content)"
        " VALUES (:bid, :bt, :cid, :ct, :c)");
    q.bindValue(":bid", bookId);
    q.bindValue(":bt",  bookTitle);
    q.bindValue(":cid", chapterId);
    q.bindValue(":ct",  chapterTitle);
    q.bindValue(":c",   content);
    if (!q.exec()) {
        qWarning() << "addNote failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool BookmarkManager::removeNote(int id) {
    QSqlQuery q(Database::instance().db());
    q.prepare("DELETE FROM notes WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

QList<Note> BookmarkManager::allNotes() const {
    QList<Note> list;
    QSqlQuery q(Database::instance().db());
    q.prepare(
        "SELECT id, book_id, book_title, chapter_id, chapter_title, content, created_at"
        " FROM notes ORDER BY created_at DESC");
    if (!q.exec()) return list;
    while (q.next()) {
        list.append({q.value(0).toInt(),
                     q.value(1).toString(),
                     q.value(2).toString(),
                     q.value(3).toString(),
                     q.value(4).toString(),
                     q.value(5).toString(),
                     q.value(6).toString()});
    }
    return list;
}

QList<Note> BookmarkManager::notesForBook(const QString &bookId) const {
    QList<Note> list;
    QSqlQuery q(Database::instance().db());
    q.prepare(
        "SELECT id, book_id, book_title, chapter_id, chapter_title, content, created_at"
        " FROM notes WHERE book_id = :bid ORDER BY created_at DESC");
    q.bindValue(":bid", bookId);
    if (!q.exec()) return list;
    while (q.next()) {
        list.append({q.value(0).toInt(),
                     q.value(1).toString(),
                     q.value(2).toString(),
                     q.value(3).toString(),
                     q.value(4).toString(),
                     q.value(5).toString(),
                     q.value(6).toString()});
    }
    return list;
}

// ── Books ─────────────────────────────────────────────────────────────────────

bool BookmarkManager::upsertBook(const QString &bookId, const QString &title,
                                  const QString &coverUrl, const QString &author) {
    // Use "title:<title>" as fallback key when real bookId is not yet known.
    const QString effectiveId = bookId.isEmpty() ? ("title:" + title) : bookId;
    auto &db = Database::instance().db();

    // Step 1: insert if the key doesn't exist yet (no-op otherwise)
    {
        QSqlQuery q(db);
        q.prepare("INSERT OR IGNORE INTO books (book_id, title, author, cover_url)"
                  " VALUES (?, ?, ?, ?)");
        q.addBindValue(effectiveId);
        q.addBindValue(title);
        q.addBindValue(author);
        q.addBindValue(coverUrl);
        if (!q.exec()) {
            qWarning() << "upsertBook insert failed:" << q.lastError().text();
            return false;
        }
    }

    // Step 2: update mutable fields (title/author/cover may change on re-scrape)
    {
        QSqlQuery q(db);
        q.prepare("UPDATE books SET title=?, author=?, cover_url=? WHERE book_id=?");
        q.addBindValue(title);
        q.addBindValue(author);
        q.addBindValue(coverUrl);
        q.addBindValue(effectiveId);
        if (!q.exec()) {
            qWarning() << "upsertBook update failed:" << q.lastError().text();
            return false;
        }
    }
    return true;
}

bool BookmarkManager::updateBookIdByTitle(const QString &title, const QString &bookId) {
    if (bookId.isEmpty() || title.isEmpty()) return false;
    QSqlQuery q(Database::instance().db());
    // Replace the temp "title:..." key with the real bookId
    q.prepare(
        "UPDATE books SET book_id = :newId"
        " WHERE title = :t AND (book_id = :oldId OR book_id = '')");
    q.bindValue(":newId", bookId);
    q.bindValue(":t",     title);
    q.bindValue(":oldId", "title:" + title);
    if (!q.exec()) {
        qWarning() << "updateBookIdByTitle failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QList<QPair<QString,QString>> BookmarkManager::allBooks() const {
    QList<QPair<QString,QString>> list;
    QSqlQuery q(Database::instance().db());
    q.prepare("SELECT book_id, title FROM books ORDER BY last_read DESC, title ASC");
    if (!q.exec()) return list;
    while (q.next()) {
        list.append({q.value(0).toString(), q.value(1).toString()});
    }
    return list;
}

bool BookmarkManager::updateLastRead(const QString &bookId) {
    QSqlQuery q(Database::instance().db());
    q.prepare(
        "UPDATE books SET last_read = datetime('now','localtime')"
        " WHERE book_id = :id");
    q.bindValue(":id", bookId);
    return q.exec();
}
