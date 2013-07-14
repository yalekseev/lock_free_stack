#ifndef STACK_H
#define STACK_H

#include <atomic>
#include <memory>

namespace lock_free {

template <typename T>
class stack {
private:
    struct node_type;

    struct counted_node_type {
        int m_external_count;
        node_type * m_node_ptr;
    };

    struct node_type {
        explicit node_type(const T & data) : m_data(data), m_internal_count(0) { }

        T m_data;
        std::atomic<int> m_internal_count;
        counted_node_type m_next;
    };

public:
    stack();
    ~stack();

    stack(const stack & other) = delete;
    stack & operator=(const stack & other) = delete;

    /*! \brief Add element to stack. */
    void push(const T & data);

    /*!
        \brief Take the most recently added element from stack. Return true if
               element was taken, false otherwise.
    */
    bool try_pop(T & data);

    /*! \brief Return true if stack is empty, false otherwise. */
    bool unsafe_empty() const;

    /*! \brief Remove all elements from stack. */
    void unsafe_clear();

    // std::size_t unsafe_size() const

private:
    counted_node_type get_head();

    std::atomic<counted_node_type> m_head;
};

template <typename T>
stack<T>::stack() {
    counted_node_type head;
    head.m_external_count = 1;
    head.m_node_ptr = 0;
    m_head.store(head);
}

template <typename T>
stack<T>::~stack() {
    unsafe_clear();
}

template <typename T>
bool stack<T>::unsafe_empty() const {
    return m_head.load().m_node_ptr == 0;
}

template <typename T>
void stack<T>::unsafe_clear() {
    counted_node_type new_head;
    new_head.m_external_count = 1;
    new_head.m_node_ptr = 0;

    counted_node_type counted_node = m_head.exchange(new_head);
    while (counted_node.m_node_ptr != 0) {
        counted_node_type next_counted_node = counted_node.m_node_ptr->m_next;
        delete counted_node.m_node_ptr;
        counted_node = next_counted_node;
    }
}

template <typename T>
void stack<T>::push(const T & data) {
    counted_node_type new_node;
    new_node.m_node_ptr = new node_type(data);
    new_node.m_external_count = 1;
    new_node.m_node_ptr->m_next = m_head.load();

    while (!m_head.compare_exchange_weak(new_node.m_node_ptr->m_next, new_node)) { ; }
}

template <typename T>
bool stack<T>::try_pop(T & data) {
    while (true) {
        counted_node_type old_head = get_head();

        node_type * node_ptr = old_head.m_node_ptr;
        if (node_ptr == 0) {
            return false;
        }

        if (m_head.compare_exchange_strong(old_head, node_ptr->m_next)) {
            // The following copy assignment may throw while the head has already
            // been popped from the stack. So there is no strong exception safety
            // guarantee.
            data = node_ptr->m_data;

            int users_left = old_head.m_external_count - 2;

            if (node_ptr->m_internal_count.fetch_add(users_left) == -users_left) {
                delete node_ptr;
            }

            return true;
        } else if (node_ptr->m_internal_count.fetch_add(-1) == 1) {
            delete node_ptr;
        }
    }
}

template <typename T>
typename stack<T>::counted_node_type stack<T>::get_head() {
    counted_node_type old_head = m_head.load();
    counted_node_type new_head;

    do {
        new_head = old_head;
        ++new_head.m_external_count;
    } while (!m_head.compare_exchange_strong(old_head, new_head));

    return new_head;
}

} // namespace lock_free

#endif
