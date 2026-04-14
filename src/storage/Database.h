#pragma once

#include <QString>
#include <QtSql/QSqlDatabase>

class Database {
public:
    static Database &instance();

    bool open();
    void close();
    QSqlDatabase &db();

private:
    Database() = default;
    ~Database();

    bool createTables();

    QSqlDatabase m_db;
};
