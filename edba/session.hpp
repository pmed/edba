#ifndef EDBA_SESSION_HPP
#define EDBA_SESSION_HPP

#include <edba/statement.hpp>
#include <edba/backend/backend.hpp>

namespace edba {

///
/// \brief SQL session object that represents a single connection and is the gateway to SQL database
///
/// It is the main class that is used for access to the DB, it uses various singleton classes to
/// load drivers open connections and cache them. 
///
class session 
{
    struct once_type;

public:   

    ///
    /// Create an empty session object, it should not be used until it is opened with calling open() function.
    ///
    session();

    ///
    /// Create a session using a pointer to backend::connection.
    ///
    template<typename Driver>
    session(Driver driver, const string_ref& conn_string, session_monitor* sm = 0) ;

    ///
    /// Close current connection.
    ///
    void close();
    ///
    /// Check if the session was opened.
    ///
    bool is_open();

    ///
    /// Try fetch statement from cache. If it doesn`t exist then create prepared statement, put it in cache and return.
    /// This is the most convenient function to create statements with.
    ///
    statement prepare_statement(const string_ref& q);

    ///
    /// Create ordinary statement it generally unprepared statement and it is never cached. It should
    /// be used when such statement is executed rarely or very customized.
    ///
    statement create_statement(const string_ref& q);

    ///
    /// Syntactic sugar for unprepared statements and their subsequent execution. Following two lines are equvalent
    /// \code
    /// sess.once() << query << use(somevar);
    /// sess.create_statement(query) << use(somevar) << exec;
    ///
    once_type once();

    ///
    /// Execute list of sql commands as single request to database
    ///
    void exec_batch(const string_ref& q);

    ///
    /// Begin a transaction. Don't use it directly for RAII reasons. Use transaction class instead.
    ///
    void begin();
    ///
    /// Commit a transaction. Don't use it directly for RAII reasons. Use transaction class instead.
    ///
    void commit();
    ///
    /// Rollback a transaction. Don't use it directly for RAII reasons. Use transaction class instead.
    ///
    void rollback();

    ///
    /// Escape a string in range [\a b,\a e) for inclusion in SQL statement. It does not add quotation marks at beginning and end.
    /// It is designed to be used with text, don't use it with generic binary data.
    ///
    /// Some backends (odbc) may not support this.
    ///
    std::string escape(char const *b,char const *e);
    ///
    /// Escape a NULL terminated string \a s for inclusion in SQL statement. It does not add quotation marks at beginning and end.
    /// It is designed to be used with text, don't use it with generic binary data.
    ///
    /// Some backends (odbc) may not support this.
    ///
    std::string escape(char const *s);
    ///
    /// Escape a string \a s for inclusion in SQL statement. It does not add quotation marks at beginning and end.
    /// It is designed to be used with text, don't use it with generic binary data.
    ///
    /// Some backends (odbc) may not support this.
    ///
    std::string escape(std::string const &s);
    ///
    /// Get the backend name, as postgresql, odbc, sqlite3.
    /// Known backends are:
    /// - odbc
    /// - sqlite3
    /// - PgSQL
    /// - Oracle
    /// - MySQL
    /// 
    const std::string& backend();
    ///
    /// Get an SQL database name, it may be not the same as driver name for multiple engine drivers like odbc.
    /// This names can be used inside of SQL scripts to write statements specific only for some engine
    /// Currently known engines are:
    /// - sqlite3
    /// - PgSQL
    /// - Oracle
    /// - MySQL
    /// - Microsoft SQL Server
    ///
    const std::string& engine();
    ///
    /// Get the SQL Server version 
    ///
    void version(int& major, int& minor);
    ///
    /// Get human readable string describing SQL Server, usefull for logging
    ///
    const std::string& description();

    ///
    /// Equality operator
    ///
    friend bool operator==(const session& s1, const session& s2)
    {
        return s1.conn_ == s2.conn_;
    }

private:
    boost::intrusive_ptr<backend::connection> conn_;
};

// ------ session implementation ------

struct session::once_type
{
    once_type(session* sess) : sess_(sess) {}

    statement operator<<(const string_ref& q)
    {
        return sess_->create_statement(q);
    }

private:
    session* sess_;
};

inline session::session()
{
}

template<typename Driver>
session::session(Driver driver, const string_ref& conn_string, session_monitor* sm) 
    : conn_(driver.open_connection(conn_string, sm))
{
}

inline void session::close()
{
    conn_.reset();
}
inline bool session::is_open()
{
    return conn_;
}
inline statement session::prepare_statement(const string_ref& query)
{
    boost::intrusive_ptr<backend::statement> stat_ptr(conn_->prepare_statement(query));
    statement stat(stat_ptr);
    return stat;
}
inline statement session::create_statement(const string_ref& query)
{
    boost::intrusive_ptr<backend::statement> stat_ptr(conn_->create_statement(query));
    statement stat(stat_ptr);
    return stat;
}
inline session::once_type session::once()
{
    return once_type(this);
}
inline void session::exec_batch(const string_ref& q)
{
    conn_->exec_batch(q);
}
inline void session::begin()
{
    conn_->begin();
}
inline void session::commit()
{
    conn_->commit();
}
inline void session::rollback()
{
    conn_->rollback();
}
inline std::string session::escape(char const *b,char const *e)
{
    return conn_->escape(b,e);
}
inline std::string session::escape(char const *s)
{
    return conn_->escape(s);
}
inline std::string session::escape(std::string const &s)
{
    return conn_->escape(s);
}
inline const std::string& session::backend()
{
    return conn_->backend();
}
inline const std::string& session::engine()
{
    return conn_->engine();
}
inline void session::version(int& major, int& minor)
{
    conn_->version(major, minor);
}
inline const std::string& session::description()
{
    return conn_->description();
}

// ------ free functions ------

///
/// Syntactic sugar, same as prepare(q)
///
inline statement operator<<(session& s, string_ref query)
{
    return s.prepare_statement(query);
}

}

#endif // EDBA_SESSION_HPP