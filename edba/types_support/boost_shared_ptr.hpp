#ifndef EDBA_TYPES_SUPPORT_BOOST_SHARED_PTR_HPP
#define EDBA_TYPES_SUPPORT_BOOST_SHARED_PTR_HPP

#include <edba/types.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

namespace edba 
{

template<typename T>
struct bind_conversion<boost::shared_ptr<T>, void>
{
    template<typename ColOrName>
    static void bind(statement& st, ColOrName col_or_name, const boost::shared_ptr<T>& v)
    {
        if (v)
            st.bind(col_or_name, *v);
        else
            st.bind(col_or_name, null);
    }
};

template<typename T>
struct fetch_conversion<boost::shared_ptr<T>, typename boost::disable_if< boost::is_const<T> >::type>
{
    template<typename ColOrName>
    static bool fetch(row& res, ColOrName col_or_name, boost::shared_ptr<T>& v)
    {
        if (!res.is_null(col_or_name))
        {
            boost::shared_ptr<T> tmp = boost::make_shared<T>();
            res.fetch(col_or_name, *tmp);
            v.swap(tmp);
        }
        else
            v.reset();

        return true;
    }
};

}

#endif // EDBA_TYPES_SUPPORT_BOOST_SHARED_PTR_HPP