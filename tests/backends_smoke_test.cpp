#include "monitor.hpp"

#include <edba/edba.hpp>

#include <boost/format.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/foreach.hpp>
#include <boost/timer.hpp>
#include <boost/scope_exit.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <ctime>

using namespace std;
using namespace edba;

#define SERVER_IP "192.168.1.102"

void test_escaping(session sess)
{
    try 
    {
        try { sess.once() << "~Oracle~drop table test_escaping~" << exec; } catch(...) {}

        sess.once() <<
            "~Microsoft SQL Server~create table ##test_escaping(txt nvarchar(100))"
            "~Sqlite3~create temp table test_escaping(txt nvarchar(100)) "
            "~Mysql~create temporary table test_escaping(txt nvarchar(100))"
            "~PgSQL~create temp table test_escaping(txt varchar(100))"
            "~Oracle~create table test_escaping( txt nvarchar2(100) )"
            "~"
            << exec;

        string bad_string = "\\''\\' insert into char'";
        string good_string = sess.escape(bad_string);

        string insert_query = boost::str(boost::format(
            "~Microsoft SQL Server~insert into ##test_escaping(txt) values('%1%')"
            "~~insert into test_escaping(txt) values('%1%')"
            "~") % good_string);

        sess.once() << insert_query << exec;

        string result;
        sess.once() << 
            "~Microsoft SQL Server~select txt from ##test_escaping"
            "~~select txt from test_escaping"
            "~"
            << first_row >> result;

        BOOST_CHECK_EQUAL(result, bad_string);
    }
    catch(edba::not_supported_by_backend&)
    {
    }
}

template<typename Driver>
void test(const char* conn_string)
{
    const std::locale& loc = std::locale::classic();

    // Workaround for postgres lobs
    conn_info ci(conn_string);
    const char* postgres_lob_type;
    if (ci.has("@blob") && boost::iequals(ci.get("@blob"), "bytea", loc))
        postgres_lob_type = "bytea";
    else
        postgres_lob_type = "oid";

    std::string oracle_cleanup_seq = "~Oracle~drop sequence test1_seq_id~;";
    std::string oracle_cleanup_tbl = "~Oracle~drop table test1~;";

    std::string create_test1_table = boost::str(boost::format(
        "~Oracle~create sequence test1_seq_id~;"
        "~Microsoft SQL Server~create table ##test1( "
        "   id int identity(1, 1) primary key clustered, "
        "   num numeric(18, 3), "
        "   dt datetime, "
        "   dt_small smalldatetime, "
        "   vchar20 nvarchar(40), "
        "   vcharmax varchar(max), "
        "   vbin20 varbinary(20), "
        "   vbinmax varbinary(max), "
        "   txt text"
        "   ) "
        "~Sqlite3~create temp table test1( "
        "   id integer primary key autoincrement, "
        "   num double, "
        "   dt text, "
        "   dt_small text, "
        "   vchar20 nvarchar(40), "
        "   vcharmax text, "
        "   vbin20 blob, "
        "   vbinmax blob, "
        "   txt text "
        "   ) "
        "~Mysql~create temporary table test1( "
        "   id integer AUTO_INCREMENT PRIMARY KEY, "
        "   num numeric(18, 3), "
        "   dt timestamp, "
        "   dt_small date, "    
        "   vchar20 nvarchar(40), "
        "   vcharmax text, "
        "   vbin20 varbinary(20), "
        "   vbinmax blob, "
        "   txt text "
        "   ) "
        "~PgSQL~create temp table test1( "
        "   id serial primary key, "
        "   num numeric(18, 3), "
        "   dt timestamp, "
        "   dt_small date, "
        "   vchar20 varchar(40), "          // postgres don`t have nvarchar
        "   vcharmax varchar(15000), "
        "   vbin20 %1%, "
        "   vbinmax %1%, "
        "   txt text "
        "   ) "
        "~Oracle~create table test1( "
        "   id number primary key, "
        "   num number(18, 3), "
        "   dt timestamp, "
        "   dt_small date, "
        "   vchar20 nvarchar2(20),  "
        "   vcharmax varchar2(4000), " 
        "   vbin20 raw(20), "
        "   vbinmax blob, "
        "   txt clob "
        "   ) "
        "~") % postgres_lob_type);

    const char* insert_test1_data =
        "~Microsoft SQL Server~insert into ##test1(num, dt, dt_small, vchar20, vcharmax, vbin20, vbinmax, txt) "
        "   values(:num, :dt, :dt_small, :vchar20, :vcharmax, :vbin20, :vbinmax, :txt)"
        "~Oracle~insert into test1(id, num, dt, dt_small, vchar20, vcharmax, vbin20, vbinmax, txt)"
        "   values(test1_seq_id.nextval, :num, :dt, :dt_small, :vchar20, :vcharmax, :vbin20, :vbinmax, :txt)"
        "~~insert into test1(num, dt, dt_small, vchar20, vcharmax, vbin20, vbinmax, txt)"
        "   values(:num, :dt, :dt_small, :vchar20, :vcharmax, :vbin20, :vbinmax, :txt)"
        "~";

    const char* select_test1_row = 
        "~Microsoft SQL Server~select id, num, dt, dt_small, vchar20, vcharmax, vbin20, vbinmax, txt from ##test1 where id=:id"
        "~~select id, num, dt, dt_small, vchar20, vcharmax, vbin20, vbinmax, txt from test1 where id=:id"
        "~";

    const char* drop_test1 =
        "~Oracle~drop sequence test1_seq_id~;"
        "~Oracle~drop table test1~;"
        "~Microsoft SQL Server~drop table ##test1"
        "~~drop table test1"
        "~";

    std::string short_binary("binary");
    std::string long_binary(10000, 't');

    std::istringstream long_binary_stream;
    std::istringstream short_binary_stream;

    short_binary_stream.str(short_binary);
    long_binary_stream.str(long_binary);

    std::string text(10000, 'z');

    std::time_t now = std::time(0);

    {
        using namespace edba;

        BOOST_TEST_MESSAGE("Run backend test for " << conn_string);

        monitor sm;
        session sess(Driver(), conn_string, 0);

        // Test empty query
        sess << "" << exec;
        sess << "~~" << exec;

        try {sess.exec_batch(oracle_cleanup_seq);} catch(...) {}
        try {sess.exec_batch(oracle_cleanup_tbl);} catch(...) {}

        // Create table
        sess.exec_batch(create_test1_table);

        // Transaction for inserting data. Postgresql backend require explicit transaction if you want to bind blobs.
        long long id;
        {
            transaction tr(sess);

            // Compile statement for inserting data 
            statement st = sess << insert_test1_data;

            // Exec when part of parameters are nulls
            st << reset
                << 10.10 
                << null
                << *std::gmtime(&now)
                << null
                << null
                << null
                << null
                << null
                << exec;

            // Bind data to statement and execute two times
            st 
                << use("num", 10.10) 
                << use("dt", *std::gmtime(&now)) 
                << use("dt_small", *std::gmtime(&now)) 
                << use("vchar20", "Hello!")
                << use("vcharmax", "Hello! max")
                << use("vbin20", &short_binary_stream)
                << use("vbinmax", &long_binary_stream)
                << use("txt", text)
                << exec
                << exec;

            if (sess.backend() == "oracle")
                id = st.sequence_last("test1_seq_id");
            else
                id = st.last_insert_id();

            // Exec with all null types
            st << reset
               << null 
               << null
               << null
               << null
               << null
               << null
               << null
               << null
               << exec;

            tr.commit();

            // Check that cache works as expected
            statement st1 = sess << insert_test1_data;
            BOOST_CHECK(st == st1);
        }

        // Query single row
        {
            transaction tr(sess);

            row r = sess << select_test1_row << id << first_row;
    
            int id;
            double num;
            std::tm tm1, tm2;
            std::string short_str;
            std::string long_str;
            std::ostringstream short_oss;
            std::ostringstream long_oss;
            std::string txt;

            r >> id >> num >> tm1 >> tm2 >> short_str >> long_str >> short_oss >> long_oss >> txt;

            BOOST_CHECK_EQUAL(num, 10.10);
            BOOST_CHECK(!memcmp(std::gmtime(&now), &tm1, sizeof(tm1)));
            BOOST_CHECK_EQUAL(short_str, "Hello!");
            BOOST_CHECK_EQUAL(long_str, "Hello! max");
            BOOST_CHECK_EQUAL(short_oss.str(), short_binary);
            BOOST_CHECK_EQUAL(long_oss.str(), long_binary);
            BOOST_CHECK_EQUAL(text, txt);

            tr.commit();
        }

        sess.exec_batch(
            "~Microsoft SQL Server~insert into ##test1(num) values(10.2)"
            "~Oracle~insert into test1(id, num) values(test1_seq_id.nextval, 10.2)"
            "~~insert into test1(num) values(10.2)"
            "~;"
            "~Microsoft SQL Server~insert into ##test1(num) values(10.3)"
            "~Oracle~insert into test1(id, num) values(test1_seq_id.nextval, 10.3)"
            "~~insert into test1(num) values(10.3)"
            "~");

        // Try to bind non prepared statement
        // Test once helper 
        sess.once() << 
            "~Microsoft SQL Server~insert into ##test1(num) values(:num)"
            "~Oracle~insert into test1(id, num) values(test1_seq_id.nextval, :num)"
            "~~insert into test1(num) values(:num)"
            "~"
            << use("num", 10.5) 
            << exec;

        // Exec when part of parameters are nulls
        sess.once() << 
            "~Microsoft SQL Server~insert into ##test1(num, dt, dt_small) values(:num, :dt, :dt_small)"
            "~Oracle~insert into test1(id, num, dt, dt_small) values(test1_seq_id.nextval, :num, :dt, :dt_small)"
            "~~insert into test1(num, dt, dt_small) values(:num, :dt, :dt_small)"
            "~"
            << 10.5
            << *std::gmtime(&now)
            << null
            << exec;

        {
            // Regression case: rowset doesn`t support non class types
            rowset<int> rs = sess << 
                "~Microsoft SQL Server~select id from ##test1"
                "~~select id from test1"
                "~";

            copy(rs.begin(), rs.end(), ostream_iterator<int>(cout, " "));
            cout << endl;
        }

        test_escaping(sess);

        sess.exec_batch(drop_test1);
    }
}

BOOST_AUTO_TEST_CASE(Oracle)
{
    test<edba::driver::oracle>("user=system; password=1; ConnectionString=" SERVER_IP ":1521/xe");
}

BOOST_AUTO_TEST_CASE(ODBCWide)
{
    test<edba::driver::odbc>("Driver={SQL Server Native Client 10.0}; Server=" SERVER_IP "\\SQLEXPRESS; Database=EDBA; UID=sa;PWD=1;@utf=wide");
}

BOOST_AUTO_TEST_CASE(ODBC)
{
    test<edba::driver::odbc>("Driver={SQL Server Native Client 10.0}; Server=" SERVER_IP "\\SQLEXPRESS; Database=EDBA; UID=sa;PWD=1;");
}

BOOST_AUTO_TEST_CASE(MySQL)
{
    test<edba::driver::mysql>("host=" SERVER_IP ";database=edba;user=edba;password=1111;");
}

BOOST_AUTO_TEST_CASE(SQLite3)
{
    test<edba::driver::sqlite3>("db=test.db");
}

BOOST_AUTO_TEST_CASE(Postgres)
{
    test<edba::driver::postgresql>("user=postgres; password=1; host=" SERVER_IP "; port=5432; dbname=test");
}

BOOST_AUTO_TEST_CASE(PostgresBytea)
{
    test<edba::driver::postgresql>("user=postgres; password=1; host=" SERVER_IP "; port=5432; dbname=test; @blob=bytea");
}

