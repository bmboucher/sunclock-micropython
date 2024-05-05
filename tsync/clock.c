#include "tsync.h"
#include "py/mphal.h"     // for mp_hal_ticks_us
#include "hardware/rtc.h" // for rtc_hw

// this is US_PER_S - ((2^32) % US_PER_S)
#define TICKS_US_ROLLOVER_SHIFT 32704U

#define US_PER_S 1000000U
#define US_PER_MS 1000U
#define MS_PER_S 1000U
#define DS_PER_S 10U
#define S_PER_MIN 60U
#define MIN_PER_HR 60U
#define HR_PER_DAY 12U
#define DS_PER_MIN (S_PER_MIN*DS_PER_S)
#define MS_PER_DS (MS_PER_S/DS_PER_S)
#define S_PER_HR (S_PER_MIN*MIN_PER_HR)

// "fractional" scales for each hand, as close to 65536 as possible
#define MS_PER_MIN (MS_PER_S*S_PER_MIN)   // 60000
#define DS_PER_HR (S_PER_MIN*DS_PER_S)    // 36000
#define S_PER_DAY (S_PER_HR*HR_PER_DAY)   // 43200

static bool ticks_us_offset_defined = false;
static mp_uint_t ticks_us_offset = 0;
static mp_uint_t prev_ticks_us = 0;

static void setup_ticks_us_offset(void)
{
    if (ticks_us_offset_defined) return;
    uint32_t rtc_reg = rtc_hw->rtc_0;
    while (rtc_hw->rtc_0 == rtc_reg && !ticks_us_offset_defined)
    {
        tight_loop_contents();
    }
    if (ticks_us_offset_defined) return;
    ticks_us_offset = mp_hal_ticks_us();
    prev_ticks_us = ticks_us_offset;
    ticks_us_offset_defined = true;
}

static uint32_t get_us(void)
{
    if (!ticks_us_offset_defined) setup_ticks_us_offset();
    const uint32_t ticks_us = mp_hal_ticks_us();
    // check for rollover and update offset if needed
    if (ticks_us < prev_ticks_us) {
        ticks_us_offset = (ticks_us_offset + TICKS_US_ROLLOVER_SHIFT) % US_PER_S;
    }
    prev_ticks_us = ticks_us;

    return (ticks_us > ticks_us_offset
            ? (ticks_us - ticks_us_offset) % US_PER_S
            : (US_PER_S - ((ticks_us_offset - ticks_us) % US_PER_S)));
}

// capture us/rtc time, return true if rtc time remains consistent while
// calculating us
static bool snap_time(uint32_t* us, uint32_t* rtc_0, uint32_t* rtc_1) {
    const uint32_t prev_rtc_0 = rtc_hw->rtc_0;
    *us = get_us();
    *rtc_0 = rtc_hw->rtc_0;
    *rtc_1 = rtc_hw->rtc_1;
    return prev_rtc_0 == *rtc_0;
}

NO_ARG_MP_FUNCTION(update_time, {
    if (!ticks_us_offset_defined) setup_ticks_us_offset();

    uint32_t us = 0;
    uint32_t rtc_0 = 0;
    uint32_t rtc_1 = 0;
    while (!snap_time(&us, &rtc_0, &rtc_1)) {
        tight_loop_contents();
    }

    time[6] = us / US_PER_MS;
    time[5] = ((rtc_0 & RTC_RTC_0_SEC_BITS ) >> RTC_RTC_0_SEC_LSB);
    time[4] = ((rtc_0 & RTC_RTC_0_MIN_BITS ) >> RTC_RTC_0_MIN_LSB);
    time[3] = ((rtc_0 & RTC_RTC_0_HOUR_BITS ) >> RTC_RTC_0_HOUR_LSB);
    time[2] = ((rtc_1 & RTC_RTC_1_DAY_BITS ) >> RTC_RTC_1_DAY_LSB);
    time[1] = ((rtc_1 & RTC_RTC_1_MONTH_BITS ) >> RTC_RTC_1_MONTH_LSB);
    time[0] = ((rtc_1 & RTC_RTC_1_YEAR_BITS ) >> RTC_RTC_1_YEAR_LSB);
})

NO_ARG_MP_FUNCTION(init_time, {
    ticks_us_offset_defined = false;
    setup_ticks_us_offset();
})

NO_ARG_MP_FUNCTION(update_hand_pos, {
    const uint32_t total_ms = time[5] * MS_PER_S + time[6];
    hand_pos[0] = (total_ms << 16) / MS_PER_MIN;

    const uint32_t total_ds = time[4] * DS_PER_MIN + total_ms / MS_PER_DS;
    hand_pos[1] = (total_ds << 16) / DS_PER_HR;

    // convet 24-hour to 12-hour
    uint16_t h = time[3] > HR_PER_DAY ? time[3] - HR_PER_DAY : time[3];

    const uint32_t total_s = h * S_PER_HR + total_ds / DS_PER_S;
    hand_pos[2] = (total_s << 16) / S_PER_DAY;
})
