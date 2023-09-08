#ifndef NBX_POLL_DESC_H_
#define NBX_POLL_DESC_H_

// Forward declarations
class ev_handler;

class poll_desc {
public:
    poll_desc() = default;
    poll_desc& operator=(const poll_desc &v) {
        this->fd = v.fd;
        this->events = v.events;
        this->eh = v.eh;
        return *this;
    }

    int fd = -1;
    uint32_t events = 0;
    ev_handler *eh = nullptr;
};

//= 核心数组, fd生命周期, 以此为准, 仅负责保存fd与处理对象的映射关系
// TODO 在多线程下这是个稀疏数组, 有优化空间
class poll_desc_map {
public:
    poll_desc_map() = delete;
    poll_desc_map(const int arr_size): arr_size(arr_size), arr(new poll_desc[arr_size]()) { }
    ~poll_desc_map() {
        if (this->arr != nullptr) {
            delete[] this->arr;
            this->arr = nullptr;
        }
    }
public:
    inline poll_desc *alloc(const int i) {
        if (i < this->arr_size)
            return &(this->arr[i]);
        return nullptr;
    }

    inline poll_desc *load(const int i) {
        if (i < this->arr_size) {
            poll_desc *p = &(this->arr[i]);
            if (p->fd != -1)
                return p;
        }
        return nullptr;
    }

    void del(const int i) {
        if (i < this->arr_size) {
            poll_desc *p = &(this->arr[i]);
            p->fd     = -1;
            p->eh     = nullptr;
        }
    }
private:
    int arr_size = 0;
    poll_desc *arr = nullptr;
};

#endif // NBX_POLL_DESC_H_
