#pragma once
#include <string>
#include <vector>
#include <map>
#include <boost/algorithm/string.hpp>

#include "httpclientlog.h"



namespace
{
    inline unsigned char tohex(const unsigned char x)
    {
        return  (x > 9) ? (x - 10 + 'A') : (x + '0');
    }

    inline unsigned char fromhex(const unsigned char x)
    {
        unsigned char y = 0;
        if (x >= 'A' && x <= 'Z')
        {
            y = x - 'A' + 10;
        }
        else if (x >= 'a' && x <= 'z')
        {
            y = x - 'a' + 10;
        }
        else if (x >= '0' && x <= '9')
        {
            y = x - '0';
        }
        else
        {
            y = 0;
        }
        return y;
    }
}



namespace HttpUtil
{
    //************************************
    // brief:    tool function, transform key-value style param to string style param with given separator
    // name:     build_kv_string
    // param:    const std::map<std::string, std::string> & kv_param
    // param:    const std::string & kv_sep                             separator between key and value
    // param:    const std::string & pair_sep                           separator between key-value pairs
    // return:   std::string
    // remarks:
    //************************************
    inline std::string build_kv_string(const std::map<std::string, std::string>& kv_param,
        const std::string& kv_sep = "=", const std::string& pair_sep = "&")
    {
        std::string s;
        for (std::map<std::string, std::string>::const_iterator iterKey = kv_param.begin();
            iterKey != kv_param.end();
            ++iterKey)
        {
            s += iterKey->first + kv_sep + iterKey->second + pair_sep;
        }
        boost::algorithm::erase_last(s, "&");
        return s;
    }

    //tool function, transform string style param to key-value style param using given separator, see above
    inline void parse_kv_string(const std::string& s, std::map<std::string, std::string>& kv_param,
        const std::string& kv_sep = "=", const std::string& pair_sep = "&")
    {
        kv_param.clear();

        std::vector<std::string> pairs;
        boost::algorithm::split(pairs, s, boost::algorithm::is_any_of(pair_sep));
        for (std::vector<std::string>::iterator iterStr = pairs.begin();
            iterStr != pairs.end();
            ++iterStr)
        {
            if (iterStr->empty())
            {
                HTTP_CLIENT_WARN << "encountered an empty pair";
                continue;
            }
            std::vector<std::string> kv;
            boost::algorithm::split(kv, *iterStr, boost::algorithm::is_any_of(kv_sep));
            if (2 != kv.size())
            {
                HTTP_CLIENT_WARN << "encountered a pair[" << *iterStr << "] which can not split 2 parts by [" << kv_sep << "]";
                continue;
            }
            kv_param[kv[0]] = kv[1];
        }
    }


    //************************************
    // brief:    tool function, url encode
    // name:     url_encode
    // param:    const std::string & s
    // return:   std::string
    // remarks:
    //************************************
    inline std::string url_encode(const std::string& s)
    {
        std::string strEncoded;
        for (std::string::const_iterator iterCh = s.begin(); iterCh != s.end(); ++iterCh)
        {
            const unsigned char ch = *iterCh;
            if (isalnum(ch) || (ch == '-') || (ch == '_') || (ch == '.') || (ch == '~'))
            {
                strEncoded += ch;
            }
            else if (ch == ' ')
            {
                strEncoded += '+';
            }
            else
            {
                strEncoded += '%';
                strEncoded += tohex(ch >> 4);//高4位
                strEncoded += tohex(ch % 16);//低4位
            }
        }
        return strEncoded;
    }

    //url decode, see above
    inline std::string url_decode(const std::string& s)
    {
        std::string strDecoded;
        for (std::string::const_iterator iterCh = s.begin(); iterCh != s.end();)
        {
            if (*iterCh == '+')
            {
                strDecoded += ' ';
                ++iterCh;
            }
            else if (*iterCh == '%')
            {
                if (++iterCh == s.end())
                {
                    break;
                }
                unsigned char high = fromhex(*iterCh);

                if (++iterCh == s.end())
                {
                    break;
                }
                unsigned char low = fromhex(*iterCh);

                strDecoded += (high * 16 + low);

                ++iterCh;
            }
            else
            {
                strDecoded += *iterCh;
                ++iterCh;
            }
        }
        return strDecoded;
    }
}

