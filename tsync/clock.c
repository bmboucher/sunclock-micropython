#include "tsync.h"
#include "py/smallint.h"
#include "py/mphal.h"     // for mp_hal_ticks_us
#include "hardware/rtc.h" // for rtc_hw
#include "shared/timeutils/timeutils.h"

#define US_PER_S 1000000
#define US_PER_MS 1000
#define MS_PER_S 1000
#define DS_PER_S 10
#define S_PER_MIN 60
#define MIN_PER_HR 60
#define HR_PER_DAY 12                     // use a 12-hour clock
#define DS_PER_MIN (S_PER_MIN*DS_PER_S)   // 600
#define MS_PER_DS (MS_PER_S/DS_PER_S)     // 100
#define S_PER_HR (S_PER_MIN*MIN_PER_HR)   // 3600

// "fractional" scales for each hand, as close to 65536 as possible
#define MS_PER_MIN (MS_PER_S*S_PER_MIN)   // 60000
#define DS_PER_HR (DS_PER_MIN*MIN_PER_HR) // 36000
#define S_PER_DAY (S_PER_HR*HR_PER_DAY)   // 43200

#define S_PER_CAL_DAY (S_PER_DAY*2)       // 86400

static uint32_t curr_rtc_0 = 0;
static uint32_t ticks_us_offset = 0;

static inline int32_t ticks_diff(uint32_t start, uint32_t end) {
    return ((end - start + MICROPY_PY_TIME_TICKS_PERIOD / 2) & (MICROPY_PY_TIME_TICKS_PERIOD - 1))
        - MICROPY_PY_TIME_TICKS_PERIOD / 2;
}

static uint32_t get_us(uint32_t rtc_0)
{
    const uint32_t ticks_us = mp_hal_ticks_us();
    if (rtc_0 != curr_rtc_0) {
        ticks_us_offset = ticks_us;
        curr_rtc_0 = rtc_0;
    }
    const int32_t us = ticks_diff(ticks_us_offset, ticks_us);
    if (us < 0) { return 0; }
    if (us > US_PER_S) { return US_PER_S; }
    return (uint32_t)us;
}

static uint32_t epoch_seconds(bool local) {
    uint32_t base_time = timeutils_seconds_since_epoch(
        time[0], time[1], time[2], time[3], time[4], time[5]
    );
    if (!local) return base_time;
    int32_t adj = (int32_t)base_time + S_PER_HR * time_adj[0] + S_PER_MIN * time_adj[1] + time_adj[2];
    return (uint32_t)(adj > 0 ? adj : 0);
}

static uint16_t last_dst_hour = 24; // can't use -1, only 0..23 are valid
static bool cached_dst = false;
static uint32_t last_dst_ts = 0;
static uint32_t dst_start = 0;
static uint32_t dst_end = 0;
// determine if it is currently daylight savings time
static bool is_dst() {
    if (time[3] == last_dst_hour) {
        // only need to update once per hour
        return cached_dst;
    }

    const uint32_t ts = epoch_seconds(false);
    // only need to recalculate dst_start and dst_end once per year
    if (ts < last_dst_ts || ts > dst_end) {
        // get calendar year of ts
        timeutils_struct_time_t tm;
        timeutils_seconds_since_epoch_to_struct_time(ts, &tm);

        // find second sunday in March, 2am
        uint32_t new_dst_start = timeutils_seconds_since_epoch(tm.tm_year, 3, 1, 7, 0, 0);
        timeutils_seconds_since_epoch_to_struct_time(new_dst_start, &tm);
        while (tm.tm_wday != 6) {
            new_dst_start += S_PER_CAL_DAY;
            timeutils_seconds_since_epoch_to_struct_time(new_dst_start, &tm);
        }
        new_dst_start += 7 * S_PER_CAL_DAY; // 1st -> 2nd sunday

        // find first sunday in November, 2am
        uint32_t new_dst_end = timeutils_seconds_since_epoch(tm.tm_year, 11, 1, 6, 0, 0);
        timeutils_seconds_since_epoch_to_struct_time(new_dst_end, &tm);
        while (tm.tm_wday != 6) {
            new_dst_end += S_PER_CAL_DAY;
            timeutils_seconds_since_epoch_to_struct_time(new_dst_end, &tm);
        }

        last_dst_ts = ts;
        dst_start = new_dst_start;
        dst_end = new_dst_end;
    }
    cached_dst = (ts >= dst_start);
    last_dst_hour = time[3];
    return cached_dst;
}

NO_ARG_MP_FUNCTION(update_time, {
    const uint32_t rtc_0 = rtc_hw->rtc_0;
    const uint32_t us = get_us(rtc_0);
    const uint32_t rtc_1 = rtc_hw->rtc_1;

    time[6] = us / US_PER_MS;
    time[5] = ((rtc_0 & RTC_RTC_0_SEC_BITS ) >> RTC_RTC_0_SEC_LSB);
    time[4] = ((rtc_0 & RTC_RTC_0_MIN_BITS ) >> RTC_RTC_0_MIN_LSB);
    time[3] = ((rtc_0 & RTC_RTC_0_HOUR_BITS ) >> RTC_RTC_0_HOUR_LSB);
    time[2] = ((rtc_1 & RTC_RTC_1_DAY_BITS ) >> RTC_RTC_1_DAY_LSB);
    time[1] = ((rtc_1 & RTC_RTC_1_MONTH_BITS ) >> RTC_RTC_1_MONTH_LSB);
    time[0] = ((rtc_1 & RTC_RTC_1_YEAR_BITS ) >> RTC_RTC_1_YEAR_LSB);

    // update TZ adjustment for EST
    time_adj[0] = is_dst() ? -4 : -5;
})

static inline uint16_t get_frac(int32_t value, int32_t denom) {
    // force value to be in [0, denom) - assume denom is in [0,1<<16)
    while (value < 0) { value += denom; }
    while (value >= denom) { value -= denom; }
    return (uint16_t)((((uint32_t)value) << 16) / ((uint32_t)denom));
}

NO_ARG_MP_FUNCTION(update_hand_pos, {
    int32_t ms = ((int32_t)time[6]) + time_adj[3];
    int32_t s  = ((int32_t)time[5]) + time_adj[2];
    int32_t m  = ((int32_t)time[4]) + time_adj[1];
    int32_t h  = ((int32_t)time[3]) + time_adj[0];

    const int32_t total_ms = s * MS_PER_S + ms;
    hand_pos[0] = get_frac(total_ms, MS_PER_MIN);

    const int32_t total_ds = m * DS_PER_MIN + total_ms / MS_PER_DS;
    hand_pos[1] = get_frac(total_ds, DS_PER_HR);

    const int32_t total_s = h * S_PER_HR + total_ds / DS_PER_S;
    hand_pos[2] = get_frac(total_s, S_PER_DAY);
})

static uint64_t epoch_millis(bool local) {
    const uint32_t us = get_us(rtc_hw->rtc_0);
    const uint32_t epoch_s = epoch_seconds(local);
    return (uint64_t)epoch_s*MS_PER_S + us/US_PER_MS;
}
static double epoch_seconds_float(bool local) {
    return ((double)epoch_millis(local))/MS_PER_S;
}

static bool optional_local_helper(mp_uint_t n_args, const mp_obj_t* args) {
    bool local = true;
    if (n_args > 0) {
        if (!mp_obj_is_bool(args[0])) {
            mp_raise_TypeError("local arg must be boolean");
        }
        local = mp_obj_is_true(args[0]);
    }
    return local;
}

static mp_obj_t tsync_epoch_millis(mp_uint_t n_args, const mp_obj_t* args) {
    const bool local = optional_local_helper(n_args, args);
    return mp_obj_new_int(epoch_millis(local));
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tsync_epoch_millis_obj, 0, 1, tsync_epoch_millis);

static mp_obj_t tsync_epoch_seconds_float(mp_uint_t n_args, const mp_obj_t* args) {
    const bool local = optional_local_helper(n_args, args);
    return mp_obj_new_float(epoch_seconds_float(local));
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tsync_epoch_seconds_float_obj, 0, 1, tsync_epoch_seconds_float);

static mp_obj_t tsync_localtime() {
    mp_obj_t dt[7];
    const uint32_t epoch_s = epoch_seconds(true);
    const uint32_t us = get_us(rtc_hw->rtc_0);
    timeutils_struct_time_t tm;
    timeutils_seconds_since_epoch_to_struct_time(epoch_s, &tm);
    dt[0] = mp_obj_new_int(tm.tm_year);
    dt[1] = mp_obj_new_int(tm.tm_mon);
    dt[2] = mp_obj_new_int(tm.tm_mday);
    dt[3] = mp_obj_new_int(tm.tm_hour);
    dt[4] = mp_obj_new_int(tm.tm_min);
    dt[5] = mp_obj_new_int(tm.tm_sec);
    dt[6] = mp_obj_new_int(us);
    return mp_obj_new_tuple(7, dt);
}
MP_DEFINE_CONST_FUN_OBJ_0(tsync_localtime_obj, tsync_localtime);


static mp_obj_t tsync_get_dst_info() {
    mp_obj_t dst_info[3];
    dst_info[0] = mp_obj_new_bool(is_dst());
    dst_info[1] = mp_obj_new_int(dst_start);
    dst_info[2] = mp_obj_new_int(dst_end);
    return mp_obj_new_tuple(3, dst_info);
}
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_dst_info_obj, tsync_get_dst_info);