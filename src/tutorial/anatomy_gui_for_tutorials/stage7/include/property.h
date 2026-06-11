#pragma once
#include "signal.h"
#include <utility>
namespace miniui {
template<typename T>
class Property {
public:
    Property() = default;
    explicit Property(T initial) : value_(std::move(initial)) {}
    Property& operator=(const T& newVal) {
        if(!(value_==newVal)){ value_=newVal; changed_.emit(value_); }
        return *this;
    }
    const T& get()const{ return value_; }
    operator const T&()const{ return value_; }
    Signal<const T&>& changed(){ return changed_; }
private:
    T value_{};
    Signal<const T&> changed_;
};
} // namespace miniui
