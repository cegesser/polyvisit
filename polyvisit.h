#pragma once


#include <type_traits>
#include <typeinfo>
#include <utility>

namespace visitor_detail {

template <typename T>
const void* get_most_derived_impl(const T *obj, std::true_type)
{
    return obj;
}

template <typename T>
const void* get_most_derived_impl(const T *obj, std::false_type)
{
    return dynamic_cast<const void*>(obj);
}

template <typename T>
const void* get_most_derived(const T *obj)
{
    return get_most_derived_impl(obj, std::integral_constant< bool,
                                                             ! std::is_polymorphic<T>::value
                                                                 || std::is_final<T>::value>());
}

class base_visitor
{
public:
    template <typename T>
    void operator()(const T& obj)
    {
        do_visit(get_most_derived(&obj), typeid(obj));
    }

protected:
    base_visitor() = default;
    base_visitor(const base_visitor &) = default;
    virtual ~base_visitor() {}

private:
    virtual void do_visit(const void* ptr,
                          const std::type_info& type) = 0;
};

template <class... Fs>
struct overload;

template <class F0, class... Frest>
struct overload<F0, Frest...> : F0, overload<Frest...>
{
    overload(F0 f0, Frest... rest) : F0(f0), overload<Frest...>(rest...) {}

    using F0::operator();
    using overload<Frest...>::operator();
};

template <class F0>
struct overload<F0> : F0
{
    overload(F0 f0) : F0(f0) {}

    using F0::operator();
};

template <typename Function, typename ... Types>
class lambda_visitor : public base_visitor
{
public:
    explicit lambda_visitor(Function f)
    : f_(std::move(f)) {}

private:
    template <typename T>
    bool try_visit(const void* ptr, const std::type_info& type)
    {
        if (type == typeid(T))
        {
            f_(*static_cast<const T*>(ptr));
            return true;
        }
        else
            return false;
    }

    template <typename H, typename ... T>
    bool try_visit(const void* ptr, const std::type_info& type, typename std::enable_if<sizeof...(T)!=0,int>::type=0)
    {
        if (type == typeid(H))
        {
            f_(*static_cast<const H*>(ptr));
            return true;
        }
        else
            return try_visit<T...>(ptr, type);
    }

    void do_visit(const void* ptr, const std::type_info& type) override
    {
        try_visit<Types...>(ptr, type);
    }

    Function f_;
};


template<typename F, typename Ret, typename A, typename... Rest>
A
helper(Ret (F::*)(A, Rest...));

template<typename F, typename Ret, typename A, typename... Rest>
A
helper(Ret (F::*)(A, Rest...) const);

template <typename ... Types>
struct type_list {};

template<typename F>
struct first_argument {
    using cv_type = decltype( helper(&F::operator()) );
    using type = std::remove_reference_t< std::remove_cv_t<cv_type> >;
};

template<typename ... F>
struct first_argument_list {
    using type = type_list<typename first_argument<F>::type...>;
};

} //namespace visitor_detail

template <typename T, typename ... F>
auto visit(const T &value, F... funcs)
{
    auto overloaded = visitor_detail::overload<F...>(std::move(funcs)...);
    using visitor = visitor_detail::lambda_visitor<decltype(overloaded), 
                                                   typename visitor_detail::first_argument<F>::type...>;
    visitor v(std::move(overloaded));
    v(value);
}
