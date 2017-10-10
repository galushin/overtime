#include <ural/functional.hpp>
#include <ural/iostream.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <locale>
#include <fstream>
#include <iostream>
#include <set>

using namespace boost::gregorian;
using namespace boost::posix_time;

// @todo Настройка праздничных дней
// @todo Обобщённые правила преобразования наряда в события
// @todo Проверки: нет ли наложения событий
// @todo Учёт времени отдыха
// @todo Перенос остатка на следующий месяц

// Приложение
struct overtime_period
{
    time_duration start;
    time_duration stop;
};

bool operator<(overtime_period const & x, overtime_period const & y)
{
    return std::tie(x.start, x.stop) < std::tie(y.start, y.stop);
}

struct date_overtime
{
    std::set<overtime_period> periods;
    std::set<std::string> orders;
};

struct overtime_event
{
    std::string name;
    date date;
    overtime_period period;
    std::string order;
};

double to_hours(time_duration t)
{
    return t.hours() + t.minutes() / 60.0;
}

class overtime_manager
{
public:
    void process(std::string const & name,
                 date const & day,
                 time_duration start,
                 time_duration stop,
                 std::string order)
    {
        this->check(day, start, stop);

        auto const work_start = this->work_start(day);
        auto const work_stop = this->work_stop(day);

        if(start < work_start)
        {
            events.push_back(overtime_event{name, day, {start, std::min(stop, work_start)}, order});
        }

        if(stop > work_stop)
        {
            events.push_back(overtime_event{name, day, {std::max(start, work_stop), stop}, order});
        }
    }

    template <class OStream>
    OStream &
    write_tables(OStream & output)
    {
        output << "<html>";
        output << "<head>\n" << "<title>Табеля</title>\n" << "</head>\n";
        output << "<body>\n";

        this->write_months(output);

        output << "</body>\n";
        output << "</html>\n";

        return output;
    }

    bool is_holiday(date d) const
    {
        if(d.day_of_week() == Saturday || d.day_of_week() == Sunday)
        {
            return true;
        }
        // @todo Учитывать праздники (сейчас только выходные)
        else
        {
            return false;
        }
    }

    time_duration rise(date d) const
    {
        // @todo Уточнить влияние праздников
        if(d.day_of_week() == Sunday)
        {
            return time_duration(7,30,0);
        }
        else
        {
            return time_duration(6,30,0);
        }
    }

    time_duration all_clear(date d) const
    {
        // @todo Уточнить влияние праздников
        if(d.day_of_week() == Saturday)
        {
            return time_duration(23,30,0);
        }
        else
        {
            return time_duration(22,30,0);
        }
    }

    time_duration work_start(date d) const
    {
        if(this->is_holiday(d))
        {
            return time_duration(24,0,0);
        }

        if(d.day_of_week() == Monday)
        {
            return time_duration(8,0,0);
        }

        return time_duration(8,30,0);
    }

    time_duration work_stop(date d) const
    {
        if(this->is_holiday(d))
        {
            return time_duration(24,0,0);
        }

        if(d.day_of_week() == Friday)
        {
            return time_duration(15,45,0);
        }

        return time_duration(17,30,0);
    }

private:
    using Container = std::vector<overtime_event>;
    Container events;

    void check(date, time_duration start, time_duration stop)
    {
        assert(time_duration(0,0,0) <= start);
        assert(start < stop);
        assert(stop <= time_duration(24,0,0));
    }

    time_duration holiday_time(date d, overtime_period ot) const
    {
        if(this->is_holiday(d))
        {
            return ot.stop - ot.start;
        }
        else
        {
            return {};
        }
    }

    time_duration night_time(date d, overtime_period ot) const
    {
        if(this->is_holiday(d))
        {
            return {};
        }

        auto const total = ot.stop - ot.start;
        auto const at_day = std::max(time_duration{},
                                     std::min(ot.stop, time_duration(22,0,0)) - std::max(ot.start, time_duration(6,0,0)));

        return total - at_day;
    }

    Container::const_iterator
    find_month_end(Container::const_iterator const pos) const
    {
        auto const pred = [m = pos->date.month()](overtime_event const & e)
                          { return e.date.month() > m; };
        return std::find_if(pos, events.cend(), pred);
    }

    using Month_table = std::map<std::string, std::vector<date_overtime>>;

    template <class OStream>
    void format_html_month_table(OStream & output,
                                 Month_table const & month_table,
                                 date const month_first)
    {
        // @todo Разделить подсчёты и построение HTML

        auto const next_month_first = month_first + months(1);

        output << "<table border=\"1\">\n";
        output << "<caption>" << month_first.month() << " " << month_first.year() << "</caption>\n";

        // Первая строка шапки
        output << "<tr>\n";
        auto const days_in_month = (next_month_first - month_first).days();

        output << "<th rowspan='2'>" << "ФИО" << "</th>"
               << "<th colspan='" << days_in_month << "'>" << "Дни месяца" << "</th>"
               << "<th colspan='3'>" << "Всего часов" << "</th>"
               << "<th rowspan='2'>" << "Выходных дней" << "</th>";
        output << "</tr>\n";

        // Вторая строка шапки
        output << "<tr>\n";
        for(auto i = month_first; i != next_month_first; i += days(1))
        {
            auto const bg = std::string(this->is_holiday(i) ? " bgcolor='#00FF00'" : "");
            output << "<th" << bg << ">" << i.day() << "</th>";
        }

        output << "<th>" << "Ночь" << "</th>"
               << "<th>" << "Выходные" << "</th>"
               << "<th>" << "Сверх" << "</th>";

        output << "</tr>\n";

        // Остальные строки табеля
        for(auto const & row : month_table)
        {
            auto total_at_night = time_duration{};
            auto total_at_holidays = time_duration{};
            auto total = time_duration{};
            auto total_holidays = int{0};

            // Выводим периоды переработки
            output << "<tr>\n";

            output << "<td rowspan='3'>" << row.first << "</td>\n";

            for(auto i = month_first; i != next_month_first; i += days(1))
            {
                auto const bg = std::string(this->is_holiday(i) ? " bgcolor='#00FF00'" : "");
                output << "<td" << bg << ">\n";

                bool need_delimiter = false;
                for(auto const & p : row.second.at(i.day() - 1).periods)
                {
                    if(need_delimiter)
                    {
                        output << ", <br>\n";
                    }
                    need_delimiter = true;

                    output << p.start << "-" << p.stop;

                    total_at_holidays += holiday_time(i, p);
                    total_at_night += night_time(i, p);
                    total += (p.stop - p.start);
                }

                if(row.second.at(i.day() - 1).periods.empty() == false)
                {
                    total_holidays += this->is_holiday(i);
                }

                output << "</td>\n";

            }

            output << "<td rowspan='3'>" << to_hours(total_at_night) << "</td>";
            output << "<td rowspan='3'>" << to_hours(total_at_holidays) << "</td>";
            output << "<td rowspan='3'>" << to_hours(total - total_at_night - total_at_holidays) << "</td>";
            output << "<td rowspan='3'>" << total_holidays << "</td>";

            output << "</tr>\n";

            // Выводим суммарную переработку (если она вообще есть)
            output << "<tr>\n";

            for(auto i = month_first; i != next_month_first; i += days(1))
            {
                auto T = time_duration{};

                for(auto const & p : row.second.at(i.day() - 1).periods)
                {
                    T += (p.stop - p.start);
                }

                auto const bg = std::string(this->is_holiday(i) ? " bgcolor='#00FF00'" : "");
                output << "<td" << bg << ">\n";

                if(T > time_duration{})
                {
                    // @todo Установить в качестве десятичный разделителя запятую
                    output << to_hours(T);
                }
                output << "</td>\n";
            }

            output << "</tr>\n";

            // Выводим основания
            output << "<tr>\n";

            for(auto i = month_first; i != next_month_first; i += days(1))
            {
                auto const bg = std::string(this->is_holiday(i) ? " bgcolor='#00FF00'" : "");
                output << "<td" << bg << ">\n";

                ::ural::write_delimited(output, row.second.at(i.day() - 1).orders, ", ");

                output << "</td>\n";
            }

            output << "</tr>\n";
        }

        output << "</table>\n" << "<br>\n";
    }

    template <class OStream>
    Container::const_iterator
    write_month(OStream & output, Container::const_iterator const pos)
    {
        assert(pos != events.end());

        auto const month_end = this->find_month_end(pos);

        auto const month_first = date(pos->date.year(), pos->date.month(), 1);
        auto const next_month_first = month_first + months(1);

        Month_table month_table;

        // Первый проход - собираем фамилии
        for(auto i = pos; i != month_end; ++ i)
        {
            month_table[i->name].resize((next_month_first - month_first).days());
        }

        // Второй проход - собираем переработки
        for(auto i = pos; i != month_end; ++ i)
        {
            auto & cell = month_table[i->name].at(i->date.day() - month_first.day());
            cell.periods.insert(i->period);
            cell.orders.insert(i->order);
        }

        // Рисуем таблицу
        this->format_html_month_table(output, month_table, month_first);

        return month_end;
    }

    template <class OStream>
    void write_months(OStream & output)
    {
        if(events.empty())
        {
            return;
        }

        std::sort(events.begin(), events.end(),
                  ::ural::compare_by(&overtime_event::date));

        for(auto i = events.cbegin(); i != events.end();)
        {
            i = this->write_month(output, i);
        }
    }
};

#include <windows.h>

template <class charT, charT sep>
class punct_facet: public std::numpunct<charT> {
protected:
    charT do_decimal_point() const { return sep; }
};

int main()
{
    std::locale::global(std::locale(""));

    std::ifstream duties("Наряды.txt");

    if(!duties)
    {
        std::cout << "Can't open duties file\n";
    }

    overtime_manager manager;

    // Обработка нарядов
    for(; duties;)
    {
        auto const row = ::ural::read_table_row(duties);

        if(row.empty())
        {
            continue;
        }

        assert(row.size() >= 4);

        auto const name = row.at(0);
        auto const date = from_simple_string(row.at(1));
        auto const type = row.at(2);
        auto const order = row.at(3);

        if(type == "Помощник дежурного" || type == "Дежурный ФПП")
        {
            manager.process(name, date, time_duration(17,30,0), time_duration(24,00,0), order);
            manager.process(name, date + days(1), time_duration(0,0,0), time_duration(2,0,0), order);

            manager.process(name, date + days(1), time_duration(6,0,0), time_duration(17,30,0), order);
        }
        else if(type == "Дежурный КПИС")
        {
            auto const start = manager.is_holiday(date) ? time_duration(18,0,0) : time_duration(20,00,0);

            manager.process(name, date, start, time_duration(24,0,0), order);

            manager.process(name, date + days(1), time_duration(0,0,0), time_duration(7,30,0), order);
        }
        else if(type == "Ответственный")
        {
            // @todo Проверить, что дежурство осуществляется в выходной
            manager.process(name, date, manager.rise(date), manager.all_clear(date), order);
        }
        // @todo Другие типы нарядов
        else
        {
            std::cout << "Unknown duty type "  << type << "\n";
            assert(false);
        }
    }

    // Обработка событий - с точным указанием времени переработки
    std::ifstream orders("Приказы.txt");

    if(!orders)
    {
        std::cout << "Can't open orders file\n";
    }

    for(;orders;)
    {
        auto const row = ::ural::read_table_row(orders);

        if(row.empty())
        {
            continue;
        }

        assert(row.size() == 5);

        auto const name = row.at(0);
        auto const date = from_simple_string(row.at(1));
        auto const start = duration_from_string(row.at(2));
        auto const stop = duration_from_string(row.at(3));
        auto const order = row.at(4);

        manager.process(name, date, start, stop, order);
    }

    auto const report_file_name = std::string("table.html");
    std::ofstream output(report_file_name);

    output.imbue(std::locale(output.getloc(), new punct_facet<char, ','>));
    boost::posix_time::time_facet * facet = new boost::posix_time::time_facet();
    facet->time_duration_format("%H:%M");
    output.imbue(std::locale(output.getloc(), facet));

    manager.write_tables(output);

    ShellExecute(NULL, "open", report_file_name.c_str(), NULL, NULL, SW_SHOWNORMAL);

    return 0;
}
