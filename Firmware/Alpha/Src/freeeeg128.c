#include "freeeeg128.h"
#include "control_protocol.h"
#include "main.h"
#include "ads131_board.h"
#include "usbd_cdc_if.h"
#include "settings.h"
#include "runtime.h"
#include "clock_profile.h"
#include "firmware_identity.h"
#include "watchdog.h"
#include <stdio.h>
#include <string.h>
#ifdef FREEEEG128_UF2_APPLICATION
#include "boot_request.h"
#endif

extern UART_HandleTypeDef huart1;
DMA_BUFFER static uint8_t uart_frame[EEG_FRAME_BYTES] __attribute__((aligned(32)));
static uint8_t frame[EEG_FRAME_BYTES], channel_bytes[EEG_CHANNELS * 3];
static unsigned capture_round;
static volatile uint32_t ready_events, rx_write, rx_read, rx_dropped;
static uint8_t rx[512];
static uint32_t seen_events, acquired, missed, dma_errors, usb_dropped, uart_dropped;
static uint32_t capture_started, sequence, rate = 500, settle_until;
static eeg_settings current_settings;
static int settings_dirty, settings_from_flash;
#define USB_QUEUE_FRAMES 16u
static uint8_t usb_queue[USB_QUEUE_FRAMES][EEG_FRAME_BYTES];
static uint32_t usb_head, usb_tail, usb_high_water, control_discarded;
static volatile uint32_t drdy_stamp;
static uint32_t capture_stamp, max_capture_us, max_latency_us, max_work_us, reset_flags;
static uint32_t window_stamp, window_events, window_acquired, window_wait_us;
static uint32_t measured_drdy_millihz, measured_capture_millihz, measured_wait_permille, measured_window_us;
static uint32_t sleep_entries;
static int capturing, streaming = 1, start_after_reply, reboot_after_reply, adc_fault;
static uint32_t adc_fault_mask, last_fault_adc, last_fault_reg, last_fault_value;
static uint32_t readback_mask; /* ADCs whose register readback disagreed with a write. */
static const char *last_fault_op = "NONE";
#ifdef FREEEEG128_UF2_APPLICATION
static int bootloader_after_reply;
#endif
static control_parser parser;
static char reply[1024];
static uint16_t reply_length;
static int reply_in_flight;
static uint32_t last_rx_dropped;

void FreeEEG128_DataReady(uint16_t pin)
{
    if (pin == ADC1_DRDY_Pin) { drdy_stamp = runtime_micros(); ++ready_events; }
}

void FreeEEG128_Receive(const uint8_t *data, uint32_t length)
{
    uint32_t i;
    for (i = 0; i < length; ++i) {
        if (rx_write - rx_read == sizeof(rx)) { ++rx_dropped; continue; }
        rx[rx_write % sizeof(rx)] = data[i];
        __DMB();
        ++rx_write;
    }
}

static void stop_capture(void)
{
    ads_abort(); capturing = 0; seen_events = ready_events;
}
static void adc_problem(unsigned adc, uint8_t reg, const char *operation)
{
    adc_fault = 1; adc_fault_mask |= 1u << adc;
    last_fault_adc = adc + 1; last_fault_reg = reg; last_fault_op = operation;
}
static int readback(unsigned adc, unsigned reg, uint16_t expected, uint16_t mask)
{
    uint16_t actual = 0;
    int ok = ads_read(adc, reg, &actual);
    if (!ok || (actual & mask) != (expected & mask)) {
        readback_mask |= 1u << adc; last_fault_value = actual;
        adc_problem(adc, (uint8_t)reg, "READBACK"); return 0;
    }
    return 1;
}
static int set_rate(uint32_t hz)
{
    stop_capture();
    if (!ads_rate(hz)) { adc_fault = 1; return 0; }
    rate = hz; current_settings.rate = hz;
    settle_until = HAL_GetTick() + 30;
    return 1;
}
static int set_gain(unsigned channel, unsigned value)
{
    if (!ads_gain(channel, value)) {
        adc_problem(channel / 8, (uint8_t)(ADS_GAIN1 + (channel % 8) / 4), "GAIN_WRITE"); return 0;
    }
    current_settings.gains[channel] = (uint8_t)value;
    settle_until = HAL_GetTick() + 30;
    return 1;
}
static uint32_t measure_drdy_hz(void)
{
    uint32_t before = ready_events, start = runtime_micros(), elapsed;
    uint16_t ignored;
    /* Paused ADS131M08 FIFOs can halve DRDY. Keep clocking complete ADC1
       frames throughout the measurement; controls still produce no EEG output. */
    do {
        if (!ads_read(0, 0, &ignored)) { adc_problem(0, 0, "CHECK_SPI"); return 0; }
        elapsed = runtime_micros() - start;
    } while (elapsed < 500000u);
    return (uint32_t)((uint64_t)(ready_events - before) * 1000000u / elapsed);
}

static int apply_settings(const eeg_settings *settings)
{
    eeg_settings copy = *settings;
    unsigned ch;
    int ok = set_rate(copy.rate);
    for (ch = 0; ch < EEG_CHANNELS; ++ch) if (!set_gain(ch, copy.gains[ch])) ok = 0;
    ads_sync();
    settle_until = HAL_GetTick() + 30;
    seen_events = ready_events;
    return ok;
}

static void diagnostics_update(void)
{
    uint32_t now = runtime_micros(), elapsed = now - window_stamp;
    if (!streaming || (int32_t)(HAL_GetTick() - settle_until) < 0) {
        window_stamp = now; window_events = ready_events; window_acquired = acquired; window_wait_us = 0;
        return;
    }
    if (elapsed < 1000000u) return;
    measured_drdy_millihz = (uint32_t)((uint64_t)(ready_events - window_events) * 1000000000u / elapsed);
    measured_capture_millihz = (uint32_t)((uint64_t)(acquired - window_acquired) * 1000000000u / elapsed);
    measured_wait_permille = (uint32_t)((uint64_t)window_wait_us * 1000u / elapsed);
    measured_window_us = elapsed;
    window_stamp = now; window_events = ready_events; window_acquired = acquired; window_wait_us = 0;
}

static void command(control_command cmd)
{
    int ok, was_streaming = streaming;
    diagnostics_update();
    streaming = 0; /* Every control transaction enters a quiet configuration mode. */
    start_after_reply = 0;
    if (capturing) ++control_discarded;
    stop_capture();
    switch (cmd.kind) {
    case CONTROL_HELP:
        if (cmd.first == 1) snprintf(reply, sizeof(reply), "OK HELP SETTINGS !SETTINGS shows all gains/rate; !SAVE persists current settings; !LOAD restores saved settings; !FACTORY erases saved settings and restores 500Hz/gain8; config pauses streaming, !START resumes\r\n");
        else if (cmd.first == 2) snprintf(reply, sizeof(reply), "OK HELP PRESETS !PRESET DEFAULT=500Hz/gain8 FAST=2000Hz/gain8 LOWGAIN=1000Hz/gain1; applies all 128 channels, pauses streaming, does not save; !SAVE persists, !START resumes\r\n");
        else if (cmd.first == 3) snprintf(reply, sizeof(reply), "OK HELP DIAG last full streaming window: drdy_millihz and capture_millihz (divide by 1000 for Hz), max_capture_us, max_latency_us, max_work_us, wait_permille (WFI wait fraction, not CPU utilization), reset_flags hex; !STATUS shows drops; !START resumes\r\n");
        else snprintf(reply, sizeof(reply), "OK HELP !INFO !STATUS !DIAG !CHECK !SETTINGS !STOP !START !RATE 250|500|1000|2000 !GAIN channel(1..128) 1|2|4|8|16|32|64|128 !PRESET DEFAULT|FAST|LOWGAIN !SAVE !LOAD !FACTORY !REBOOT !BOOTLOADER; controls pause stream, !START resumes; !HELP SETTINGS|PRESETS|DIAG\r\n");
        break;
    case CONTROL_INFO:
#ifdef FREEEEG128_UF2_APPLICATION
        snprintf(reply, sizeof(reply), "OK INFO FreeEEG128-alpha " FREEEEG128_VERSION " STM32H743 128ch CDC protocol=1 update=UF2 settings=flash-explicit-save idle=" FREEEEG128_IDLE_NAME " usb_queue=16 cpu_mhz=%lu ahb_mhz=%lu requested_mhz=%u silicon_rev=0x%04lx build=%s watchdog=%s watchdog_nominal_ms=32768 clock=%s\r\n",
            (unsigned long)(SystemCoreClock / 1000000u), (unsigned long)(HAL_RCC_GetHCLKFreq() / 1000000u),
            (unsigned)FREEEEG128_CPU_MHZ, (unsigned long)HAL_GetREVID(), FREEEEG128_BUILD_TAG, watchdog_active() ? "IWDG1" : "not-started", runtime_clock_source());
#else
        snprintf(reply, sizeof(reply), "OK INFO FreeEEG128-alpha " FREEEEG128_VERSION " STM32H743 128ch CDC protocol=1 update=SWD settings=flash-explicit-save idle=" FREEEEG128_IDLE_NAME " usb_queue=16 cpu_mhz=%lu ahb_mhz=%lu requested_mhz=%u silicon_rev=0x%04lx build=%s watchdog=%s watchdog_nominal_ms=32768 clock=%s\r\n",
            (unsigned long)(SystemCoreClock / 1000000u), (unsigned long)(HAL_RCC_GetHCLKFreq() / 1000000u),
            (unsigned)FREEEEG128_CPU_MHZ, (unsigned long)HAL_GetREVID(), FREEEEG128_BUILD_TAG, watchdog_active() ? "IWDG1" : "not-started", runtime_clock_source());
#endif
        break;
    case CONTROL_STATUS:
        snprintf(reply, sizeof(reply), "OK STATUS rate=%lu acquired=%lu missed=%lu dma_errors=%lu usb_dropped=%lu uart_dropped=%lu rx_dropped=%lu adc_fault=%d streaming=0 usb_queued=%lu usb_high_water=%lu control_discarded=%lu adc_fault_mask=0x%lx last_adc=%lu last_reg=0x%02lx last_op=%s uptime_ms=%lu readback_mask=0x%lx\r\n",
            (unsigned long)rate, (unsigned long)acquired, (unsigned long)missed,
            (unsigned long)dma_errors, (unsigned long)usb_dropped, (unsigned long)uart_dropped,
            (unsigned long)rx_dropped, adc_fault, (unsigned long)(usb_head - usb_tail),
            (unsigned long)usb_high_water, (unsigned long)control_discarded, (unsigned long)adc_fault_mask,
            (unsigned long)last_fault_adc, (unsigned long)last_fault_reg, last_fault_op, (unsigned long)HAL_GetTick(),
            (unsigned long)readback_mask); break;
    case CONTROL_CHECK: {
        unsigned i, group, ch;
        uint32_t failed = 0, drdy_hz = measure_drdy_hz();
        int rate_ok = drdy_hz * 100u >= rate * 99u && drdy_hz * 100u <= rate * 101u;
        for (i = 0; i < EEG_ADCS; ++i) {
            int good = rate_ok;
            if (!readback(i, ADS_CLOCK, ads_clock_value(rate), 0xffff)) good = 0;
            if (!readback(i, ADS_MODE, 0x0110, 0xfbff)) good = 0;
            for (group = 0; group < 2; ++group) {
                uint16_t gains = 0;
                for (ch = 0; ch < 4; ++ch) {
                    unsigned code = 0, gain = current_settings.gains[i * 8 + group * 4 + ch];
                    while ((1u << code) < gain) ++code;
                    gains |= (uint16_t)(code << (ch * 4));
                }
                if (!readback(i, ADS_GAIN1 + group, gains, 0x7777)) good = 0;
            }
            if (!good) failed |= 1u << i;
        }
        snprintf(reply, sizeof(reply), "OK CHECK adc_ok_mask=0x%lx adc_failed_mask=0x%lx adc_fault=%d last_adc=%lu last_reg=0x%02lx last_op=%s readback_mask=0x%lx drdy_hz=%lu rate_ok=%d last_value=0x%04lx\r\n",
            (unsigned long)(0xffffu & ~failed), (unsigned long)failed, adc_fault,
            (unsigned long)last_fault_adc, (unsigned long)last_fault_reg, last_fault_op,
            (unsigned long)readback_mask, (unsigned long)drdy_hz, rate_ok, (unsigned long)last_fault_value);
        break;
    }
    case CONTROL_DIAG:
        snprintf(reply, sizeof(reply), "OK DIAG drdy_millihz=%lu capture_millihz=%lu window_us=%lu max_capture_us=%lu max_latency_us=%lu max_work_us=%lu wait_permille=%lu sleep_entries=%lu reset_flags=0x%08lx\r\n",
            (unsigned long)measured_drdy_millihz, (unsigned long)measured_capture_millihz,
            (unsigned long)measured_window_us, (unsigned long)max_capture_us, (unsigned long)max_latency_us,
            (unsigned long)max_work_us, (unsigned long)measured_wait_permille,
            (unsigned long)sleep_entries, (unsigned long)reset_flags); break;
    case CONTROL_SETTINGS: {
        unsigned ch, used = (unsigned)snprintf(reply, sizeof(reply), "OK SETTINGS rate=%lu source=%s dirty=%d gains=",
            (unsigned long)rate, settings_from_flash ? "saved" : "defaults", settings_dirty);
        for (ch = 0; ch < EEG_CHANNELS; ++ch) used += (unsigned)snprintf(reply + used, sizeof(reply) - used,
            "%s%u", ch ? "," : "", current_settings.gains[ch]);
        snprintf(reply + used, sizeof(reply) - used, "\r\n"); break;
    }
    case CONTROL_SAVE:
        ok = adc_fault ? 0 : settings_save(&current_settings);
        if (ok == 1) { settings_from_flash = 1; settings_dirty = 0; snprintf(reply, sizeof(reply), "OK SAVE\r\n"); }
        else snprintf(reply, sizeof(reply), ok == -1 ? "ERR SAVE journal full; FACTORY recycles it\r\n" : "ERR SAVE ADC fault or flash failure\r\n");
        break;
    case CONTROL_LOAD: {
        eeg_settings saved;
        int found = settings_load(&saved);
        ok = apply_settings(&saved);
        if (ok) { settings_from_flash = found; settings_dirty = 0; }
        snprintf(reply, sizeof(reply), ok ? "OK LOAD source=%s\r\n" : "ERR LOAD SPI/readback; reboot before START\r\n", found ? "saved" : "defaults");
        break;
    }
    case CONTROL_FACTORY: {
        eeg_settings defaults;
        settings_defaults(&defaults);
        ok = apply_settings(&defaults) && !adc_fault && settings_factory();
        if (ok) { settings_from_flash = 1; settings_dirty = 0; }
        else settings_dirty = 1;
        snprintf(reply, sizeof(reply), ok ? "OK FACTORY 500Hz gain8 saved\r\n" : "ERR FACTORY ADC/flash failure; stream stopped\r\n");
        break;
    }
    case CONTROL_PRESET: {
        eeg_settings preset;
        settings_defaults(&preset);
        if (cmd.first == 2) preset.rate = 2000;
        if (cmd.first == 3) { preset.rate = 1000; memset(preset.gains, 1, sizeof(preset.gains)); }
        ok = apply_settings(&preset); settings_dirty = 1;
        snprintf(reply, sizeof(reply), ok ? "OK PRESET %s unsaved\r\n" : "ERR PRESET SPI/readback; reboot before START\r\n",
            cmd.first == 2 ? "FAST" : cmd.first == 3 ? "LOWGAIN" : "DEFAULT");
        break;
    }
    case CONTROL_STOP:
        snprintf(reply, sizeof(reply), "OK STOP was_streaming=%d\r\n", was_streaming); break;
    case CONTROL_START:
        if (adc_fault) snprintf(reply, sizeof(reply), "ERR ADC configuration; reboot before START\r\n");
        else { snprintf(reply, sizeof(reply), "OK START\r\n"); start_after_reply = 1; }
        break;
    case CONTROL_RATE:
        ok = set_rate(cmd.first);
        settings_dirty = 1;
        snprintf(reply, sizeof(reply), ok ? "OK RATE %lu\r\n" : "ERR RATE SPI/readback; stream stopped\r\n", (unsigned long)rate);
        break;
    case CONTROL_GAIN: {
        ok = set_gain(cmd.first - 1, cmd.second); settings_dirty = 1;
        snprintf(reply, sizeof(reply), ok ? "OK GAIN %lu %lu\r\n" : "ERR GAIN SPI/readback; stream stopped\r\n",
                 (unsigned long)cmd.first, (unsigned long)cmd.second);
        break;
    }
    case CONTROL_SPIREAD: {
        uint16_t value = 0;
        ok = ads_read(cmd.first - 1, cmd.second, &value);
        snprintf(reply, sizeof(reply), ok ? "OK SPIREAD adc=%lu reg=%lu value=%u\r\n" : "ERR SPIREAD transport/CRC\r\n",
            (unsigned long)cmd.first, (unsigned long)cmd.second, value);
        break;
    }
    case CONTROL_SPIWRITE:
        /* Unrestricted MODE/CLOCK writes could change frame format behind the
           acquisition engine. Diagnostics stay read-only; use RATE/GAIN. */
        snprintf(reply, sizeof(reply), "ERR use RATE/GAIN for verified ADS131M08 writes\r\n"); break;
    case CONTROL_REBOOT:
        snprintf(reply, sizeof(reply), "OK REBOOT\r\n"); reboot_after_reply = 1; break;
    case CONTROL_BOOTLOADER:
#ifdef FREEEEG128_UF2_APPLICATION
        snprintf(reply, sizeof(reply), "OK BOOTLOADER\r\n"); bootloader_after_reply = 1;
#else
        snprintf(reply, sizeof(reply), "ERR install the UF2 factory image with SWD first\r\n");
#endif
        break;
    default:
        snprintf(reply, sizeof(reply), "ERR command; use !HELP\r\n"); break;
    }
    reply_length = (uint16_t)strlen(reply);
}

static void send_frame(void)
{
    frame[0] = 0xa0; frame[1] = (uint8_t)sequence++;
    memcpy(frame + 2, channel_bytes, sizeof(channel_bytes));
    memset(frame + 386, 0, 6); frame[392] = 0xc0;
    /* Queue complete frames without waiting for USB. Preserve the byte-stream
       framing; batching changes USB transfer boundaries, not EEG packets. */
    if (usb_head - usb_tail == USB_QUEUE_FRAMES) ++usb_dropped;
    else {
        memcpy(usb_queue[usb_head % USB_QUEUE_FRAMES], frame, sizeof(frame)); ++usb_head;
        if (usb_head - usb_tail > usb_high_water) usb_high_water = usb_head - usb_tail;
    }
    if (HAL_UART_GetState(&huart1) == HAL_UART_STATE_READY) {
        memcpy(uart_frame, frame, sizeof(frame));
        if (HAL_UART_Transmit_DMA(&huart1, uart_frame, sizeof(frame)) != HAL_OK) ++uart_dropped;
    } else ++uart_dropped;
}

static void pump_usb(void)
{
    uint32_t frames = usb_head - usb_tail, index = usb_tail % USB_QUEUE_FRAMES;
    if (frames > USB_QUEUE_FRAMES - index) frames = USB_QUEUE_FRAMES - index;
    /* At most five frames, below CDC's owned 2048-byte TX buffer. A busy endpoint
       retains the queue. No sample-rate throttling or per-frame delay. */
    if (frames > 5) frames = 5; /* 5*393 = 1965 < CDC owned TX buffer 2048. */
    if (frames && CDC_Transmit_HS(usb_queue[index], (uint16_t)(frames * sizeof(frame))) == USBD_OK)
        usb_tail += frames;
}

static void capture_failed(void)
{
    uint32_t extra = ready_events - seen_events;
    ++dma_errors; ++sequence; missed += extra; sequence += extra;
    stop_capture(); ads_sync(); settle_until = HAL_GetTick() + 30;
    seen_events = ready_events;
}

static void acquire(void)
{
    uint32_t events, stamp, now, primask;
    if (capturing) {
        int complete = ads_round_ready();
        if (complete < 0 || HAL_GetTick() - capture_started >= 10 || ready_events != seen_events) {
            capture_failed(); return;
        }
        if (!complete) return;
        if (!ads_finish_round(capture_round, channel_bytes)) {
            capture_failed(); return;
        }
        if (++capture_round < 4) {
            if (!ads_begin_round(capture_round)) { capture_failed(); }
            return;
        }
        now = runtime_micros() - capture_stamp;
        if (now > max_capture_us) max_capture_us = now;
        capturing = 0;
        /* Drain the two-deep ADC FIFOs throughout filter settling. Pausing
           reads here would retain old conversions behind the next DRDY. */
        if ((int32_t)(HAL_GetTick() - settle_until) >= 0) { ++acquired; send_frame(); }
        return;
    }
    primask = __get_PRIMASK(); __disable_irq();
    events = ready_events; stamp = drdy_stamp; now = runtime_micros();
    __set_PRIMASK(primask);
    if (events == seen_events) return;
    if (events - seen_events > 1) {
        /* Drop the pending period too: a missed conversion fills the ADC
           FIFOs and needs a common resynchronization before valid capture. */
        missed += events - seen_events; sequence += events - seen_events;
        ads_sync(); settle_until = HAL_GetTick() + 30; seen_events = ready_events;
        return;
    }
    seen_events = events; capture_stamp = now;
    if (now - stamp > max_latency_us) max_latency_us = now - stamp;
    capture_started = HAL_GetTick(); capture_round = 0; capturing = 1;
    if (!ads_begin_round(0)) { capture_failed(); }
}

static int usb_ready(void)
{
    extern USBD_HandleTypeDef hUsbDeviceHS;
    USBD_CDC_HandleTypeDef *cdc = hUsbDeviceHS.pClassData;
    return hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED && cdc && !cdc->TxState;
}

/* Called with IRQs masked. Check DMA completion and queued work after masking,
   so an IRQ immediately before WFI cannot be lost until the next millisecond. */
static int work_pending(void)
{
    if (reply_length || reply_in_flight) return usb_ready();
    if (rx_read != rx_write || (usb_head != usb_tail && usb_ready())) return 1;
    if (!streaming) return 0;
    if (!capturing) return ready_events != seen_events;
    if (HAL_GetTick() - capture_started >= 10) return 1;
    return ads_round_ready() != 0 || ready_events != seen_events;
}

static void idle_if_possible(void)
{
    uint32_t primask = __get_PRIMASK();
    if (primask) return;
    __disable_irq();
    if (!work_pending()) {
        uint32_t start = runtime_micros();
        ++sleep_entries;
        runtime_sleep();
        window_wait_us += runtime_micros() - start;
    }
    __set_PRIMASK(primask);
}

static void poll(void)
{
    if (last_rx_dropped != rx_dropped) {
        /* Never execute a command truncated by queue overflow. Resync at newline. */
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        rx_read = rx_write;
        last_rx_dropped = rx_dropped;
        __set_PRIMASK(primask);
        memset(&parser, 0, sizeof(parser)); parser.overflow = 1;
    }
    if (reply_length) {
        /* Drain already acquired binary frames before beginning a text reply. */
        if (usb_head != usb_tail) { pump_usb(); return; }
        if (CDC_Transmit_HS((uint8_t *)reply, reply_length) == USBD_OK) {
            reply_length = 0; reply_in_flight = 1;
        }
        return;
    }
    if (reply_in_flight) {
        extern USBD_HandleTypeDef hUsbDeviceHS;
        uint32_t primask = __get_PRIMASK();
        int busy;
        USBD_CDC_HandleTypeDef *cdc;
        __disable_irq();
        cdc = hUsbDeviceHS.pClassData;
        busy = !cdc || cdc->TxState;
        __set_PRIMASK(primask);
        if (busy) return;
        reply_in_flight = 0;
#ifdef FREEEEG128_UF2_APPLICATION
        if (bootloader_after_reply) { uf2_request_bootloader(); NVIC_SystemReset(); }
#endif
        if (reboot_after_reply) NVIC_SystemReset();
        if (start_after_reply) {
            ads_sync(); settle_until = HAL_GetTick() + 30;
            streaming = 1; start_after_reply = 0; seen_events = ready_events;
        }
    }
    if (rx_read != rx_write) {
        control_command cmd;
        __DMB();
        cmd = control_feed(&parser, rx[rx_read % sizeof(rx)]);
        ++rx_read;
        if (cmd.kind != CONTROL_NONE) command(cmd);
    }
    if (streaming && !reply_length) acquire();
    if (!reply_length) pump_usb();
    diagnostics_update();
}

void FreeEEG128_Run(void)
{
    eeg_settings saved;
    reset_flags = runtime_init();
    settings_from_flash = settings_load(&saved);
    if (!ads_init() || !apply_settings(&saved)) { adc_fault = 1; streaming = 0; }
    watchdog_init(HAL_GetTick());
    window_stamp = runtime_micros(); window_events = ready_events; window_acquired = acquired;
    for (;;) {
        uint32_t start = runtime_micros(), elapsed;
        poll();
        watchdog_service(HAL_GetTick());
        elapsed = runtime_micros() - start;
        if (streaming && !reply_length && elapsed > max_work_us) max_work_us = elapsed;
        idle_if_possible();
    }
}
