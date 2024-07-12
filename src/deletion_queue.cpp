#include "deletion_queue.h"

void DeletionQueue::push(std::function<void()>&& function) {
    mDeletors.push_back(function);
}

void DeletionQueue::flush() {
    for (auto it = mDeletors.rbegin(); it != mDeletors.rend(); it++) {
        (*it)(); // call deletor
    }

    mDeletors.clear();
}
