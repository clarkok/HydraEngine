#ifndef _REPLACABLE_H_
#define _REPLACABLE_H_

#include "Common/HydraCore.h"

#include <set>
#include <type_traits>

namespace hydra
{
namespace vm
{

template <typename T>
class Replacable
{
public:
    Replacable()
    {
        static_assert(std::is_base_of<Replacable<T>, T>::value,
            "T should inherit from Replacable<T>");
    }
    Replacable(const Replacable &) = delete;
    Replacable(Replacable &&) = delete;
    Replacable &operator = (const Replacable &) = delete;
    Replacable &operator = (Replacable &&) = delete;

    ~Replacable()
    {
        ReplaceWith(nullptr);
    }

    void ReplaceWith(T *other)
    {
        if (other == this)
        {
            return;
        }

        if (other)
        {
            other->UsedBy.insert(UsedBy.begin(), UsedBy.end());
        }

        for (auto used : UsedBy)
        {
            *used = other;
        }
        UsedBy.clear();
    }

    struct Ref
    {
        Ref(T *ptr = nullptr)
            : Ptr(ptr)
        {
            if (Ptr)
            {
                Ptr->UsedBy.insert(&Ptr);
            }
        }

        Ref(const Ref &other)
            : Ref(other.Ptr)
        { }

        Ref(Ref &&) = delete;

        operator bool() const
        {
            return Ptr != nullptr;
        }

        Ref &operator = (const Ref &other)
        {
            Reset(other.Ptr);
            return *this;
        }

        Ref &operator = (Ref &&) = delete;

        Ref &operator = (T *ptr)
        {
            Reset(ptr);
            return *this;
        }

        ~Ref()
        {
            Reset();
        }

        T *Reset(T *ptr = nullptr)
        {
            auto ret = Ptr;

            if (Ptr == ptr)
            {
                return ret;
            }

            if (Ptr)
            {
                Ptr->UsedBy.erase(&Ptr);
            }

            Ptr = ptr;
            if (Ptr)
            {
                Ptr->UsedBy.insert(&Ptr);
            }

            return ret;
        }

        T *operator -> ()
        {
            return Ptr;
        }

        T &operator * ()
        {
            return *Ptr;
        }

        template <typename U>
        U *To() const
        {
            static_assert(std::is_base_of<T, U>::value,
                "U must inherit from T");

            return dynamic_cast<U*>(Ptr);
        }

        T *Get() const
        {
            return Ptr;
        }

        T *Ptr;
    };

protected:
    std::set<T **> UsedBy;
};

}
}

#endif // _REPLACABLE_H_