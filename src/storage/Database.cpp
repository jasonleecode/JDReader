#include "Database.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

Database &Database::instance() {
    static Database inst;
    return inst;
}

Database::~Database() {
    close();
}

bool Database::open() {
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);

    const QString dbPath = dataDir + "/jdreader.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", "jdreader");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Database open failed:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}

void Database::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

QSqlDatabase &Database::db() {
    return m_db;
}

bool Database::createTables() {
    QSqlQuery q(m_db);

    const QStringList ddl = {
        R"(CREATE TABLE IF NOT EXISTS books (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            book_id     TEXT NOT NULL UNIQUE,
            title       TEXT NOT NULL,
            author      TEXT,
            cover_url   TEXT,
            last_read   TEXT
        ))",
        R"(CREATE TABLE IF NOT EXISTS bookmarks (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            book_id         TEXT NOT NULL,
            book_title      TEXT NOT NULL,
            chapter_id      TEXT,
            chapter_title   TEXT,
            created_at      TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS notes (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            book_id         TEXT NOT NULL,
            book_title      TEXT NOT NULL,
            chapter_id      TEXT,
            chapter_title   TEXT,
            content         TEXT,
            created_at      TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        ))",
    };

    for (const QString &sql : ddl) {
        if (!q.exec(sql)) {
            qWarning() << "DDL failed:" << q.lastError().text();
            return false;
        }
    }

    // Schema migrations: ADD COLUMN silently fails if the column already exists,
    // which is fine — we just need it to succeed on older DB files.
    q.exec("ALTER TABLE books ADD COLUMN author TEXT");

    return true;
}
