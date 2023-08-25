// USB(CDC)-CAN Converter Software
//
// Copyright (C) 2023  Waco Giken Co., Ltd. <github@wacogiken.co.jp>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

//--------------------------------------------------------------------+
// include
//--------------------------------------------------------------------+
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "bsp/board.h"
#include "tusb.h"

#include "hardware/irq.h"
#include "can2040.h"
#include "RP2040.h"
#include "pico/stdlib.h"
#include "crc_ccitt.h"

//--------------------------------------------------------------------+
// define
//--------------------------------------------------------------------+
// General
#define BUFSIZE (64)
#define mask(x) ((x)&(BUFSIZE-1))

// Clock
#define SYS_CLOCK (125000000)

// CDC
#define STX ('\002')
#define ETX ('\003')
#define PROT_STS_NONE (0)
#define PROT_STS_EXIT (1)
#define PROT_STS_END  (2)

// GPIO
#define GPIO_RX (9)
#define GPIO_TX (8)
#define GPIO_SW2 (4)
#define GPIO_SW1 (3)
#define GPIO_ERLED (2)
#define GPIO_TXLED (1)
#define GPIO_RXLED (0)

// LED
#define LED_ON  (0)
#define LED_OFF (1)
#define LED_STS_NONE (0)
#define LED_STS_ON   (1)
#define LED_STS_OFF  (2)
#define LED_ON_TIME  (100000)
#define LED_OFF_TIME (50000)

//--------------------------------------------------------------------+
// variable
//--------------------------------------------------------------------+

// CAN
static struct can2040 cbus;
static struct can2040_msg cmsg, cbuf[BUFSIZE];
uint8_t sflag=0, eflag=0;
int8_t push=0, pop=0;

// LED
typedef struct {
  uint32_t time;
  uint8_t status;
  uint8_t fold;
  uint8_t gpio;
} LED;

LED led_tx = {0, LED_STS_NONE, 0, GPIO_TXLED};
LED led_rx = {0, LED_STS_NONE, 0, GPIO_RXLED};
LED led_er = {0, LED_STS_NONE, 0, GPIO_ERLED};

// CDC
unsigned char rbuf[BUFSIZE];
uint8_t no=0;

//--------------------------------------------------------------------+
// CAN Interrupt Callback
//--------------------------------------------------------------------+
static void can2040_cb(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg)
{
    switch(notify) {
    case CAN2040_NOTIFY_RX :
      if (mask(push+1) != mask(pop)) {
        push = mask(push+1);
        cbuf[push] = *msg;
      }
      else {
        eflag = 2;
      }
      break;
    case CAN2040_NOTIFY_TX :
      sflag = 0;
      break;
    case CAN2040_NOTIFY_ERROR :
    default :
      eflag = 1;
      break;
    }
}

//--------------------------------------------------------------------+
// PIO Interrupt Callback
//--------------------------------------------------------------------+
static void PIOx_IRQHandler(void)
{
    can2040_pio_irq_handler(&cbus);
}

//--------------------------------------------------------------------+
// LED Control
//--------------------------------------------------------------------+
void led_ctrl(LED *led, uint8_t flag)
{
  switch(led->status) {
  case LED_STS_NONE:  // OFF -> ON
    if (flag > 0) {
      led->fold = flag;
      led->status = LED_STS_ON;
      led->time = time_us_32() + led->fold * LED_ON_TIME;
      gpio_put(led->gpio, LED_ON);
    }
    break;
  case LED_STS_ON:  // ON -> OFF
    if ((int32_t)(time_us_32()-led->time) > 0) {
      led->status = LED_STS_OFF;
      led->time = time_us_32()+ led->fold * LED_OFF_TIME;
      gpio_put(led->gpio, LED_OFF);
    }
    break;
  case LED_STS_OFF: // OFF -> NONE
  default:
    if ((int32_t)(time_us_32()-led->time) > 0) {
      led->status = LED_STS_NONE;
    }
    break;
  }
}

//--------------------------------------------------------------------+
// CDC packet protocol analysis
//--------------------------------------------------------------------+
uint8_t cdc_read(void)
{
  uint8_t _flag = PROT_STS_NONE;
  unsigned char _c;

  while (_flag == PROT_STS_NONE) {
    if (!tud_cdc_connected()) {
      // not Connected
      eflag = 10;
      no = 0;
      _flag = PROT_STS_EXIT;
      break;
    }

    if (tud_cdc_available()) {
      _c = tud_cdc_read_char();
      switch (_c) {
      case STX:
        no = 0;
        break;
      case ETX:
        _flag = PROT_STS_END;
        break;
      default:
        if (no < BUFSIZE) {
          rbuf[no++] = _c;
        }
        break;
      }
    } else {
      _flag = PROT_STS_EXIT;
      break;
    }
  }

  return _flag;
}

//--------------------------------------------------------------------+
// CDC -> CAN conversion
//--------------------------------------------------------------------+
void cdc_to_can(void)
{
  uint16_t _crc;
  uint8_t _i, _flag;
  char _num[16], *_ptr;

  // flag
  strncpy(_num, (const char *)rbuf+0, 2);
  _num[2]='\0';
  _flag = strtol(_num, &_ptr, 16);
  if (_ptr[0] != '\0') {
    return;
  }

  // ID
  strncpy(_num, (const char *)rbuf+2, 8);
  _num[8]='\0';
  cmsg.id = strtol(_num, &_ptr, 16);
  if (_ptr[0] != '\0') {
    return;
  }
  cmsg.id |= _flag & 0x10 ? CAN2040_ID_EFF : 0;

  // DLC
  cmsg.dlc = _flag & 0xf;

  // Data
  for (_i=0; _i<cmsg.dlc; _i++) {
    strncpy(_num, (const char *)rbuf+10+_i*2, 2);
    _num[2]='\0';

    cmsg.data[_i] = strtol(_num, &_ptr, 16);
    if (_ptr[0] != '\0') {
      return;
    }
  }

  // CRC check
  strncpy(_num, (const char *)rbuf+10+cmsg.dlc*2, 4);
  _num[4]='\0';
  _crc = strtol(_num, &_ptr, 16);
  if (_ptr[0] != '\0') {
    return;
  }

  if (_crc != crc_ccitt(rbuf,	10+cmsg.dlc*2, 0xffff, 0xffff)) {
    return;
  }

  // CAN send
  sflag = 1;
  can2040_transmit(&cbus, &cmsg);
  led_ctrl(&led_tx, 1);
}

//--------------------------------------------------------------------+
// CAN -> CDC conversion
//--------------------------------------------------------------------+
void can_to_cdc(struct can2040_msg _rmsg)
{
  uint16_t _crc;
  uint8_t _flag, _i;
  char _wbuf[BUFSIZE];

  // Start
  _wbuf[0] = STX;

  // flag & ID
  _flag = _rmsg.dlc;
  if (_rmsg.id & CAN2040_ID_EFF) {
    _flag |= 0x10;
  }
  snprintf(_wbuf+1, 16, "%02X", _flag); // Flag
  snprintf(_wbuf+3, 16, "%08X", _rmsg.id & ~CAN2040_ID_EFF); // EID

  // Data
  for (_i=0; _i<_rmsg.dlc; _i++) {
    snprintf(_wbuf+11+_i*2, 16, "%02X", _rmsg.data[_i]); // Data
  }

  // CRC
  _crc = crc_ccitt((unsigned char *)_wbuf+1,	10+_rmsg.dlc*2, 0xffff, 0xffff);
  snprintf(_wbuf+11+_rmsg.dlc*2, 16, "%04X", _crc); // CRC

  // End
  _wbuf[15+_rmsg.dlc*2] = ETX;

  // Rxd LED
  led_ctrl(&led_rx, 1);

  // Check
  if (tud_cdc_connected()) {
    _i = 16+_rmsg.dlc*2;
    if (tud_cdc_write_available() > _i) {
      // Write
      tud_cdc_write(_wbuf, _i);
      tud_cdc_write_flush();
    }
    else {
        // not Write
        eflag = 1;
    }
  }
  else {
    // not Connected
    eflag = 10;
  }
}

//--------------------------------------------------------------------+
// USB CDC task
//--------------------------------------------------------------------+
static void cdc_task(void)
{
  // CDC -> CAN
  if (sflag == 0) {
    if (cdc_read() == PROT_STS_END) {
      cdc_to_can();
    }    
  }

  // CAN -> CDC
  if (mask(push) != mask(pop)) {
    can_to_cdc(cbuf[pop]);
    pop = mask(pop+1);
  }
}

//--------------------------------------------------------------------+
// main
//--------------------------------------------------------------------+
int main(void)
{
  uint32_t _pio_num = 0;
  uint32_t _bitrate;
  uint8_t _i;

  // Pico board initialization
  board_init();

  // switch input initialization
  gpio_init(GPIO_SW1);
  gpio_set_dir(GPIO_SW1, GPIO_IN);
  gpio_pull_up(GPIO_SW1);

  gpio_init(GPIO_SW2);
  gpio_set_dir(GPIO_SW2, GPIO_IN);
  gpio_pull_up(GPIO_SW2);

  // LED output initialization
  gpio_init(GPIO_TXLED);
  gpio_set_dir(GPIO_TXLED, GPIO_OUT);
  gpio_put(GPIO_TXLED, LED_OFF);

  gpio_init(GPIO_RXLED);
  gpio_set_dir(GPIO_RXLED, GPIO_OUT);
  gpio_put(GPIO_RXLED, LED_OFF);

  gpio_init(GPIO_ERLED);
  gpio_set_dir(GPIO_ERLED, GPIO_OUT);
  gpio_put(GPIO_ERLED, LED_OFF);

  // CAN-Bus initialization
  can2040_setup(&cbus, _pio_num);
  can2040_callback_config(&cbus, can2040_cb);

  // interrupt enable
  irq_set_exclusive_handler(PIO0_IRQ_0_IRQn, PIOx_IRQHandler);
  NVIC_SetPriority(PIO0_IRQ_0_IRQn, 1);
  NVIC_EnableIRQ(PIO0_IRQ_0_IRQn);

  // bitrate setting
  _i = (gpio_get(GPIO_SW1)?0:1)+(gpio_get(GPIO_SW2)?0:2);
  switch(_i) {
  case 0:
  default:
    _bitrate = 250000;
    break;
  case 1:
    _bitrate = 500000;
    break;
  case 2:
    _bitrate = 1000000;
    break;
  }

  // CAN-Bus start
  can2040_start(&cbus, SYS_CLOCK, _bitrate, GPIO_RX, GPIO_TX);

  // Tiny USB start
  tusb_init();

  // Loop
 for(;;) {
    tud_task();
    cdc_task();
    if (eflag > 0) {
      led_ctrl(&led_er, eflag);
      eflag = 0;
    }
    led_ctrl(&led_tx, 0);
    led_ctrl(&led_rx, 0);
    led_ctrl(&led_er, 0);
  }

  return 0;
}
