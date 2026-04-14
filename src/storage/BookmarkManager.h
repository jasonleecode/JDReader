#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

struct Bookmark {
    int     id;
    QString bookId;
    QString bookTitle;
    QString chapterId;
    QString chapterTitle;
    QString createdAt;
};

struct Note {
    int     id;
    QString bookId;
    QString bookTitle;
    QString chapterId;
    QString chapterTitle;
    QString content;
    QString createdAt;
};

class BookmarkManager : public QObject {
    Q_OBJECT
public:
    explicit BookmarkManager(QObject *parent = nullptr);

    // Bookmarks
    bool addBookmark(const QString &bookId, const QString &bookTitle,
                     const QString &chapterId, const QString &chapterTitle);
    bool removeBookmark(int id);
    QList<Bookmark> allBookmarks() const;

    // Notes
    bool addNote(const QString &bookId, const QString &bookTitle,
                 const QString &chapterId, const QString &chapterTitle,
                 const QString &content);
    bool removeNote(int id);
    QList<Note> allNotes() const;
    QList<Note> notesForBook(const QString &bookId) const;

    // Known books (scraped from shelf)
    bool upsertBook(const QString &bookId, const QString &title,
                    const QString &coverUrl, const QString &author = {});
    bool updateLastRead(const QString &bookId);
    // Fill in a real bookId for a book that was stored with an empty id
    bool updateBookIdByTitle(const QString &title, const QString &bookId);
    QList<QPair<QString,QString>> allBooks() const; // list of {bookId, title}
};
