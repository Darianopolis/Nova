#include "nova_Sqlite.hpp"

#include <nova/core/nova_Debug.hpp>

namespace nova
{
    Database::Database(const std::string& path)
    {
        if (auto err = sqlite3_open(path.c_str(), &db)) {
            NOVA_THROW("Error[{}] opening sqlite3 db file [{}]", err, path);
        }
    }

    Database::~Database()
    {
        if (db) {
            sqlite3_close(db);
        }
    }

    sqlite3* Database::GetDB()
    {
        return db;
    }

    // -----------------------------------------------------------------------------

    Statement::Statement(Database& _db, const std::string& sql)
        : db(_db.GetDB())
    {
        const c8* tail;
        if (auto err = sqlite3_prepare_v2(db, sql.c_str(), i32(sql.length()) + 1, &stmt, &tail)) {
            NOVA_THROW("Error[{}] during prepare: {}", err, sqlite3_errmsg(db));
        }
    }

    Statement::~Statement()
    {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }

    void Statement::ResetIfComplete()
    {
        if (!complete) {
            return;
        }

        if (auto err = sqlite3_reset(stmt)) {
            NOVA_THROW("Error[{}] resetting stmt: {}", err, sqlite3_errmsg(db));
        }

        complete = false;
    }

    bool Statement::Step()
    {
        ResetIfComplete();

        auto res = sqlite3_step(stmt);
        if (res == SQLITE_DONE) {
            complete = true;
            return false;
        }

        if (res == SQLITE_ROW) {
            return true;
        }

        NOVA_THROW("Error[{}] during step: {}", res, sqlite3_errmsg(db));
    }

    i64 Statement::Insert()
    {
        Step();
        return sqlite3_last_insert_rowid(db);
    }

    Statement& Statement::SetNull(u32 index)
    {
        ResetIfComplete();

        if (auto err = sqlite3_bind_null(stmt, index)) {
            NOVA_THROW("Error[{}] during set: {}", err, sqlite3_errmsg(db));
        }

        return *this;
    }

    Statement& Statement::SetString(u32 index, StringView str)
    {
        ResetIfComplete();

        if (auto err = sqlite3_bind_text(stmt, index, str.Data(), i32(str.Size()), SQLITE_STATIC)) {
            NOVA_THROW("Error[{}] during set: {}", err, sqlite3_errmsg(db));
        }

        return *this;
    }

    Statement& Statement::SetInt(u32 index, i64 value)
    {
        ResetIfComplete();

        if (auto err = sqlite3_bind_int64(stmt, index, value)) {
            NOVA_THROW("Error[{}] during set: {}", err, sqlite3_errmsg(db));
        }

        return *this;
    }

    Statement& Statement::SetReal(u32 index, f64 value)
    {
        ResetIfComplete();

        if (auto err = sqlite3_bind_double(stmt, index, value)) {
            NOVA_THROW("Error[{}] during set: {}", err, sqlite3_errmsg(db));
        }

        return *this;
    }

    StringView Statement::GetString(u32 index)
    {
        return reinterpret_cast<const c8*>(sqlite3_column_text(stmt, index - 1));
    }

    i64 Statement::GetInt(u32 index)
    {
        return sqlite3_column_int64(stmt, index - 1);
    }

    f64 Statement::GetReal(u32 index)
    {
        return sqlite3_column_double(stmt, index - 1);
    }
}