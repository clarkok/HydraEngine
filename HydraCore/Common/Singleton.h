#ifndef _SINGLETON_H_
#define _SINGLETON_H_

namespace hydra
{

template <typename T>
class Singleton
{
public:
    static T* GetInstance()
    {
        static T instance;
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