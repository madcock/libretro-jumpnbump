#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "libretro.h"

#include "globals.h"
#include "micromod.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

uint16_t *frame_buf;

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

extern int flies_enabled;
extern int flip;

static const struct retro_variable var_jnb_flip    = { "jnb-flip", "Flip Level; OFF|ON" };
static const struct retro_variable var_jnb_flies   = { "jnb-flies", "Flies; OFF|ON" };

static const struct retro_variable var_empty = { NULL, NULL };


#define GAME_STATE_BOOT 0
#define GAME_STATE_MENU 1
#define GAME_STATE_GAME 2
#define GAME_STATE_END  3

static int game_state;
static bool supports_input_bitmasks;

void retro_init(void)
{
   struct retro_variable vars[3];
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Jump" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Jump" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },

      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Jump" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },

      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Jump" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },

      { 0 },
   };

   frame_buf = calloc(JNB_WIDTH * JNB_HEIGHT, sizeof(uint16_t));
   supports_input_bitmasks = environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL);
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   vars[0] = var_jnb_flip;
   vars[1] = var_jnb_flies;
   vars[2] = var_empty;
   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)vars);
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device) { }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "Jump 'n Bump";
   info->library_version  = "0.1";
   info->need_fullpath    = false;
   info->valid_extensions = "dat";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width   = JNB_WIDTH;
   info->geometry.base_height  = JNB_HEIGHT;
   info->geometry.max_width    = JNB_WIDTH;
   info->geometry.max_height   = JNB_HEIGHT;
   info->geometry.aspect_ratio = (400.0f / 256.0f);
   info->timing.fps            = 60.0;
#if !defined(SF2000)
   info->timing.sample_rate    = 44100.0f;
#else
   info->timing.sample_rate    = 11025;
#endif
}

void retro_set_environment(retro_environment_t cb)
{     
   environ_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) { }

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_reset(void)
{
   game_state = GAME_STATE_BOOT;
}

static void update_controller(int port)
{
   int left, right, jump;

   if (supports_input_bitmasks)
   {
      const int16_t active = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
      left = (active & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT));
      right = (active & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT));
      jump = (active & (1 << RETRO_DEVICE_ID_JOYPAD_A));
   }
   else
   {
      left = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
      right = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
      jump = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
   }

   addkey(((KEY_PL1_LEFT + 0x10 * port) & 0x7fff) | ((left != 0) ? 0x0 : 0x8000));
   addkey(((KEY_PL1_RIGHT + 0x10 * port) & 0x7fff) | ((right != 0) ? 0x0 : 0x8000));
   addkey(((KEY_PL1_JUMP + 0x10 * port) & 0x7fff) | ((jump != 0) ? 0x0 : 0x8000));
}

static void update_input(void)
{
   int esc_pressed, port;
   input_poll_cb();

   for (port = 0; port < 4; port++)
   {
      update_controller(port);
   }

   esc_pressed = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
   addkey((1 & 0x7fff) | ((esc_pressed != 0) ? 0x0 : 0x8000));
}

static void check_variables(void)
{
   struct retro_variable var = { 0 };

   var.key = var_jnb_flip.key;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      if (strcmp(var.value, "OFF") == 0)
      {
         flip = 0;
      }
      else
      {
         flip = 1;
      }
   }

   var.key = var_jnb_flies.key;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
   {
      if (strcmp(var.value, "OFF") == 0)
      {
         flies_enabled = 0;
      }
      else
      {
         flies_enabled = 1;
      }
   }
}

static int16_t audio_buffer[735 * 2];

static void audio_callback(void)
{
   memset((void*)audio_buffer, 0, 735 * 4);
   micromod_get_audio(audio_buffer, 735);
   mix_sound(audio_buffer, 735 * 4);
   audio_batch_cb(audio_buffer, 735);
}

void retro_run(void)
{
   bool updated;

   update_input();
   audio_callback();

   if (game_state == GAME_STATE_BOOT)
   {
      menu_init();
      game_state = GAME_STATE_MENU;
   }
   else if (game_state == GAME_STATE_MENU)
   {
      if(menu_frame() == 1)
      {
         init_level();
         game_init();
         game_state = GAME_STATE_GAME;
      }
   }
   else if (game_state == GAME_STATE_GAME)
   {
      if(game_loop() == 1)
      {
         end_game();
         game_state = GAME_STATE_END;
      }
   }
   else if (game_state == GAME_STATE_END)
   {
      if(end_loop() == 1)
      {
         game_state = GAME_STATE_BOOT;
      }
   }

   video_cb(frame_buf, JNB_WIDTH, JNB_HEIGHT, JNB_WIDTH << 1);

   updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
   {
      check_variables();
      if (game_state == GAME_STATE_GAME) {
         // need to restart
         game_state = GAME_STATE_BOOT;
      }
   }
}

bool retro_load_game(const struct retro_game_info *info)
{   

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      return false;
   }

   check_variables();

   datafile_buffer = (unsigned char *) malloc(info->size);
   memcpy(datafile_buffer, info->data, info->size);

   init_program();
   game_state = GAME_STATE_BOOT;

   return true;
}

void retro_unload_game(void)
{
   deinit_program();
   free(datafile_buffer);
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_PAL;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void) { }

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}