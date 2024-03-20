#include <functional>

class Defer
{
public:
    explicit Defer(std::function<void()> fn) : m_fn(fn) {};
    Defer(const Defer&) = delete;
    Defer(Defer&&) = delete;
    Defer operator=(const Defer&) = delete;
    Defer operator=(Defer&&) = delete;
    ~Defer() { m_fn(); }
private:
    std::function<void()> m_fn;
};
