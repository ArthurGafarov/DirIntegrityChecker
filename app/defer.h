#include <functional>

class Defer
{
public:
    explicit Defer(std::function<void()> fn) : m_fn(fn) {};
    Defer operator=(const Defer&) = delete;
    ~Defer() { m_fn(); }
private:
    std::function<void()> m_fn;
};
