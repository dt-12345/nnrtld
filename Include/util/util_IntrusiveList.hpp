#pragma once

#include "types.hpp"

#include <iterator>

namespace nn::util {

class IntrusiveListNode {
public:
    constexpr IntrusiveListNode() noexcept : m_Prev(this), m_Next(this) {}

    constexpr void Initialize() noexcept {
        m_Prev = this;
        m_Next = this;
    }

    constexpr IntrusiveListNode* GetPrev() noexcept { return m_Prev; }
    constexpr const IntrusiveListNode* GetPrev() const noexcept { return m_Prev; }

    constexpr IntrusiveListNode* GetNext() noexcept { return m_Next; }
    constexpr const IntrusiveListNode* GetNext() const noexcept { return m_Next; }
    
    constexpr void InsertBack(IntrusiveListNode* node) noexcept {
        auto prev = node->m_Prev;
        prev->m_Next = this;
        node->m_Prev = m_Prev;
        m_Prev->m_Next = node;
        m_Prev = prev;
    }

private:
    IntrusiveListNode* m_Prev;
    IntrusiveListNode* m_Next;
};

template <typename T, size_t ListNodeOffset>
class IntrusiveList {
public:
    constexpr IntrusiveList() noexcept = default;
    
    using value_type = T;
    using const_value_type = const T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    class iterator {
    public:
        friend class const_iterator;

        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = IntrusiveList::value_type;
        using difference_type = ptrdiff_t;
        using pointer = IntrusiveList::pointer;
        using reference = IntrusiveList::reference;

        constexpr explicit iterator(IntrusiveListNode* node) : m_Node(node) {}
        constexpr explicit iterator(pointer node) noexcept : m_Node(IntrusiveList::GetNode(node)) {}

        constexpr bool operator==(const iterator& other) const noexcept {
            return this->m_Node == other.m_Node;
        }

        constexpr pointer operator->() const {
            return IntrusiveList::Get(m_Node);
        }

        constexpr reference operator*() const {
            return *IntrusiveList::Get(m_Node);
        }

        constexpr iterator& operator++() {
            m_Node = m_Node->GetNext();
            return *this;
        }

        constexpr iterator operator++(std::int32_t) {
            const iterator it = *this;
            m_Node = m_Node->GetNext();
            return it;
        }

        constexpr iterator& operator--() {
            m_Node = m_Node->GetPrev();
            return *this;
        }

        constexpr iterator operator--(std::int32_t) {
            const iterator it = *this;
            m_Node = m_Node->GetPrev();
            return it;
        }

    private:
        IntrusiveListNode* m_Node;
    };

    class const_iterator {
    public:
        friend class iterator;

        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = IntrusiveList::const_value_type;
        using difference_type = ptrdiff_t;
        using pointer = IntrusiveList::const_pointer;
        using reference = IntrusiveList::const_reference;

        constexpr const_iterator(iterator it) noexcept : m_Node(it.m_Node) {}
        constexpr explicit const_iterator(const IntrusiveListNode* node) : m_Node(node) {}
        constexpr explicit const_iterator(pointer node) noexcept : m_Node(IntrusiveList::GetNode(node)) {}

        constexpr bool operator==(const const_iterator& other) const noexcept {
            return this->m_Node == other.m_Node;
        }

        constexpr pointer operator->() const {
            return IntrusiveList::Get(m_Node);
        }

        constexpr reference operator*() const {
            return *IntrusiveList::Get(m_Node);
        }

        constexpr const_iterator& operator++() {
            m_Node = m_Node->GetNext();
            return *this;
        }

        constexpr const_iterator operator++(std::int32_t) {
            const const_iterator it = *this;
            m_Node = m_Node->GetNext();
            return it;
        }

        constexpr const_iterator& operator--() {
            m_Node = m_Node->GetPrev();
            return *this;
        }

        constexpr const_iterator operator--(std::int32_t) {
            const const_iterator it = *this;
            m_Node = m_Node->GetPrev();
            return it;
        }

    private:
        const IntrusiveListNode* m_Node;
    };

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    class reverse_adaptor {
    public:
        constexpr reverse_adaptor(IntrusiveList& list) noexcept : m_List(list) {}

        constexpr reverse_iterator begin() noexcept { return m_List.rbegin(); }
        constexpr reverse_iterator end() noexcept { return m_List.rend(); }

        constexpr const_reverse_iterator cbegin() const noexcept { return m_List.crbegin(); }
        constexpr const_reverse_iterator cend() const noexcept { return m_List.crend(); }
    private:
        IntrusiveList& m_List;
    };

    constexpr iterator begin() noexcept { return iterator(m_Root.GetNext()); }
    constexpr iterator end() noexcept { return iterator(&m_Root); }

    constexpr const_iterator cbegin() const noexcept { return const_iterator(m_Root.GetNext()); }
    constexpr const_iterator cend() const noexcept { return const_iterator(&m_Root); }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    constexpr const_reverse_iterator crbegin() noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crend() noexcept { return const_reverse_iterator(begin()); }

    constexpr reverse_adaptor reverse() noexcept { return reverse_adaptor(*this); }

    constexpr pointer front() noexcept { return Get(m_Root.GetNext()); }
    constexpr pointer back() noexcept { return Get(m_Root.GetPrev()); }

    constexpr const_pointer front() const noexcept { return Get(m_Root.GetNext()); }
    constexpr const_pointer back() const noexcept { return Get(m_Root.GetPrev()); }

    constexpr void InsertBack(pointer p) noexcept {
        m_Root.InsertBack(GetNode(p));
    }

    constexpr bool IsEmpty() const noexcept {
        return m_Root.GetNext() == &m_Root;
    }

private:
    static constexpr pointer Get(IntrusiveListNode* node) noexcept {
        return reinterpret_cast<pointer>(reinterpret_cast<uintptr_t>(node) - ListNodeOffset);
    }

    static constexpr const_pointer Get(const IntrusiveListNode* node) noexcept {
        return reinterpret_cast<const_pointer>(reinterpret_cast<uintptr_t>(node) - ListNodeOffset);
    }

    static constexpr IntrusiveListNode* GetNode(pointer p) noexcept {
        return reinterpret_cast<IntrusiveListNode*>(reinterpret_cast<uintptr_t>(p) + ListNodeOffset);
    }

    static constexpr const IntrusiveListNode* GetNode(const_pointer p) noexcept {
        return reinterpret_cast<const IntrusiveListNode*>(reinterpret_cast<uintptr_t>(p) + ListNodeOffset);
    }

    IntrusiveListNode m_Root;
};

} // namespace nn::util