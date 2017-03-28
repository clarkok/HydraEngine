#ifndef _CONCURRENT_LINKED_LIST_H_
#define _CONCURRENT_LINKED_LIST_H_

#include "HydraCore.h"

#include <initializer_list>
#include <type_traits>

namespace hydra
{

namespace concurrent
{

template <typename T>
struct ForwardLinkedListNode
{
    std::atomic<T *> Next;

    ForwardLinkedListNode(T *next = nullptr)
        : Next(next)
    { }

    virtual ~ForwardLinkedListNode()
    {
        hydra_assert(Next.load() == nullptr, "Forget to free following Nodes");
    }
};


template <typename T>
class ForwardLinkedList
{
public:
    template <typename T>
    using Node = ForwardLinkedListNode<T>;

    ForwardLinkedList()
        : Head(nullptr), Count(0)
    {
        static_assert(std::is_base_of<Node<T>, T>::value, "Argument T of ForwardLinkedList<T> should inhert from ForwardLinkedListNode<T>");
    }

    ForwardLinkedList(std::initializer_list<T *> initializer)
        : ForwardLinkedList()
    {
        auto end = std::rend(initializer);
        for (auto iter = std::rbegin(initializer); iter != end; ++iter)
        {
            Push(*iter);
        }
    }

    virtual ~ForwardLinkedList() = default;

    void Push(T *node)
    {
        hydra_assert(node->Next.load(std::memory_order_relaxed) == nullptr, "Node is linked in other lists");

        T *head = Head.load(std::memory_order_relaxed);
        do
        {
            node->Next.store(head, std::memory_order_release);
        } while (!Head.compare_exchange_weak(head, node, std::memory_order_release, std::memory_order_relaxed));
        Count.fetch_add(1, std::memory_order_relaxed);
    }

    bool Pop(T* &node)
    {
        node = Head.load(std::memory_order_consume);
        while (node && !Head.compare_exchange_weak(
            node,
            node->Next.load(std::memory_order_relaxed),
            std::memory_order_release,
            std::memory_order_acquire));

        if (!node)
        {
            return false;
        }

        Count.fetch_add(-1, std::memory_order_relaxed);

        node->Next.store(nullptr, std::memory_order_release);
        return true;
    }

    void Steal(ForwardLinkedList &other)
    {
        T *otherHead = other.Head.load(std::memory_order_consume);
        while (!other.Head.compare_exchange_weak(otherHead, nullptr, std::memory_order_release, std::memory_order_relaxed));

        if (!otherHead)
        {
            return;
        }

        T *tail = otherHead;
        T *tailNext = tail->Next.load(std::memory_order_relaxed);
        size_t count = 1;

        while (tailNext)
        {
            count++;
            tail = tailNext;
            tailNext = tail->Next.load(std::memory_order_relaxed);
        }

        other.Count.fetch_add(static_cast<size_t>(-static_cast<i64>(count)), std::memory_order_relaxed);

        T *head = Head.load(std::memory_order_relaxed);
        do
        {
            tail->Next.store(head, std::memory_order_release);
        } while (!Head.compare_exchange_weak(head, otherHead, std::memory_order_release, std::memory_order_relaxed));

        Count.fetch_add(count);
    }

private:
    std::atomic<T *> Head;
    std::atomic<size_t> Count;
};

}

}

#endif // _CONCURRENT_LINKED_LIST_H_