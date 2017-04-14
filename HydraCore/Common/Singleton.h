#ifndef _SINGLETON_H_
#define _SINGLETON_H_

namespace hydra
{

template <typename T>
class Singleton
{
public:
    template <typename ...T_Args>
    inline static T* GetInstance(T_Args ...args)
    {
        static T *instance = nullptr;
        if (instance)
        {
            return instance;
        }

        return (instance = GetInstanceActual(args...));
    }

    ~Singleton() = default;

protected:
    Singleton() = default;
    Singleton(const Singleton &) = delete;
    Singleton(Singleton &&) = delete;
    Singleton &operator = (const Singleton &) = delete;
    Singleton &operator = (Singleton &&) = delete;

private:
    template <typename ...T_Args>
    inline static T* GetInstanceActual(T_Args ...args)
    {
        static T instance(args...);
        return &instance;
    }
};

}

#endif // _SINGLETON_H_