/* JOYSTICK.C - joystick support for WinX68k */

#include "common.h"
#include "prop.h"
#include "joystick.h"
#include "winui.h"
#include "keyboard.h"

#include "libretro.h"
extern retro_environment_t environ_cb;
extern retro_input_state_t input_state_cb;
extern uint32_t libretro_supports_input_bitmasks;

extern unsigned turbo_delay;
extern bool turbo_toggle;

uint8_t joy[2];
uint8_t JoyKeyState;
static uint8_t JoyKeyState0;
static uint8_t JoyKeyState1;

static uint8_t JoyState0[2];
static uint8_t JoyState1[2];
static uint8_t JoyPortData[2];

/* This stores whether the buttons were down. This avoids key repeats. */
static uint8_t JoyDownState0;

void Joystick_Init(void)
{
	joy[0]         = 1; /* activate JOY1 */
	joy[1]         = 1; /* activate JOY2 */
	JoyKeyState    = 0;
	JoyKeyState0   = 0;
	JoyKeyState1   = 0;
	JoyState0[0]   = 0xff;
	JoyState0[1]   = 0xff;
	JoyState1[0]   = 0xff;
	JoyState1[1]   = 0xff;
	JoyPortData[0] = 0;
	JoyPortData[1] = 0;
}

void Joystick_Cleanup(void)
{
}

uint8_t FASTCALL Joystick_Read(uint8_t num)
{
	uint8_t ret0 = 0xff, ret1 = 0xff;

	if (joy[num])
	{
		ret0 = JoyState0[num];
		ret1 = JoyState1[num];
	}

	return ((~JoyPortData[num]) & ret0) | (JoyPortData[num] & ret1);
}


void FASTCALL Joystick_Write(uint8_t num, uint8_t data)
{
	if ((num == 0)||(num == 1))
      JoyPortData[num] = data;
}

/* Menu navigation related vars */
#define RATE   3      /* repeat rate */
#define JOYDELAY 30   /* delay before 1st repeat */

static uint32_t get_px68k_input_bitmasks(int port)
{
   return input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
}

static uint16_t get_px68k_input(int port)
{
   uint32_t i   = 0;
   uint32_t res = 0;
   for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R2 + 1); i++)
      res |= input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
   return res;
}

typedef struct
{
   uint8_t retropad;
   uint8_t joypad;
   uint8_t swap;
} RetroToJoypad;

static const RetroToJoypad retro_to_joy_2btn[] = {
   { RETRO_DEVICE_ID_JOYPAD_A, JOY_TRG2, JOY_TRG1 },
   { RETRO_DEVICE_ID_JOYPAD_B, JOY_TRG1, JOY_TRG2 },
   { RETRO_DEVICE_ID_JOYPAD_X, JOY_TRG1, JOY_TRG2 },
   { RETRO_DEVICE_ID_JOYPAD_Y, JOY_TRG2, JOY_TRG1 },
};

static const RetroToJoypad retro_to_joy_md[] = {
   { RETRO_DEVICE_ID_JOYPAD_A,      JOY_TRG1 }, /* MD B - Medium Kick  */
   { RETRO_DEVICE_ID_JOYPAD_B,      JOY_TRG2 }, /* MD A - Light Kick   */
   { RETRO_DEVICE_ID_JOYPAD_X,      JOY_TRG4 }, /* MD Y - Medium Punch */
   { RETRO_DEVICE_ID_JOYPAD_Y,      JOY_TRG3 }, /* MD X - Light Punch  */
   { RETRO_DEVICE_ID_JOYPAD_L,      JOY_TRG5 }, /* MD Z - High Punch   */
   { RETRO_DEVICE_ID_JOYPAD_R,      JOY_TRG8 }, /* MD C - High Kick    */
   { RETRO_DEVICE_ID_JOYPAD_START,  JOY_TRG6 }, /* MD Start            */
   { RETRO_DEVICE_ID_JOYPAD_SELECT, JOY_TRG7 }, /* MD Mode             */
};

static const RetroToJoypad retro_to_joy_sfc[] = {
   { RETRO_DEVICE_ID_JOYPAD_A,      JOY_TRG2 }, /* High Kick    */
   { RETRO_DEVICE_ID_JOYPAD_B,      JOY_TRG1 }, /* Medium Kick  */
   { RETRO_DEVICE_ID_JOYPAD_X,      JOY_TRG3 }, /* Medium Punch */
   { RETRO_DEVICE_ID_JOYPAD_Y,      JOY_TRG4 }, /* Light Kick   */
   { RETRO_DEVICE_ID_JOYPAD_L,      JOY_TRG8 }, /* Light Punch  */
   { RETRO_DEVICE_ID_JOYPAD_R,      JOY_TRG5 }, /* High Punch   */
   { RETRO_DEVICE_ID_JOYPAD_START,  JOY_TRG6 },
   { RETRO_DEVICE_ID_JOYPAD_SELECT, JOY_TRG7 },
};

static void display_msg(const char *str, unsigned frames)
{
   struct retro_message msg; 
   msg.msg    = str;
   msg.frames = frames;
   environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
}

#define JOY_2BTN 4 /* A and B are duplicated to X and Y, so 4 */
#define JOY_8BTN 8

void FASTCALL Joystick_Update(int is_menu, int key, int port)
{
   uint8_t ret0            = 0xff, ret1 = 0xff;
   uint8_t temp            = 0;
   static uint8_t pre_ret0 = 0xff;
   uint32_t res            = 0;

   static uint8_t turbo_counter[2][8];
   static bool turbo_modifier[2];
   static bool turbo_hold[2];
   unsigned i;

   if (libretro_supports_input_bitmasks)
      res                  = get_px68k_input_bitmasks(port);
   else
      res                  = get_px68k_input(port);

   /* D-Pad */
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      temp |= JOY_RIGHT;
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
      temp |=  JOY_LEFT;
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
      temp |=  JOY_UP;
   if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
      temp |=  JOY_DOWN;

   if ((temp & (JOY_LEFT | JOY_RIGHT)) == (JOY_LEFT | JOY_RIGHT))
      temp &= ~(JOY_LEFT | JOY_RIGHT);
   if ((temp & (JOY_UP | JOY_DOWN)) == (JOY_UP | JOY_DOWN))
      temp &= ~(JOY_UP | JOY_DOWN);

   ret0 ^= temp;

   if (turbo_toggle)
   {
      bool turbo_hold_prev = turbo_hold[port];
      turbo_hold[port] = !!(res & (1 << RETRO_DEVICE_ID_JOYPAD_R2));

      if (turbo_hold[port] && !turbo_hold_prev)
      {
         char turbo_msg[64] = {0};
         turbo_modifier[port] = !turbo_modifier[port];

         snprintf(turbo_msg, sizeof(turbo_msg), "Port %d turbo buttons: %s", port + 1, turbo_modifier[port] ? "ENABLED" : "DISABLED");
         display_msg(turbo_msg, 180);
      }
   }
   else
      turbo_modifier[port] = !!(res & (1 << RETRO_DEVICE_ID_JOYPAD_R2));

   /* Buttons */
   switch (Config.JOY_TYPE[port])
   {
      case PAD_2BUTTON:
         for (i = 0; i < JOY_2BTN; i++)
         {
            if (res & (1 << retro_to_joy_2btn[i].retropad))
            {
               if (!turbo_modifier[port] || !turbo_counter[port][i])
                  ret0 ^= Config.VbtnSwap ? retro_to_joy_2btn[i].swap : retro_to_joy_2btn[i].joypad;
               turbo_counter[port][i]++;
               turbo_counter[port][i] %= turbo_delay + 1;
            }
            else
               turbo_counter[port][i] = 0;
         }

         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            ret0 &= ~(JOY_UP | JOY_DOWN);
         if (!Config.joy1_select_mapping)
            if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
               ret0 &= ~(JOY_LEFT | JOY_RIGHT);
         break;

      case PAD_CPSF_MD:
         for (i = 0; i < JOY_8BTN; i++)
         {
            if (res & (1 << retro_to_joy_md[i].retropad))
            {
               if (!turbo_modifier[port] || !turbo_counter[port][i])
               {
                  if (i < 2) /* A and B */
                     ret0 ^= retro_to_joy_md[i].joypad;
                  else       /* X, Y, L, R, Start and Select */
                     ret1 ^= retro_to_joy_md[i].joypad;
               }
               turbo_counter[port][i]++;
               turbo_counter[port][i] %= turbo_delay + 1;
            }
            else
               turbo_counter[port][i] = 0;
         }
         break;

      case PAD_CPSF_SFC:
         for (i = 0; i < JOY_8BTN; i++)
         {
            if (res & (1 << retro_to_joy_sfc[i].retropad))
            {
               if (!turbo_modifier[port] || !turbo_counter[port][i])
               {
                  if (i < 2) /* A and B */
                     ret0 ^= retro_to_joy_sfc[i].joypad;
                  else       /* X, Y, L, R, Start and Select */
                     ret1 ^= retro_to_joy_sfc[i].joypad;
               }
               turbo_counter[port][i]++;
               turbo_counter[port][i] %= turbo_delay + 1;
            }
            else
               turbo_counter[port][i] = 0;
         }
         break;
   }

   JoyDownState0   = ~(ret0 ^ pre_ret0) | ret0;
   pre_ret0        = ret0;

   /* input overrides section during Menu mode for faster menu browsing
    * by pressing and holding key or button aka turbo mode */
   if (is_menu)
   {
      static int repeat_rate, repeat_delay;
      static uint8_t last_inbuf;
      uint8_t joy_in = (ret0 ^ 0xff);
      uint8_t inbuf  = (joy_in | key);

      if ((inbuf & (JOY_LEFT | JOY_RIGHT)) == (JOY_LEFT | JOY_RIGHT))
         inbuf &= ~(JOY_LEFT | JOY_RIGHT);
      if ((inbuf & (JOY_UP | JOY_DOWN)) == (JOY_UP | JOY_DOWN))
         inbuf &= ~(JOY_UP | JOY_DOWN);

      for (i = 0; i < 4; i++)
         speedup_joy[1 << i] = 0;

      if (last_inbuf != inbuf)
      {
         last_inbuf    = inbuf;
         repeat_delay  = JOYDELAY;
         repeat_rate   = 0;
         JoyDownState0 = (inbuf ^ 0xff);
      }
      else
      {
         if (repeat_delay)
            repeat_delay--;
         if (repeat_delay == 0)
         {
            if (repeat_rate)
               repeat_rate--;
            if (repeat_rate == 0)
            {
               repeat_rate = RATE;
               for (i = 0; i < 4; i++)
               {
                  uint8_t tmp = (1 << i); /* which direction? UP/DOWN/LEFT/RIGHT */
                  if ((inbuf & tmp) == tmp)
                     speedup_joy[tmp] = 1;
               }
            }
         }
      }
   }
   else
   {
      /* disable Joystick when software keyboard is active */
      if (!Keyboard_IsSwKeyboard())
      {
         JoyState0[port] = ret0;
         JoyState1[port] = ret1;
      }
   }
}

uint8_t get_joy_downstate(void)   { return JoyDownState0;  }
void    reset_joy_downstate(void) { JoyDownState0 = 0xff;  }
