#pragma once

#include <nova/core/Core.hpp>

#include <sqlite3.h>

namespace nova
{
    class Database
    {
        sqlite3* db = {};

    public:
        Database(const std::string& path);
        ~Database();

        sqlite3* GetDB();
    };

    class Statement
    {
        sqlite3*        db = {};
        sqlite3_stmt* stmt = {};
        bool      complete = false;

    public:
        Statement(Database& db, const std::string& sql);
        ~Statement();

        void ResetIfComplete();
        bool Step();
        i64 Insert();
        Statement& SetNull(u32 index);
        Statement& SetString(u32 index, StringView str);
        Statement& SetInt(u32 index, i64 value);
        Statement& SetReal(u32 index, f64 value);

        StringView GetString(u32 index);
        i64 GetInt(u32 index);
        f64 GetReal(u32 index);
    };
}
