#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include <arpa/inet.h>
#include <iostream>
#define BUFFER_SIZE 64

class util_timer;  // ~ 前向声明

// ~ 用户数据结构
struct client_data {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

class util_timer {
public:
    util_timer(): prev(NULL), next(NULL) {};
public:
    time_t expire;
    void (*cb_func) (client_data*);
    client_data* user_data;
    util_timer *prev;
    util_timer *next;
};

class sort_timer_lst {
public:
    sort_timer_lst() : head(NULL), tail(NULL) {} // ~ 构造函数
    // ~ 链表被销毁时，删除其中所有的定时器
    ~sort_timer_lst() {
        util_timer *tmp = head;
        while (tmp) {
            head = head->next;
            delete tmp;
            tmp = head;
        }
    }
    // ~ add
    void add_timer(util_timer *timer) {
        if (!timer) return;
        if (!head) {  // ~ 链表为空
            head = tail = timer;
            return;
        }
        if (timer->expire < head->expire) { // ~ timer 期限比 head 还小
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head); // ~ 辅助函数（重载） （本质就是往有序链表中插入数据）
    }

    // ~ adjust
    void adjust_timer(util_timer *timer) {
        if (!timer) return;
        util_timer *tmp = timer->next;
        // ~ 如果目标位于链表尾部，或者重新设置的超时时间仍然小于下一个定时器（每次调整只会增加）
        if (!tmp || (timer->expire < tmp->expire)) return;
        if (timer == head) { // ~ 如果被调整的定时器是head，则需要将head从头部拔下来,重新设置头节点，并插入修改后的head'
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        } else {
            // ~ 拔下当前节点
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next); // ~ 往后插入
        }
    }

    // ~ del_timer
    void del_timer(util_timer *timer) {
        if (!timer) return;
        if ((timer == head) && (timer == tail)) { // ~ 链表只有一个节点
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        if (timer == head) {  // ~ 删除头节点
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if (timer == tail) { // ~ 删除尾节点
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        // ~ 删除中部节点
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete(timer);
    }

    // ~ 心搏函数
    void tick() {
        if (!head) return; // ~ 链表空
        std::cout << "timer tick" << std::endl;
        time_t cur = time(NULL);
        util_timer *tmp = head;
        while (tmp) {
            if (cur < tmp->expire) break;
            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if (head) head->prev = NULL;
            delete tmp;
            tmp = head;
        }

    }

private:
    void add_timer(util_timer *timer, util_timer *lst_head) {
        util_timer* prev = lst_head;
        util_timer* tmp = prev->next;
        while (tmp) {
            if (timer->expire < tmp->expire) {
                prev->next = timer;
                timer->prev = prev;
                timer->next = tmp;
                return;
            }
            prev = prev->next;
            tmp = tmp->next;
        }
        prev->next = timer; // ~ 此时 prev 是 tail 位置
        timer->prev = prev;
        timer->next = NULL;
    }
    

private:
    util_timer *head;
    util_timer *tail;
};


#endif