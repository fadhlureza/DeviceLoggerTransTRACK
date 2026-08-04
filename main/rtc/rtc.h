#pragma once

void rtc_init();
bool rtc_read_and_sync();
bool rtc_set_time(int year, int month, int day, int hour, int min, int sec);