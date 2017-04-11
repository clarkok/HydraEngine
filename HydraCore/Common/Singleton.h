#ifndef _SINGLETON_H_
#define _SINGLETON_H_

namespace hydra
{

template <typename T>
class Singleton
{
public:
    template <typename ...T_Args>
    static T* GetInstance(T_Args ...args)
    {
        static T instance(args...);
        return &instance;
    }

    ~Singleton() = default;

protected:
    Singleton() = default;
    Singleton(const Singleton &) = delete;
    Singleton(Singleton &&) = delete;
    Singleton &operator = (const Singleton &) = delete;
    Singleton &operator = (Singleton &&) = delete;
};

}

#endif // _SINGLETON_H_