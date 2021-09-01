
/* Copyright (c) 2010-2020, Peter Barrett
 **
 ** Permission to use, copy, modify, and/or distribute this software for
 ** any purpose with or without fee is hereby granted, provided that the
 ** above copyright notice and this permission notice appear in all copies.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 ** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 ** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
 ** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
 ** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 ** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 ** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 ** SOFTWARE.
 */

#ifndef __ir_input__
#define __ir_input__


// generate events on state changes of IR pin, feed to state machines.
// event timings  have a resolution of HSYNC, timing is close enough between 15720 and 15600 to make this work
// poll to synthesize hid events at every frame
// i know there is a perfectly good peripheral for this in the ESP32 but this seems more fun somehow

uint8_t _ir_last = 0;
uint8_t _ir_count = 0;
uint8_t _keyDown = 0;
uint8_t _keyUp = 0;
uint16_t _K8561KeyCode = 0xFFFF;

void IRAM_ATTR ir_event(uint8_t ticks, uint8_t value); // t is HSYNCH ticks, v is value
void IRAM_ATTR ir_webtv(uint8_t t, uint8_t v);
void IRAM_ATTR ir_K8561(uint8_t t, uint8_t v);
void IRAM_ATTR ir_retcon(uint8_t t, uint8_t v);
void IRAM_ATTR ir_apple(uint8_t t, uint8_t v);
void IRAM_ATTR ir_flashback(uint8_t t, uint8_t v);

int get_hid_apple(uint8_t* dst);
int get_hid_retcon(uint8_t* dst);
int get_hid_flashback(uint8_t* dst);
int get_hid_nes(uint8_t* dst);
int get_hid_snes(uint8_t* dst);
int get_hid_webtv(uint8_t* dst);
int get_hid_serial(uint8_t* dst);
int get_hid_K8561(uint8_t* dst);

typedef class SDL_KEYCODE
{
	public:
		SDL_KEYCODE() {SDL_Scancode = 0; key_mod = 0;}
		uint8_t SDL_Scancode;
		uint8_t key_mod;
} SDL_KEYCODE;

void _K8561_To_SDL_Scancode (uint16_t cmd, SDL_KEYCODE& SDL_Scancode);

inline void IRAM_ATTR ir_sample()
{
    uint8_t ir = (GPIO.in & (1 << IR_PIN)) != 0;
    if (ir != _ir_last)
    {
        ir_event(_ir_count,_ir_last);
        _ir_count = 0;
        _ir_last = ir;
    }
    if (_ir_count != 0xFF)
        _ir_count++;
}

class IRState {
public:
    uint8_t _state = 0;
    uint16_t _code = 0;
    uint16_t _output = 0;
    uint16_t _joy[2] = {0};
    uint16_t _joy_last[2] = {0};
    uint8_t _timer[2] = {0};

    void set(int i, int m, int t)
    {
        uint8_t b = 0;
        if ((m & (GENERIC_LEFT | GENERIC_RIGHT)) == (GENERIC_LEFT | GENERIC_RIGHT))
            b++; // bogus?
        if ((m & (GENERIC_UP | GENERIC_DOWN)) == (GENERIC_UP | GENERIC_DOWN))
            b++; // bogus?
        if (b) {
            //printf("bogus:%04X\n",m);
            return;
        }

        _joy[i] = m;
        _timer[i] = t;
    }

    // make a fake hid event
    int get_hid(uint8_t* dst)
    {
        if (_timer[0] && !--_timer[0])
            _joy[0] = 0;
        if (_timer[1] && !--_timer[1])
            _joy[1] = 0;
        if ((_joy[0] != _joy_last[0]) || (_joy[1] != _joy_last[1])) {
            _joy_last[0] = _joy[0];
            _joy_last[1] = _joy[1];
            dst[0] = 0xA1;
            dst[1] = 0x42;
            dst[2] = _joy[0];
            dst[3] = _joy[0]>>8;
            dst[4] = _joy[1];
            dst[5] = _joy[1]>>8;
            return 6;           // only sent if it changes
        }
        return 0;
    }
};

//==================================================================
//Classic hard wired NES controllers
//==================================================================
#ifdef NES_CONTROLLER
const uint16_t map_nes[8] = 
{
	GENERIC_FIRE | GENERIC_FIRE_A,	//NES_A
	GENERIC_FIRE_B,	//NES_B
	GENERIC_SELECT,	//NES_SELECT
	GENERIC_START,	//NES_START
	GENERIC_UP,		//NES_UP
	GENERIC_DOWN,	//NES_DOWN
	GENERIC_LEFT,	//NES_LEFT
	GENERIC_RIGHT	//NES_RIGHT
};

IRState _nes;
int get_hid_nes(uint8_t* dst)
{
	digitalWrite(NES_CTRL_LATCH, 1);
	delayMicroseconds(0);
	digitalWrite(NES_CTRL_LATCH, 0);
	delayMicroseconds(0);

	uint16_t buttonsA = 0;
	uint16_t buttonsB = 0;
	for (int i = 0; i < 8; i++)
		{
			buttonsA |= (1^digitalRead(NES_CTRL_ADATA)) * map_nes[i];
			buttonsB |= (1^digitalRead(NES_CTRL_BDATA)) * map_nes[i];
			digitalWrite(NES_CTRL_CLK, 0);
			delayMicroseconds(0);
			digitalWrite(NES_CTRL_CLK, 1);
			delayMicroseconds(0);
		}
	//printf("NESCTRL:%04X %04X\n", buttonsA, buttonsB);
  
	//Setup Controller A
	if (buttonsA == (GENERIC_LEFT | GENERIC_SELECT)) buttonsA |= GENERIC_OTHER;	//Press LEFT & SELECT to open file menu
	_nes.set(0,buttonsA,0); // no repeat period

	//Setup Controller B
	_nes.set(1,buttonsB,0); // no repeat period
  
  return _nes.get_hid(dst);		
}
#endif // NES_CONTROLLER

//==================================================================
//Classic hard wired SNES controllers
//==================================================================
#ifdef SNES_CONTROLLER
const uint16_t map_snes[12] = 
{
	GENERIC_FIRE_B,	//SNES_B
	GENERIC_FIRE_C,	//SNES_Y
	GENERIC_SELECT,	//SNES_SELECT
	GENERIC_START,	//SNES_START
	GENERIC_UP,		//SNES_UP
	GENERIC_DOWN,	//SNES_DOWN
	GENERIC_LEFT,	//SNES_LEFT
	GENERIC_RIGHT,	//SNES_RIGHT
	GENERIC_FIRE | GENERIC_FIRE_A,	//SNES_A
	GENERIC_FIRE_X,	//SNES_X
	GENERIC_FIRE_Y,	//SNES_L
	GENERIC_FIRE_Z	//SNES_R
};

IRState _snes;
int get_hid_snes(uint8_t* dst)
{
	digitalWrite(NES_CTRL_LATCH, 1);
	delayMicroseconds(0);
	digitalWrite(NES_CTRL_LATCH, 0);
	delayMicroseconds(0);

	uint16_t buttonsA = 0;
	uint16_t buttonsB = 0;
	for (int i = 0; i < 12; i++)
		{
			buttonsA |= (1^digitalRead(NES_CTRL_ADATA)) * map_snes[i];
			buttonsB |= (1^digitalRead(NES_CTRL_BDATA)) * map_snes[i];
			digitalWrite(NES_CTRL_CLK, 0);
			delayMicroseconds(0);
			digitalWrite(NES_CTRL_CLK, 1);
			delayMicroseconds(0);
		}
	//printf("SNESCTRL:%04X %04X\n", buttonsA, buttonsB);
  
	//Setup Controller A
	if (buttonsA == (GENERIC_LEFT | GENERIC_SELECT)) buttonsA |= GENERIC_OTHER;	//Press LEFT & SELECT to open file menu
	_snes.set(0,buttonsA,0); // no repeat period

	//Setup Controller B
	_snes.set(1,buttonsB,0); // no repeat period
  
  return _snes.get_hid(dst);		
}
#endif // SNES_CONTROLLER

//==========================================================
//==========================================================
// Apple remote NEC code
// pretty easy to adapt to any NEC remote

#ifdef APPLE_TV_CONTROLLER

// Silver apple remote, 7 Bit code
// should work with both silvers and white
#define APPLE_MENU      0x40
#define APPLE_PLAY      0x7A
#define APPLE_CENTER    0x3A
#define APPLE_RIGHT     0x60
#define APPLE_LEFT      0x10
#define APPLE_UP        0x50
#define APPLE_DOWN      0x30

#define APPLE_RELEASE   0x20 // sent after menu and play?

//    generic repeat code
#define NEC_REPEAT    0xAAAA

/*
  9ms preamble ~142
  4.5ms 1 ~71 - start
  2.25ms ~35 - repeat

  32 bits
  0 – a 562.5µs/562.5µs   9ish wide
  1 – a 562.5µs/1.6875ms  27ish wide
*/

IRState _apple;
int get_hid_apple(uint8_t* dst)
{
    if (_apple._output)
    {
        if (_apple._output != NEC_REPEAT)
            _keyDown = (_apple._output >> 8) & 0x7F;  // 7 bit code
        _apple._output = 0;

        uint16_t k = 0;
        switch (_keyDown) {
            case APPLE_UP:     k = GENERIC_UP;     break;
            case APPLE_DOWN:   k = GENERIC_DOWN;   break;
            case APPLE_LEFT:   k = GENERIC_LEFT;   break;
            case APPLE_RIGHT:  k = GENERIC_RIGHT;  break;
            case APPLE_CENTER: k = GENERIC_FIRE;   break;
            case APPLE_MENU:   k = GENERIC_RESET;  break;
            case APPLE_PLAY:   k = GENERIC_SELECT; break;
        }
        _apple.set(0,k,15); // 108ms repeat period
    }
    return _apple.get_hid(dst);
}

// NEC codes used by apple remote
void IRAM_ATTR ir_apple(uint8_t t, uint8_t v)
{
  if (!v) {
    if (t > 32)
      _apple._state = 0;
  } else {
    if (t < 32)
    {
      _apple._code <<= 1;
      if (t >= 12)
        _apple._code |= 1;
      if (++_apple._state == 32)
        _apple._output = _apple._code;    // Apple code in bits 14-8*
    } else {
        if (t > 32 && t < 40 && !_apple._state)  // Repeat 2.25ms pulse 4.5ms start
          _apple._output = NEC_REPEAT;
        _apple._state = 0;
    }
  }
}

#endif // APPLE_TV_CONTROLLER

//==========================================================
//==========================================================
//  Atari Flashback 4 wireless controllers

#ifdef FLASHBACK_CONTROLLER

// HSYNCH period is 44/315*455 or 63.55555..us
// 18 bit code 1.87khz clock
// 2.3ms zero preamble  // 36
// 0 is 0.27ms pulse    // 4
// 1 is 0.80ms pulse    // 13

// Keycodes...
// Leading bit is 1 for player 1 control..

#define PREAMBLE(_t) (_t >= 34 && _t <= 38)
#define SHORT(_t)    (_t >= 2 && _t <= 6)
#define LONG(_t)     (_t >= 11 && _t <= 15)

// Codes come in pairs 33ms apart
// Sequence repeats every 133ms
// bitmap is released if no code for 10 vbls (167ms) or 0x01 (p1) / 0x0F (p2)
// up to 12 button bits, 4 bits of csum/p1/p2 indication

//  called at every loop ~60Hz

IRState _flashback;
int get_hid_flashback(uint8_t* dst)
{
    if (_flashback._output)
    {
        uint16_t m = _flashback._output >> 4;        // 12 bits of buttons
        printf("F:%04X\n",m);
        uint8_t csum = _flashback._output & 0xF;     // csum+1 for p1, csum-1 for p2
        uint8_t s = m + (m >> 4) + (m >> 8);
        if (((s+1) & 0xF) == csum)
            _flashback.set(0,m,15);
        else if (((s-1) & 0xF) == csum)
            _flashback.set(1,m,20);
        _flashback._output = 0;
    }
    return _flashback.get_hid(dst);
}

void IRAM_ATTR ir_flashback(uint8_t t, uint8_t v)
{
  if (_flashback._state == 0)
  {
    if (PREAMBLE(t) && (v == 0))  // long 0, rising edge of start bit
    {
      _flashback._state = 1;
    }
  }
  else
  {
    if (v)
    {
      _flashback._code <<= 1;
      if (LONG(t))
      {
        _flashback._code |= 1;
      }
      else if (!SHORT(t))
      {
        _flashback._state = 0;  // framing error
        return;
      }

      if (++_flashback._state == 19)
      {
        _flashback._output = _flashback._code;
        _flashback._state = 0;
      }
    }
    else
    {
      if (!SHORT(t))
        _flashback._state = 0;  // Framing err
    }
  }
}

#endif // FLASHBACK_CONTROLLER

//==========================================================
//==========================================================
// RETCON controllers
// 75ms keyboard repeat
// Preamble is 0.80ms low, 0.5 high
// Low: 0.57ms=0,0.27,s=1, high 0.37
// 16 bits
// Preamble = 800,500/63.55555 ~ 12.6,7.87
// LOW0 = 8.97
// LOW1 = 4.25
// HIGH = 5.82

#ifdef RETCON_CONTROLLER

// number of 63.55555 cycles per bit
#define PREAMBLE_L(_t) (_t >= 12 && _t <= 14) // 12/13/14 preamble
#define PREAMBLE_H(_t) (_t >= 6 && _t <= 10)  // 8
#define LOW_0(_t)     (_t >= 8 && _t <= 10)   // 8/9/10
#define LOW_1(_t)     (_t >= 4 && _t <= 6)    // 4/5/6

// map retcon to generic
const uint16_t _jmap[] = {
  0x0400, GENERIC_UP,
  0x0200, GENERIC_DOWN,
  0x0100, GENERIC_LEFT,
  0x0080, GENERIC_RIGHT,

  0x1000, GENERIC_SELECT,
  0x0800, GENERIC_START,

  0x0020, GENERIC_FIRE_X,
  0x0040, GENERIC_FIRE_Y,
  0x0002, GENERIC_FIRE_Z,

  0x2000, GENERIC_FIRE_A,
  0x4000, GENERIC_FIRE_B,
  0x0008, GENERIC_FIRE_C,
};

IRState _retcon;
int get_hid_retcon(uint8_t* dst)
{
    if (_retcon._output)
    {
        uint16_t m = 0;
        const uint16_t* jmap = _jmap;
        int16_t i = 12;
        uint16_t k = _retcon._output;
        _retcon._output = 0;
        while (i--)
        {
            if (jmap[0] & k)
                m |= jmap[1];
            jmap += 2;
        }
        printf("R:%04X\n",m);
        _retcon.set(k >> 15,m,20);
    }
    return _retcon.get_hid(dst);
}

void IRAM_ATTR ir_retcon(uint8_t t, uint8_t v)
{
  if (_retcon._state == 0)
  {
    if (v == 0)  {   // start bit
      if (PREAMBLE_L(t))
        _retcon._state = 1;
    }
  }
  else
  {
    if (!v)
    {
      _retcon._code <<= 1;
      if (LOW_1(t))
        _retcon._code |= 1;
      if (_retcon._state++ == 16)
      {
        _retcon._output = _retcon._code;
        _retcon._state = 0;
      }
    }
  }
}

#endif // RETCON_CONTROLLER


//==========================================================
//==========================================================
//  Webtv keyboard
#ifdef WEBTV_KEYBOARD

#define BAUDB   12  // Width of uart bit in HSYNCH
#define WT_PREAMBLE(_t) (_t >= 36 && _t <= 40)   // 3.25 baud
#define SHORTBIT(_t) (_t >= 9 && _t <= 13)     // 1.5ms ish

// converts webtv keyboard to common scancodes
const uint8_t _ir2scancode[128] = {
    0x00, // 00
    0x00, // 02
    0x05, // 04 B
    0x00, // 06
    0x00, // 08
    0x51, // 0A Down
    0x00, // 0C
    0x00, // 0E
    0x00, // 10
    0x50, // 12 Left
    0xE6, // 14 Right Alt
    0x38, // 16 /
    0xE2, // 18 Left Alt
    0x4F, // 1A Right
    0x2C, // 1C Space
    0x11, // 1E N
    0x32, // 20 #
    0x00, // 22
    0x22, // 24 5
    0x41, // 26 F8
    0x3B, // 28 F2
    0xE4, // 2A Right Ctrl
    0x00, // 2C
    0x2E, // 2E =
    0x3A, // 30 F1
    0x4A, // 32 Home
    0x00, // 34
    0x2D, // 36 -
    0xE0, // 38 Left Ctrl
    0x35, // 3A `
    0x42, // 3C F9
    0x23, // 3E 6
    0x00, // 40
    0x00, // 42
    0x19, // 44 V
    0x37, // 46 .
    0x06, // 48 C
    0x68, // 4A F13
    0xE5, // 4C Right Shift
    0x36, // 4E ,
    0x1B, // 50 X
    0x4D, // 52 End
    0x00, // 54
    0x00, // 56
    0x1D, // 58 Z
    0x00, // 5A
    0x28, // 5C Return
    0x10, // 5E M
    0x00, // 60
    0xE7, // 62 Right GUI
    0x09, // 64 F
    0x0F, // 66 L
    0x07, // 68 D
    0x4E, // 6A PageDown
    0x00, // 6C
    0x0E, // 6E K
    0x16, // 70 S
    0x4B, // 72 PageUp
    0x00, // 74
    0x33, // 76 ;
    0x04, // 78 A
    0x00, // 7A
    0x31, // 7C |
    0x0D, // 7E J
    0x00, // 80
    0x00, // 82
    0x17, // 84 T
    0x40, // 86 F7
    0x3C, // 88 F3
    0x00, // 8A
    0xE1, // 8C Left Shift
    0x30, // 8E ]
    0x39, // 90 CapsLock
    0x00, // 92
    0x29, // 94 Escape
    0x2F, // 96 [
    0x2B, // 98 Tab
    0x00, // 9A
    0x2A, // 9C Backspace
    0x1C, // 9E Y
    0x00, // A0
    0x00, // A2
    0x21, // A4 4
    0x26, // A6 9
    0x20, // A8 3
    0x44, // AA F11
    0x00, // AC
    0x25, // AE 8
    0x1F, // B0 2
    0x00, // B2
    0x46, // B4 PrintScreen
    0x27, // B6 0
    0x1E, // B8 1
    0x45, // BA F12
    0x43, // BC F10
    0x24, // BE 7
    0x00, // C0
    0x00, // C2
    0x0A, // C4 G
    0x00, // C6
    0x3D, // C8 F4
    0x00, // CA
    0x00, // CC
    0x00, // CE
    0x3E, // D0 F5
    0x52, // D2 Up
    0xE3, // D4 Left GUI
    0x34, // D6 '
    0x29, // D8 Escape
    0x48, // DA Pause
    0x3F, // DC F6
    0x0B, // DE H
    0x00, // E0
    0x00, // E2
    0x15, // E4 R
    0x12, // E6 O
    0x08, // E8 E
    0x00, // EA
    0x00, // EC
    0x0C, // EE I
    0x1A, // F0 W
    0x00, // F2
    0x53, // F4 Numlock
    0x13, // F6 P
    0x14, // F8 Q
    0x00, // FA
    0x00, // FC
    0x18, // FE U
};


// IR Keyboard State
uint8_t _state = 0;
uint16_t _code = 0;
uint8_t _wt_keys[6] = {0};
uint8_t _wt_expire[6] = {0};
uint8_t _wt_modifiers = 0;

static
uint8_t parity_check(uint8_t k)
{
    uint8_t c;
    uint8_t v = k;
    for (c = 0; v; c++)
      v &= v-1;
    return (c & 1) ? k : 0;
}

// make a mask from modifier keys
static
uint8_t ctrl_mask(uint8_t k)
{
    switch (k) {
        case 0x38:  return KEY_MOD_LCTRL;
        case 0x8C:  return KEY_MOD_LSHIFT;
        case 0x18:  return KEY_MOD_LALT;
        case 0xD4:  return KEY_MOD_LGUI;
        case 0x2A:  return KEY_MOD_RCTRL;
        case 0x4C:  return KEY_MOD_RSHIFT;
        case 0x14:  return KEY_MOD_RALT;
        case 0x62:  return KEY_MOD_RGUI;
    }
    return 0;
}

// update state of held keys
// produce a hid keyboard record
int get_hid_webtv(uint8_t* dst)
{
    bool dirty = false;
    uint8_t k = parity_check(_keyUp);
    _keyUp = 0;
    if (k) {
       _wt_modifiers &= ~ctrl_mask(k);
        for (int i = 0; i < 6; i++) {
            if (_wt_keys[i] == k) {
                _wt_expire[i] = 1;  // key will expire this frame
                break;
            }
        }
    }

    k = parity_check(_keyDown);
    _keyDown = 0;
    if (k) {
        _wt_modifiers |= ctrl_mask(k);

        // insert key into list of pressed keys
        int j = 0;
        for (int i = 0; i < 6; i++) {
            if ((_wt_keys[i] == 0) || (_wt_expire[i] == 0) || (_wt_keys[i] == k)) {
                j = i;
                break;
            }
            if (_wt_expire[i] < _wt_expire[j])
                j = i;
        }
        if (_wt_keys[j] != k) {
            _wt_keys[j] = k;
            dirty = true;
        }
        _wt_expire[j] = 8;  // key will be down for ~130ms or 8 frames
    }

    // generate hid keyboard events if anything was pressed or changed...
    // A1 01 mods XX k k k k k k
    dst[0] = 0xA1;
    dst[1] = 0x01;
    dst[2] = _wt_modifiers;
    dst[3] = 0;
    int j = 0;
    for (int i = 0; i < 6; i++) {
        dst[4+i] = 0;
        if (_wt_expire[i]) {
            if (!--_wt_expire[i])
                dirty = true;
        }
        if (_wt_expire[i] == 0) {
            _wt_keys[i] = 0;
        } else {
            dst[4 + j++] = _ir2scancode[_wt_keys[i] >> 1];
        }
    }
    return dirty ? 10 : 0;
}

// WebTV UART like keyboard protocol
// 3.25 bit 0 start preamble the 19 bits
// 10 bit code for keyup, keydown, all keys released etc
// 7 bit keycode + parity bit.
//

#define KEYDOWN     0x4A
#define KEYUP       0x5E
void IRAM_ATTR ir_webtv(uint8_t t, uint8_t v)
{
  if (_state == 0)
  {
    if (WT_PREAMBLE(t) && (v == 0))  // long 0, rising edge of start bit
      _state = 1;
  }
  else if (_state == 1)
  {
    _state = (SHORTBIT(t) && (v == 1)) ? 2 : 0;
  }
  else
  {
      t += BAUDB>>1;
      uint8_t bits = _state-2;
      while ((t > BAUDB) && (bits < 16))
      {
          t -= BAUDB;
          _code = (_code << 1) | v;
          bits++;
      }
      if (bits == 16)
      {
        v = t <= BAUDB;
        uint8_t md = _code >> 8;
        _code |= v;                 // Low bit of code is is parity
        if (md == KEYDOWN)
            _keyDown = _code;
        else if (md == KEYUP)
            _keyUp = _code;
        _state = 0;                 // got one
        return;
      }
      _state = bits+2;
    }
}
#endif // WEBTV_KEYBOARD

// called from interrupt
void IRAM_ATTR ir_event(uint8_t t, uint8_t v)
{
#ifdef WEBTV_KEYBOARD
    ir_webtv(t,v);
#endif
#ifdef K8561_KEYBOARD
    ir_K8561(t,v);
#endif
#ifdef RETCON_CONTROLLER
    ir_retcon(t,v);
#endif
#ifdef APPLE_TV_CONTROLLER
    ir_apple(t,v);
#endif
#ifdef FLASHBACK_CONTROLLER
    ir_flashback(t,v);
#endif
}

// called every frame from emu
int get_hid_ir(uint8_t* dst)
{
    int n = 0;
#ifdef APPLE_TV_CONTROLLER
    if (n = get_hid_apple(dst))
        return n;
#endif
#ifdef RETCON_CONTROLLER
    if (n = get_hid_retcon(dst))
        return n;
#endif
#ifdef FLASHBACK_CONTROLLER
    if (n = get_hid_flashback(dst))
        return n;
#endif
#ifdef NES_CONTROLLER
    if (n = get_hid_nes(dst))
        return n;
#endif
#ifdef SNES_CONTROLLER
    if (n = get_hid_snes(dst))
        return n;
#endif
#ifdef WEBTV_KEYBOARD
        return get_hid_webtv(dst);
#endif
#ifdef SERIAL_KEYBOARD
		return get_hid_serial(dst);
#endif
#ifdef K8561_KEYBOARD
		return get_hid_K8561(dst);
#endif
	return 0;
}


//==========================================================
//==========================================================
//  K8561 keyboard
#ifdef K8561_KEYBOARD

#define STATE_LEFT_SHIFT    0x01
#define STATE_RIGHT_SHIFT   0x02
#define STATE_LEFT_CTRL     0x04
#define STATE_RIGHT_CTRL     0x08
#define STATE_LEFT_ALT      0x10
#define STATE_RIGHT_ALT     0x20

#if (1)
// Will be called every frame (25Hz/30Hz)
int get_hid_K8561(uint8_t* dst)
{
	bool dirty = false;
	SDL_KEYCODE SDL_Code;
	
	static bool bKeyDown = false;
	static uint16_t lastK8561KeyCode = 0xFFFF;
	static int wt_expire = 0;
	
	// if (_K8561KeyCode != lastK8561KeyCode)
	if (_K8561KeyCode != 0xFFFF)
	{
		lastK8561KeyCode = _K8561KeyCode;

		_K8561_To_SDL_Scancode(_K8561KeyCode, SDL_Code);

		bKeyDown = ((_K8561KeyCode & 0x0100) != 0) ? false : true;
#if (0)
		printf("data:0x%04x\t -> aascii=0x%02x mod=0x%02x %\t", _K8561KeyCode, SDL_Code.SDL_Scancode, SDL_Code.key_mod);
		for (int k = 15; k >= 0; k--)
		   printf("%c", (_K8561KeyCode & 1 << k) ? '1' : '0');
		printf("\n");		
#endif
		_K8561KeyCode = 0xFFFF;
		if (bKeyDown)
		{
			printf("Key down\n");
			if (SDL_Code.SDL_Scancode != 0)
			{
				// generate hid keyboard events if anything was pressed or changed...
				// A1 01 mods XX k k k k k k
				dst[0] = 0xA1;
				dst[1] = 0x01;
				dst[2] = SDL_Code.key_mod;
				dst[3] = 0;
				dst[4] = SDL_Code.SDL_Scancode;  // Scancode
				dirty = true;
				wt_expire = 40;
			}
		}
		else
		{
			printf("Key up\n");
			dst[0] = 0xA1;
			dst[1] = 0x01;
			dst[2] = SDL_Code.key_mod;
			dst[3] = 0;
			dst[4] = SDL_Code.SDL_Scancode;
			dirty = true;
			wt_expire = 1;
		}	
	}
	else
	{
		if (wt_expire > 0)
		{
			wt_expire--;
			if (wt_expire == 0)
			{
				printf("Force key up\n");			
				dst[0] = 0xA1;
				dst[1] = 0x01;
				dst[2] = 0;
				dst[3] = 0;
				dst[4] = 0;  // Scancode
				dirty = true;	
			}
		}		
	}
	return dirty ? 10 : 0;
}
#else
// Will be called every frame (25Hz/30Hz)
int get_hid_K8561(uint8_t* dst)
{
	bool dirty = false;
	static int wt_expire = 0;
	int wt_modifiers = 0;
	SDL_KEYCODE SDL_Code;
	
	static int counter = 0;

	// To prevent too many keystrokes from being sent
	// only communicate the last keystroke every 200ms.
	if ((counter++ >= 10) && (_K8561KeyCode != 0xffff))
	{
		_K8561_To_SDL_Scancode(_K8561KeyCode, SDL_Code);
#if (0)
		printf("data:0x%04x\t -> aascii=0x%02x mod=0x%02x %\t", _K8561KeyCode, SDL_Code.SDL_Scancode, SDL_Code.key_mod);
		for (int k = 15; k >= 0; k--)
		   printf("%c", (_K8561KeyCode & 1 << k) ? '1' : '0');
		printf("\n");		
#endif
  	    _K8561KeyCode = 0xffff;
	    counter = 0;	
	}	
	
	if (SDL_Code.SDL_Scancode != 0)
	{
		// generate hid keyboard events if anything was pressed or changed...
		// A1 01 mods XX k k k k k k
		dst[0] = 0xA1;
		dst[1] = 0x01;
		dst[2] = SDL_Code.key_mod;
		dst[3] = 0;
		dst[4] = SDL_Code.SDL_Scancode;  // Scancode
		wt_expire = 10;
		dirty = true;
	}
	else
	{
		if (wt_expire-- == 0)
		{
			dst[0] = 0xA1;
			dst[1] = 0x01;
			dst[2] = 0;
			dst[3] = 0;
			dst[4] = 0;  // Scancode
			dirty = true;
		}			
	}
	
    return dirty ? 10 : 0;
}
#endif


#define START(_t) (_t >= 23 && _t <= 28) // Streuung zwischen 25 und 26
#define SHORT(_t) (_t >= 5 && _t <= 8)   // Streuung zwischen 6 und 7
#define LONG(_t)  (_t >= 11 && _t <= 15) // Streuung zwischen 12 und 14
void IRAM_ATTR ir_K8561(uint8_t t, uint8_t ir)
{
    static uint16_t code = 0;
    static uint8_t bit = 0;
    static bool bStartBit = false;

    if (ir == 1)
    {
      if (bStartBit == false)
      {
          if (START(t)==true)
          {
            bit = 0;
            code = 0;
            bStartBit = true;
          }
      }
      else
      {
          if (SHORT(t)==true)
          {
               code = code << 1;
               bit++;
          }
          else if (LONG(t)==true)
          {
              code = (code<<1) | 1;
              bit++;
          }
          else // Invalid bit length
          {
              bStartBit = false;
          }

          if (bit == 12)
          {
              bStartBit = false;
              _K8561KeyCode = code;
          }
      }
   }
}

void _K8561_To_SDL_Scancode(uint16_t cmd, SDL_KEYCODE& SDL_Code)
{
    static uint8_t state;

    uint8_t SDL_Scancode = 0;
	uint8_t key_mod = 0;
	
    switch (cmd)
    {
        case 0x06fc: state |=  STATE_LEFT_SHIFT;    break;              // pressed left shift
        case 0x07fc: state &= ~STATE_LEFT_SHIFT;    break;              // released left shift      
        case 0x0c2a: state |=  STATE_RIGHT_SHIFT;   break;              // pressed right shift
        case 0x0d2a: state &= ~STATE_RIGHT_SHIFT;   break;              // released right shift
        case 0x02b2: state |=  STATE_LEFT_CTRL;     break;              // pressed left ctrl
        case 0x03b2: state &= ~STATE_LEFT_CTRL;     break;              // released left ctrl
        case 0x0cc2: state |=  STATE_LEFT_ALT;      break;              // pressed left alt
        case 0x0dc2: state &= ~STATE_LEFT_ALT;      break;              // released left alt
        case 0x02e2: state |=  STATE_RIGHT_ALT;     break;              // pressed right alt
        case 0x03e2: state &= ~STATE_RIGHT_ALT;     break;              // released right alt
        case 0x0d4c:                                                    // caps lock
        {
          // Toggle the shift state
          if (state & (STATE_LEFT_SHIFT | STATE_RIGHT_SHIFT)) 
          {
            state &= ~STATE_LEFT_SHIFT;
            state &= ~STATE_RIGHT_SHIFT;
          }
          else
          {
            state |=  STATE_LEFT_SHIFT;
          }
          break;
        }
	}

    cmd &= 0xFEFF; // Clear the KeyUp Bit
    if (state & (STATE_LEFT_SHIFT | STATE_RIGHT_SHIFT))
    {
        switch (cmd)
        {
          case 0x0444:	SDL_Scancode = 0x1E; key_mod = KEY_MOD_LSHIFT;            break; // '!'
          case 0x0cc4:  SDL_Scancode = 0x34; key_mod = KEY_MOD_LSHIFT;            break; // '\'
          case 0x0ca4:  SDL_Scancode = 0x21; key_mod = KEY_MOD_LSHIFT;            break; // '$'
          case 0x0c64:  SDL_Scancode = 0x22; key_mod = KEY_MOD_LSHIFT;            break; // '%'
          case 0x02e4:  SDL_Scancode = 0x24; key_mod = KEY_MOD_LSHIFT;            break; // '&'
          case 0x0000:  SDL_Scancode = 0x38; key_mod = 0;                         break; // '/'
          case 0x0880:  SDL_Scancode = 0x26; key_mod = KEY_MOD_LSHIFT;            break; // '('
          case 0x0840:  SDL_Scancode = 0x27; key_mod = KEY_MOD_LSHIFT;            break; // ')'
          case 0x04c0:  SDL_Scancode = 0x2E; key_mod = 0;                         break; // '='
          case 0x0414:  SDL_Scancode = 0x38; key_mod = KEY_MOD_LSHIFT;            break; // '?'
          case 0x0c34:  SDL_Scancode = 0x14; key_mod = 0;  			  break; // 'Q'
          case 0x02b4:  SDL_Scancode = 0x1A; key_mod = 0;  			  break; // 'W'
          case 0x0274:  SDL_Scancode = 0x08; key_mod = 0;  			  break; // 'E'
          case 0x0af4:  SDL_Scancode = 0x15; key_mod = 0;  			  break; // 'R'
          case 0x040c:  SDL_Scancode = 0x17; key_mod = 0;  			  break; // 'T'
          case 0x0c8c:  SDL_Scancode = 0x1D; key_mod = 0;  			  break; // 'Z'
          case 0x0820:  SDL_Scancode = 0x18; key_mod = 0;  			  break; // 'U'
          case 0x04a0:  SDL_Scancode = 0x0C; key_mod = 0;  			  break; // 'I'
          case 0x0460:  SDL_Scancode = 0x12; key_mod = 0;  			  break; // 'O'
          case 0x0ce0:  SDL_Scancode = 0x13; key_mod = 0;  			  break; // 'P'
          case 0x0aea:  SDL_Scancode = 0x25; key_mod = KEY_MOD_LSHIFT;            break; // '*'
          case 0x02cc:  SDL_Scancode = 0x04; key_mod = 0; 			  break; // 'A'
          case 0x0c2c:  SDL_Scancode = 0x16; key_mod = 0; 			  break; // 'S'
          case 0x02ac:  SDL_Scancode = 0x07; key_mod = 0; 			  break; // 'D'
          case 0x026c:  SDL_Scancode = 0x09; key_mod = 0; 			  break; // 'F'
          case 0x0aec:  SDL_Scancode = 0x0A; key_mod = 0; 			  break; // 'G'
          case 0x0c1c:  SDL_Scancode = 0x0B; key_mod = 0; 			  break; // 'H'
          case 0x0810:  SDL_Scancode = 0x0D; key_mod = 0; 			  break; // 'J'
          case 0x0490:  SDL_Scancode = 0x0E; key_mod = 0; 			  break; // 'K'
          case 0x0450:  SDL_Scancode = 0x0F; key_mod = 0; 			  break; // 'L'
          case 0x029c:  SDL_Scancode = 0x1C; key_mod = 0;  			  break; // 'Y'
          case 0x025c:  SDL_Scancode = 0x1B; key_mod = 0;  			  break; // 'X'
          case 0x0adc:  SDL_Scancode = 0x06; key_mod = 0;  			  break; // 'C'
          case 0x023c:  SDL_Scancode = 0x19; key_mod = 0;  			  break; // 'V'
          case 0x0abc:  SDL_Scancode = 0x05; key_mod = 0;  			  break; // 'B'
          case 0x0a7c:  SDL_Scancode = 0x11; key_mod = 0;  			  break; // 'N'
          case 0x0430:  SDL_Scancode = 0x10; key_mod = 0;  			  break; // 'M'
          case 0x025a:  SDL_Scancode = 0x33; key_mod = 0; 			  break; // ';'
          case 0x0cb0:  SDL_Scancode = 0x33; key_mod = KEY_MOD_LSHIFT;            break; // ':'
          case 0x0c1a:  SDL_Scancode = 0x34; key_mod = 0; 			  break; // '''
          case 0x0c70:  SDL_Scancode = 0x2D; key_mod = KEY_MOD_LSHIFT;            break; // '_'
          case 0x0c46:  SDL_Scancode = 0x37; key_mod = KEY_MOD_LSHIFT;            break; // '>'
          case 0x0428: 	SDL_Scancode = 0x3E; key_mod = KEY_MOD_LSHIFT;            break; // Shift F5
          case 0x02f0: 	SDL_Scancode = 0x28; key_mod = KEY_MOD_LSHIFT;            break; // Shift ENTER
        }
    }
    else if (state & (STATE_LEFT_ALT | STATE_RIGHT_ALT))
    {
        switch (cmd)
        {
            case 0x0880: SDL_Scancode = 0x2F; key_mod = KEY_MOD_LSHIFT;  break; // '['
            case 0x0840: SDL_Scancode = 0x30; key_mod = KEY_MOD_LSHIFT;  break; // ']'
			case 0x0414: SDL_Scancode = 0x31; key_mod = 0;  				break; // '\'	
            case 0x0c34: SDL_Scancode = 0x1F; key_mod = KEY_MOD_LSHIFT;  break; // '@'
            case 0x0c46: SDL_Scancode = 0x31; key_mod = KEY_MOD_LSHIFT;	break; // '|'
 
		}
    }
    else if (state & (STATE_LEFT_CTRL | STATE_RIGHT_CTRL))
    {
        switch (cmd)
        {
		  // Atari special graphic symbols...
          case 0x0c34:	SDL_Scancode = 0x14; key_mod = KEY_MOD_LCTRL;	break; // 'Q'
          case 0x02b4:	SDL_Scancode = 0x1A; key_mod = KEY_MOD_LCTRL; 	break; // 'W'
          case 0x0274:	SDL_Scancode = 0x08; key_mod = KEY_MOD_LCTRL;	break; // 'E'
          case 0x0af4:	SDL_Scancode = 0x15; key_mod = KEY_MOD_LCTRL;	break; // 'R'
          case 0x040c:	SDL_Scancode = 0x17; key_mod = KEY_MOD_LCTRL;	break; // 'T'
          case 0x0c8c:	SDL_Scancode = 0x1D; key_mod = KEY_MOD_LCTRL;  	break; // 'Z'
          case 0x0820:	SDL_Scancode = 0x18; key_mod = KEY_MOD_LCTRL;  	break; // 'U'
          case 0x04a0:	SDL_Scancode = 0x0C; key_mod = KEY_MOD_LCTRL;  	break; // 'I'
          case 0x0460:	SDL_Scancode = 0x12; key_mod = KEY_MOD_LCTRL;  	break; // 'O'
          case 0x0ce0:	SDL_Scancode = 0x13; key_mod = KEY_MOD_LCTRL;  	break; // 'P'
          case 0x02cc:	SDL_Scancode = 0x04; key_mod = KEY_MOD_LCTRL; 	break; // 'A'
          case 0x0c2c:	SDL_Scancode = 0x16; key_mod = KEY_MOD_LCTRL; 	break; // 'S'
          case 0x02ac:	SDL_Scancode = 0x07; key_mod = KEY_MOD_LCTRL; 	break; // 'D'
          case 0x026c:	SDL_Scancode = 0x09; key_mod = KEY_MOD_LCTRL; 	break; // 'F'
          case 0x0aec:	SDL_Scancode = 0x0A; key_mod = KEY_MOD_LCTRL; 	break; // 'G'
          case 0x0c1c:	SDL_Scancode = 0x0B; key_mod = KEY_MOD_LCTRL; 	break; // 'H'
          case 0x0810:	SDL_Scancode = 0x0D; key_mod = KEY_MOD_LCTRL; 	break; // 'J'
          case 0x0490:	SDL_Scancode = 0x0E; key_mod = KEY_MOD_LCTRL; 	break; // 'K'
          case 0x0450:	SDL_Scancode = 0x0F; key_mod = KEY_MOD_LCTRL; 	break; // 'L'
          case 0x029c:	SDL_Scancode = 0x1C; key_mod = KEY_MOD_LCTRL;  	break; // 'Y'
          case 0x025c:	SDL_Scancode = 0x1B; key_mod = KEY_MOD_LCTRL;  	break; // 'X'
          case 0x0adc:	SDL_Scancode = 0x06; key_mod = KEY_MOD_LCTRL;  	break; // 'C'
          case 0x023c:	SDL_Scancode = 0x19; key_mod = KEY_MOD_LCTRL;  	break; // 'V'
          case 0x0abc:	SDL_Scancode = 0x05; key_mod = KEY_MOD_LCTRL;  	break; // 'B'
          case 0x0a7c:	SDL_Scancode = 0x11; key_mod = KEY_MOD_LCTRL;  	break; // 'N'
          case 0x0430:	SDL_Scancode = 0x10; key_mod = KEY_MOD_LCTRL;  	break; // 'M'
        }
    }
    else
    {
        switch (cmd)
        {
			case 0x0444:	SDL_Scancode = 0x1E; key_mod = 0;	break; // '1'
			case 0x0cc4:	SDL_Scancode = 0x1F; key_mod = 0;	break; // '2'
			case 0x0424:	SDL_Scancode = 0x20; key_mod = 0;	break; // '3'
			case 0x0ca4:	SDL_Scancode = 0x21; key_mod = 0;	break; // '4'
			case 0x0c64:	SDL_Scancode = 0x22; key_mod = 0; 	break; // '5'
			case 0x02e4:	SDL_Scancode = 0x23; key_mod = 0;	break; // '6'
			case 0x0000:	SDL_Scancode = 0x24; key_mod = 0;	break; // '7'
			case 0x0880:	SDL_Scancode = 0x25; key_mod = 0;	break; // '8'
			case 0x0840:	SDL_Scancode = 0x26; key_mod = 0;	break; // '9'
                        case 0x04c0:	SDL_Scancode = 0x27; key_mod = 0;       break; // '0'

			case 0x0c34:	SDL_Scancode = 0x14; key_mod = 0;  	break; // 'q'
			case 0x02b4:	SDL_Scancode = 0x1A; key_mod = 0;  	break; // 'w'
			case 0x0274:	SDL_Scancode = 0x08; key_mod = 0;  	break; // 'e'
			case 0x0af4:	SDL_Scancode = 0x15; key_mod = 0;  	break; // 'r'
			case 0x040c:	SDL_Scancode = 0x17; key_mod = 0;  	break; // 't'
			case 0x0c8c:	SDL_Scancode = 0x1D; key_mod = 0;  	break; // 'z'
			case 0x0820:	SDL_Scancode = 0x18; key_mod = 0;  	break; // 'u'
			case 0x04a0:	SDL_Scancode = 0x0C; key_mod = 0;  	break; // 'i'
			case 0x0460:	SDL_Scancode = 0x12; key_mod = 0;  	break; // 'o'
			case 0x0ce0:	SDL_Scancode = 0x13; key_mod = 0;  	break; // 'p'
			case 0x0aea:	SDL_Scancode = 0x2E; key_mod = KEY_MOD_LSHIFT;   break; // '+'
			case 0x0c1a:	SDL_Scancode = 0x20; key_mod = KEY_MOD_LSHIFT;   break; // '#'

			case 0x02cc:	SDL_Scancode = 0x04; key_mod = 0; 	break; // 'a'
			case 0x0c2c:	SDL_Scancode = 0x16; key_mod = 0; 	break; // 's'
			case 0x02ac:	SDL_Scancode = 0x07; key_mod = 0; 	break; // 'd'
			case 0x026c:	SDL_Scancode = 0x09; key_mod = 0; 	break; // 'f'
			case 0x0aec:	SDL_Scancode = 0x0A; key_mod = 0; 	break; // 'g'
			case 0x0c1c:	SDL_Scancode = 0x0B; key_mod = 0; 	break; // 'h'
			case 0x0810:	SDL_Scancode = 0x0D; key_mod = 0; 	break; // 'j'
			case 0x0490:	SDL_Scancode = 0x0E; key_mod = 0; 	break; // 'k'
			case 0x0450:	SDL_Scancode = 0x0F; key_mod = 0; 	break; // 'l'
			case 0x029c:	SDL_Scancode = 0x1C; key_mod = 0;  	break; // 'y'
			case 0x025c:	SDL_Scancode = 0x1B; key_mod = 0;  	break; // 'x'
			case 0x0adc:	SDL_Scancode = 0x06; key_mod = 0;  	break; // 'c'
			case 0x023c:	SDL_Scancode = 0x19; key_mod = 0;  	break; // 'v'
			case 0x0abc:	SDL_Scancode = 0x05; key_mod = 0;  	break; // 'b'
			case 0x0a7c:	SDL_Scancode = 0x11; key_mod = 0;  	break; // 'n'
			case 0x0430:	SDL_Scancode = 0x10; key_mod = 0;  	break; // 'm'

			case 0x025a:	SDL_Scancode = 0x36; key_mod = 0; 	break; // ','
			case 0x0cb0:	SDL_Scancode = 0x37; key_mod = 0; 	break; // '.'
			case 0x0c70:	SDL_Scancode = 0x2D; key_mod = 0; 	break; // '-'
			case 0x0c46:	SDL_Scancode = 0x36; key_mod = KEY_MOD_LSHIFT; 	break; // '<'       

			case 0x0c38: 	SDL_Scancode = 0x29; key_mod = 0;	break; // ESCAPE
			case 0x0c54: 	SDL_Scancode = 0x2A; key_mod = 0;	break; // BACKSPACE
			case 0x0af2: 	SDL_Scancode = 0x50; key_mod = KEY_MOD_LCTRL;		break; // LEFT
			case 0x0482: 	SDL_Scancode = 0x52; key_mod = KEY_MOD_LCTRL;		break; // UP
			case 0x0272: 	SDL_Scancode = 0x51; key_mod = KEY_MOD_LCTRL;		break; // DOWN
			case 0x0442: 	SDL_Scancode = 0x4F; key_mod = KEY_MOD_LCTRL;		break; // RIGHT
			case 0x0804: 	SDL_Scancode = 0x40; key_mod = 0;  	break; // STOP
                        case 0x02d4: 	SDL_Scancode = 0x2B; key_mod = 0;       break; // TABULATOR
                        case 0x02f0: 	SDL_Scancode = 0x28; key_mod = 0;       break; // ENTER
			case 0x0808: 	SDL_Scancode = 0x3A; key_mod = 0;	break; // F1
			case 0x0488: 	SDL_Scancode = 0x3B; key_mod = 0;	break; // F2
			case 0x0448: 	SDL_Scancode = 0x3C; key_mod = 0;	break; // F3
			case 0x0cc8: 	SDL_Scancode = 0x3D; key_mod = 0;	break; // F4
			case 0x0428: 	SDL_Scancode = 0x3E; key_mod = 0;	break; // F5
			case 0x0ca8: 	SDL_Scancode = 0x3F; key_mod = 0;	break; // F6
			case 0x0c68: 	SDL_Scancode = 0x40; key_mod = 0;	break; // F7
			case 0x02e8: 	SDL_Scancode = 0x41; key_mod = 0;	break; // F8
			case 0x0418: 	SDL_Scancode = 0x42; key_mod = 0;	break; // F9
			case 0x0c98: 	SDL_Scancode = 0x43; key_mod = 0;	break; // F10
			case 0x0c58: 	SDL_Scancode = 0x44; key_mod = 0;	break; // F11
			case 0x02d8: 	SDL_Scancode = 0x45; key_mod = 0;	break; // F12
                        case 0x02aa: 	SDL_Scancode = 0x2C; key_mod = 0;       break; // SPACE
			case 0x0484: 	SDL_Scancode = 0x23; key_mod = KEY_MOD_LSHIFT;    break; // '^'
                        case 0x0ca2: 	SDL_Scancode = 0x4B; key_mod = 0;       break; // Page up
                        case 0x0c4a: 	SDL_Scancode = 0x4E; key_mod = 0;       break; // Page down
			//        case 0x0c32: SDL_Scancode = KEY_INSERT;              break; // INSERT
			//        case 0x040a: SDL_Scancode = KEY_DELETE;              break; // DELETE

        }
    }
	
	SDL_Code.SDL_Scancode = SDL_Scancode;
	SDL_Code.key_mod = key_mod;
}

#endif // K8561_KEYBOARD

#ifdef SERIAL_KEYBOARD

#define KEY_ESCAPE          0x1B 
#define KEY_BACK            0x7F
#define KEY_MENUE           0x80
#define KEY_FORWARD         0x82
#define KEY_ADDRESS         0x83
#define KEY_WINDOW          0x84
#define KEY_1ST_PAGE        0x85
#define KEY_STOP            0x86
#define KEY_MAIL            0x87
#define KEY_FAVORITES       0x88
#define KEY_NEW_PAGE        0x89
#define KEY_SETUP           0x8A
#define KEY_FONT            0x8B
#define KEY_PRINT           0x8C
#define KEY_ON_OFF          0x8E

#define KEY_INSERT          0x90
#define KEY_DELETE          0x91
#define KEY_LEFT            0x92
#define KEY_HOME            0x93
#define KEY_END             0x94
#define KEY_UP              0x95
#define KEY_DOWN            0x96
#define KEY_PAGE_UP         0x97
#define KEY_PAGE_DOWN       0x98
#define KEY_RIGHT           0x99
#define KEY_TAB             0x09
#define KEY_ENTER			0x0A

#define KEY_F1	0x9A
#define KEY_F2	0x9B
#define KEY_F3  0x9C
#define KEY_F4  0x9D
#define KEY_F5  0x9E
#define KEY_F6  0x9F
#define KEY_F7  0xA0
#define KEY_F8  0xA1
#define KEY_F9  0xA2
#define KEY_F10 0xA3
#define KEY_F11 0xA4
#define KEY_F12 0xA5

const uint8_t _ascii2scancode[][3] =
{
	{'A', 0x04, 0},
	{'B', 0x05, 0},
	{'C', 0x06, 0},
	{'D', 0x07, 0},
	{'E', 0x08, 0},
	{'F', 0x09, 0},
	{'G', 0x0A, 0},
	{'H', 0x0B, 0},
	{'I', 0x0C, 0},
	{'J', 0x0D, 0},
	{'K', 0x0E, 0},
	{'L', 0x0F, 0},
	{'M', 0x10, 0},
	{'N', 0x11, 0},
	{'O', 0x12, 0},
	{'P', 0x13, 0},
	{'Q', 0x14, 0},
	{'R', 0x15, 0},
	{'S', 0x16, 0},
	{'T', 0x17, 0},
	{'U', 0x18, 0},
	{'V', 0x19, 0},
	{'W', 0x1A, 0},
	{'X', 0x1B, 0},
	{'Y', 0x1C, 0},
	{'Z', 0x1D, 0},
	{'a', 0x04, 0},
	{'b', 0x05, 0},
	{'c', 0x06, 0},
	{'d', 0x07, 0},
	{'e', 0x08, 0},
	{'f', 0x09, 0},
	{'g', 0x0A, 0},
	{'h', 0x0B, 0},
	{'i', 0x0C, 0},
	{'j', 0x0D, 0},
	{'k', 0x0E, 0},
	{'l', 0x0F, 0},
	{'m', 0x10, 0},
	{'n', 0x11, 0},
	{'o', 0x12, 0},
	{'p', 0x13, 0},
	{'q', 0x14, 0},
	{'r', 0x15, 0},
	{'s', 0x16, 0},
	{'t', 0x17, 0},
	{'u', 0x18, 0},
	{'v', 0x19, 0},
	{'w', 0x1A, 0},
	{'x', 0x1B, 0},
	{'y', 0x1C, 0},
	{'z', 0x1D, 0},	
	{'1', 0x1E, 0},
	{'!', 0x1E, KEY_MOD_LSHIFT},
	{'2', 0x1F, 0},
	{'@', 0x1F, KEY_MOD_LSHIFT},
	{'3', 0x20, 0},
	{'#', 0x20, KEY_MOD_LSHIFT},
	{'4', 0x21, 0},
	{'$', 0x21, KEY_MOD_LSHIFT},
	{'5', 0x22, 0},
	{'%', 0x22, KEY_MOD_LSHIFT},
	{'6', 0x23, 0},
	{'&', 0x23, KEY_MOD_LSHIFT},
	{'7', 0x24, 0},
	{'8', 0x25, 0},
	{'*', 0x25, KEY_MOD_LSHIFT},
	{'9', 0x26, 0},
	{'(', 0x26, KEY_MOD_LSHIFT},
	{'0', 0x27, 0},
	{')', 0x27, KEY_MOD_LSHIFT},
	{KEY_ENTER,   0x28, 0},
	{KEY_ESCAPE,  0x29, 0},
	{KEY_BACK, 	  0x2A, 0},
	{KEY_TAB,     0x2B, 0},
	{' ', 0x2C, 0},
	{'-', 0x2D, 0},
	{'_', 0x2D, KEY_MOD_LSHIFT},
	{'=', 0x2E, 0},
	{'+', 0x2E, KEY_MOD_LSHIFT},	
	{'[', 0x2F, KEY_MOD_LSHIFT},
	{']', 0x30, KEY_MOD_LSHIFT},
	{'|', 0x31, KEY_MOD_LSHIFT},
	{';', 0x33, 0},
	{':', 0x33, KEY_MOD_LSHIFT},
	{'\"',0x34, KEY_MOD_LSHIFT},
	{',', 0x36, 0},	
	{'<', 0x36, KEY_MOD_LSHIFT},
	{'.', 0x37, 0},
	{'>', 0x37, KEY_MOD_LSHIFT},
	{'/', 0x38, 0},
	{'?', 0x38, KEY_MOD_LSHIFT},
	{KEY_F1, 0x3A, 0},
	{KEY_F2, 0x3B, 0},
	{KEY_F3, 0x3C, 0},
	{KEY_F4, 0x3D, 0},
	{KEY_F5, 0x3E, 0},
	{KEY_F6, 0x3F, 0},
	{KEY_F7, 0x40, 0},
	{KEY_F8, 0x41, 0},
	{KEY_F9, 0x42, 0},
	{KEY_F10,0x43, 0},
	{KEY_F11,0x44, 0},
	{KEY_F12,0x45, 0},	
	{KEY_RIGHT, 0x4F, KEY_MOD_LCTRL},
	{KEY_LEFT, 0x50, KEY_MOD_LCTRL},
	{KEY_DOWN, 0x51, KEY_MOD_LCTRL},
	{KEY_UP, 0x52, KEY_MOD_LCTRL},
};


int get_hid_serial(uint8_t* dst)
{
	bool dirty = false;
	static int wt_expire = 0;
	int wt_modifiers = 0;
	int incomingByte = 0;
	incomingByte = getchar();
	
	if (incomingByte != -1)
	{
		int scancode = -1;
		for (int i=0; i < sizeof(_ascii2scancode); i++)
		{
			if (_ascii2scancode[i][0]== incomingByte)
			{
				scancode = _ascii2scancode[i][1];
				wt_modifiers = _ascii2scancode[i][2];
				break;
			}			

		}
		// scancode = incomingByte;
		// wt_modifiers = KEY_MOD_LCTRL;
		if (scancode != -1)
		{
			printf("Key=%d->0x%02x\n", incomingByte, scancode);
			
			// generate hid keyboard events if anything was pressed or changed...
			// A1 01 mods XX k k k k k k
			dst[0] = 0xA1;
			dst[1] = 0x01;
			dst[2] = wt_modifiers;
			dst[3] = 0;
			dst[4] = (char)scancode;  // Scancode
			wt_expire = 10;
			dirty = true;
		}
	}
	else
	{
		if (wt_expire-- == 0)
		{
			dst[0] = 0xA1;
			dst[1] = 0x01;
			dst[2] = 0;
			dst[3] = 0;
			dst[4] = 0;  // Scancode
			dirty = true;
		}			
	}
	
    return dirty ? 10 : 0;
}
#endif // SERIAL_KEYBOARD

#endif // __ir_input__
