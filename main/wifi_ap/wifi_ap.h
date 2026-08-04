#pragma once

#include <stdbool.h>

void wifi_ap_init(void);
void wifi_ap_enable(void);
void wifi_ap_disable(void);
bool wifi_ap_is_enabled(void);