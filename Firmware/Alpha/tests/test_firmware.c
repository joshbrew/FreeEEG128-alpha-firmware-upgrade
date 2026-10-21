/* Main-loop simulation: actual parser, settings reply and packet queue. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#define __attribute__(x)
#endif
#include "../Src/freeeeg128.c"
UART_HandleTypeDef huart1;
USBD_HandleTypeDef hUsbDeviceHS;
static USBD_CDC_HandleTypeDef cdc;
uint32_t SystemCoreClock = 80000000, test_primask;
static unsigned tick, micros, rounds, finishes, fail_round, adc_fail, transmitted;
static uint8_t transmitted_data[2048];
uint32_t HAL_GetTick(void) { return tick; }
uint32_t HAL_RCC_GetHCLKFreq(void) { return 40000000; }
uint32_t HAL_GetREVID(void) { return 0x2003; }
void HAL_Delay(uint32_t ms) { tick += ms; micros += ms*1000; ready_events += ms*rate/1000; }
void test_set_primask(uint32_t mask) { test_primask = mask; }
int HAL_UART_GetState(UART_HandleTypeDef *uart) { (void)uart; return 1; }
int HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart, uint8_t *buffer, uint16_t size) { (void)uart; (void)buffer; assert(size == 393); return 0; }
uint8_t CDC_Transmit_HS(uint8_t *buffer, uint16_t length)
{ assert(length <= 2048); memcpy(transmitted_data,buffer,length); transmitted=length; return USBD_OK; }
uint32_t runtime_init(void) { return 0; }
uint32_t runtime_micros(void) { return micros++; }
void runtime_sleep(void) {}
static const char *test_clock_source = "HSE";
const char *runtime_clock_source(void) { return test_clock_source; }
void watchdog_init(uint32_t now) { (void)now; }
void watchdog_service(uint32_t now) { (void)now; }
int watchdog_active(void) { return 1; }
void settings_defaults(eeg_settings *s) { s->rate=500; memset(s->gains,8,128); }
int settings_load(eeg_settings *s) { settings_defaults(s); return 0; }
int settings_save(const eeg_settings *s) { (void)s; return 1; }
int settings_factory(void) { return 1; }
int ads_init(void) { return 1; }
void ads_abort(void) {}
void ads_sync(void) {}
int ads_rate(uint32_t hz) { (void)hz; return !adc_fail; }
int ads_gain(unsigned ch,unsigned gain) { assert(ch<128 && gain<=128); return !adc_fail; }
uint16_t ads_clock_value(uint32_t hz) { (void)hz; return 0xff5a; }
int ads_read(unsigned adc,unsigned reg,uint16_t *value)
{ static uint32_t fraction; assert(adc<16); micros += 100; fraction += 101 * rate;
  ready_events += fraction / 1000000u; fraction %= 1000000u; *value=reg==ADS_CLOCK ? 0xff5a : reg==ADS_MODE ? 0x110 : 0x3333; return 1; }
int ads_begin_round(unsigned round) { assert(round<4); ++rounds; return !fail_round; }
int ads_round_ready(void) { return 1; }
int ads_finish_round(unsigned round,uint8_t *out)
{ assert(round<4); ++finishes; memset(out+round*96,(int)(round+1),96); return 1; }
static control_command parse_line(const char *text)
{
    control_parser p={0}; control_command cmd={0};
    while (*text) cmd=control_feed(&p,(uint8_t)*text++);
    return cmd;
}
int main(void)
{
    unsigned i;
    control_command cmd;
    hUsbDeviceHS.pClassData=&cdc; hUsbDeviceHS.dev_state=USBD_STATE_CONFIGURED;
    settings_defaults(&current_settings);
    command(parse_line("!INFO\n")); assert(strstr(reply,"clock=HSE"));
    test_clock_source="HSI";
    command(parse_line("!INFO\n")); assert(strstr(reply,"clock=HSI"));
    test_clock_source="HSE";
    cmd=parse_line("!GAIN 128 128\n"); assert(cmd.kind==CONTROL_GAIN); command(cmd);
    assert(current_settings.gains[127]==128 && settings_dirty);
    assert(parse_line("!GAIN 129 8\n").kind==CONTROL_ERROR);
    assert(parse_line("!RATE 512\n").kind==CONTROL_ERROR);
    assert(parse_line("!RATE 250\n").kind==CONTROL_RATE);
    assert(parse_line("!SPIWRITE 1 2 256\n").kind==CONTROL_ERROR);
    for(i=0;i<128;++i) current_settings.gains[i]=128;
    command(parse_line("!SETTINGS\n"));
    assert(reply_length<sizeof(reply) && reply_length>512 && strstr(reply,"128,128\r\n"));
    settings_defaults(&current_settings);
    command(parse_line("!CHECK\n")); assert(strstr(reply,"adc_ok_mask=0xffff") && strstr(reply,"rate_ok=1"));
    command(parse_line("!START\n")); assert(start_after_reply && !adc_fault);
    reply_length=0; tick=100; seen_events=ready_events; ++ready_events;
    acquire(); for(i=0;i<4;++i) acquire();
    assert(rounds==4 && finishes==4 && acquired==1 && usb_head-usb_tail==1);
    pump_usb(); assert(transmitted==393 && transmitted_data[0]==0xa0 && transmitted_data[392]==0xc0);
    assert(transmitted_data[2]==1 && transmitted_data[98]==2 && transmitted_data[290]==4);
    assert(transmitted_data[386]==0 && transmitted_data[391]==0);
    settle_until=tick+30; ++ready_events; acquire(); for(i=0;i<4;++i) acquire();
    assert(acquired==1 && usb_head==usb_tail); /* Drain during settling, do not emit. */
    tick+=30;
    for(i=0;i<17;++i) send_frame();
    assert(usb_head-usb_tail==16 && usb_dropped==1);
    pump_usb(); assert(transmitted<=1965 && transmitted%393==0);
    stop_capture(); ++ready_events; fail_round=1; acquire(); assert(dma_errors && !capturing);
    adc_fail=1; command(parse_line("!GAIN 1 8\n")); assert(adc_fault);
    command(parse_line("!START\n")); assert(!start_after_reply && strstr(reply,"ERR"));
    puts("Acquisition/control: 128 channels, bounded replies, four DMA rounds, 393-byte frames, USB batching, faults passed.");
    return 0;
}
