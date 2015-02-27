#ifndef __DELEGATE_H__
#define __DELEGATE_H__

#include <functional>

class delegate_base {
public:
    virtual ~delegate_base() {}
};


template <class R> struct delegate;

template <class R, class... Args>
struct delegate<R(Args...)> : public delegate_base {
    delegate()=default;

    template <typename A, typename FnR>
    delegate(A* a, FnR fn) : fn_([=](Args... args) -> R {
        return (a->*fn)(args...);
    }) {}

    R operator()(Args... args) { return fn_(std::forward<Args>(args)...); }

    operator bool() { return fn_ != nullptr; }

private:
    typename std::function<R(Args...)> fn_;
};

#endif
