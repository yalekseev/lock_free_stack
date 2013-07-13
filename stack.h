#include <atomic>
#include <memory>

namespace lock_free {

template <typename T>
class stack {
private:
    struct node;

    struct counted_node_ptr {
        int m_external_count;
        node * m_node_ptr;
    };

    struct node {
        explicit node(const T & data) : m_data(data), m_internal_count(0) { }

        T m_data;
        std::atomic<int> m_internal_count;
        counted_node_ptr m_next;
    };

public:
    stack();
    ~stack();

    stack(const stack & other) = delete;
    stack & operator=(const stack & other) = delete;

    void push(const T & data);

    bool try_pop(T & data);

    // empty()
    // size()

private:
    counted_node_ptr get_head();

    std::atomic<counted_node_ptr> m_head;
};

template <typename T>
stack<T>::stack() {
    counted_node_ptr head;
    head.m_external_count = 1;
    head.m_node_ptr = 0;
    m_head.store(head);
}

template <typename T>
stack<T>::~stack() {
    T data;
    while (try_pop(data)) {
        ;
    }
}

template <typename T>
void stack<T>::push(const T & data) {
    counted_node_ptr new_node;
    new_node.m_node_ptr = new node(data);
    new_node.m_external_count = 1;
    new_node.m_node_ptr->m_next = m_head.load();

    while (!m_head.compare_exchange_weak(new_node.m_node_ptr->m_next, new_node)) { ; }
}

template <typename T>
bool stack<T>::try_pop(T & data) {
    while (true) {
        counted_node_ptr old_head = get_head();

        node * node_ptr = old_head.m_node_ptr;
        if (node_ptr == 0) {
            return false;
        }

        if (m_head.compare_exchange_strong(old_head, node_ptr->m_next)) {
            // The following copy assignment may thow while the head has already
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
typename stack<T>::counted_node_ptr stack<T>::get_head() {
    counted_node_ptr old_head = m_head.load();
    counted_node_ptr new_head;

    do {
        new_head = old_head;
        ++new_head.m_external_count;
    } while (!m_head.compare_exchange_strong(old_head, new_head));

    return new_head;
}

} // namespace lock_free
