#pragma once

#include "esp_err.h"

class LedManager {
public:
    esp_err_t init();

private:
    static void task_entry(void *arg);
    void task();

    void *strip_handle_ = nullptr;
};
