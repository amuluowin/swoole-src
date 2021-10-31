/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <mikan.tenny@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#include "swoole.h"
#include "swoole_coroutine.h"
#include "swoole_lock.h"

#ifdef HAVE_SPINLOCK

namespace swoole {

AtomicSpinLock::AtomicSpinLock(int use_in_process) : Lock() {
    if (use_in_process) {
        impl = (sw_atomic_t *)sw_mem_pool()->alloc(sizeof(*impl));
        if (impl == nullptr) {
            throw std::bad_alloc();
        }
        shared_ = true;
    } else {
        impl = new sw_atomic_t();
        shared_ = false;
    }
    type_ = ATOMIC_SPIN_LOCK;
}

void AtomicSpinLock::init() {
    if (pid == 0) {
        pid = (uint32_t)getpid();
        queue_ = new std::queue<Coroutine *>;
    }
}

int AtomicSpinLock::lock() {
    uint32_t i, n;
    init();
    while (1) {
        if (*impl == 0 && sw_atomic_cmp_set(impl, 0, pid)) {
            return 1;
        }
        if (SW_CPU_NUM > 1) {
            for (n = 1; n < SW_SPINLOCK_LOOP_N; n <<= 1) {
                for (i = 0; i < n; i++) {
                    sw_atomic_cpu_pause();
                }

                if (*impl == 0 && sw_atomic_cmp_set(impl, 0, pid)) {
                    return 1;
                }
            }
        }

        if (*impl == pid) {
            Coroutine *co = Coroutine::get_current();
            if (co != nullptr && !co->is_suspending()) {
                queue_->push(co);
                co->yield();
                continue;
            }
        }
        sw_yield();
    }
}

int AtomicSpinLock::lock_rd() {
    return lock();
}

int AtomicSpinLock::unlock() {
    init();
    bool res = sw_atomic_cmp_set(impl, pid, 0);
    if (!queue_->empty()) {
        Coroutine *co = queue_->front();
        queue_->pop();
        if (co->is_suspending()) {
            co->resume();
        }
    }
    return res;
}

int AtomicSpinLock::trylock() {
    init();
    return *impl == 0 && sw_atomic_cmp_set(impl, 0, pid);
}

int AtomicSpinLock::trylock_rd() {
    return trylock();
}

AtomicSpinLock::~AtomicSpinLock() {
    if (queue_ != nullptr) {
        delete queue_;
    }
    if (shared_) {    
        sw_mem_pool()->free((void *) impl);
    } else {
        delete impl;
    }
}
}  // namespace swoole
#endif
