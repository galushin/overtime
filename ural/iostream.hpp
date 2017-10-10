#ifndef Z_URAL_IOSTREAM_HPP_INCLUDED
#define Z_URAL_IOSTREAM_HPP_INCLUDED

#include <vector>

namespace ural
{
inline namespace v0
{
    template <class IOStream>
    struct iostream_string_type
    {
        using type = std::basic_string<typename IOStream::char_type,
                                       typename IOStream::traits_type>;
    };

    template <class IOStream>
    using iostream_string_type_t = typename iostream_string_type<IOStream>::type;

    template <class OStream, class Range, class Delimiter>
    OStream &
    write_delimited(OStream & os, Range const & r, Delimiter const & delim)
    {
        using std::begin;
        using std::end;

        auto first = begin(r);
        auto const last = end(r);

        if(first == last)
        {
            return os;
        }

        os << *first;
        ++ first;

        for(; first != last; ++ first)
        {
            os << delim;
            os << *first;
        }

        return os;
    }

    template <class IStream>
    std::vector<iostream_string_type_t<IStream>>
    read_table_row(IStream & is)
    {
        iostream_string_type_t<IStream> line;

        if(!std::getline(is, line) || line.empty())
        {
            return {};
        }

        std::vector<iostream_string_type_t<IStream>> result;

        for(auto first = 0*line.size();;)
        {
            auto const tab_pos = line.find('\t', first);

            result.push_back(line.substr(first, tab_pos - first));

            if(tab_pos >= line.size())
            {
                break;
            }

            first = tab_pos + 1;
        }

        return result;
    }
}
// namespace v0
}
// namespace ural

#endif // Z_URAL_IOSTREAM_HPP_INCLUDED
