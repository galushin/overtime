#ifndef Z_URAL_FUNCTIONAL_HPP_INCLUDED
#define Z_URAL_FUNCTIONAL_HPP_INCLUDED

#include <functional>

namespace ural
{
inline namespace v0
{
    template <class C, class T>
    class member_ptr_fn
    {
    public:
        member_ptr_fn(T C::*ptr)
         : ptr_(ptr)
        {}

        T & operator()(C & obj) const
        {
            return obj.*(this->ptr_);
        }

        T const & operator()(C const & obj) const;

    private:
        T C::*ptr_;
    };

    template <class F>
    struct function_type
    {
        using type = F;
    };

    template <class C, class T>
    struct function_type<T C::*>
    {
        using type = member_ptr_fn<C, T>;
    };

    template <class F>
    using function_type_t = typename function_type<F>::type;

    template <class UnaryFunction, class Compare = std::less<>>
    class comparer_by
    {
    public:
        comparer_by(UnaryFunction f, Compare cmp = Compare{})
         : f_(std::move(f))
         , cmp_(std::move(cmp))
        {}

        template <class T1, class T2>
        bool operator()(T1 && x, T2 && y) const
        {
            return this->cmp_(this->f_(std::forward<T1>(x)),
                              this->f_(std::forward<T2>(y)));
        }

    private:
        function_type_t<UnaryFunction> f_;
        function_type_t<Compare> cmp_;
    };

    template <class UnaryFunction>
    comparer_by<UnaryFunction>
    compare_by(UnaryFunction f)
    {
        return comparer_by<UnaryFunction>(std::move(f));
    }
}
// namespace v0
}
// namespace ural

#endif
// Z_URAL_FUNCTIONAL_HPP_INCLUDED
