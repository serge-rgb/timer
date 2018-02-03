#if defined(BUILD_SCRIPT)

if [ ! -d build ]; then
   mkdir build
fi

glfw_lib=glfw/build/src/libglfw3.a


if [ ! -f $glfw_lib ]; then
   ./build.sh
fi

cflags="-ObjC -g -Iglfw/include -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo"

clang $cflags timer.c $glfw_lib -o build/timer

exit

#endif



// C std lib.
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// POSIX
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>


// macOS
// #include <Cocoa/Cocoa.h>
#include <AppKit/AppKit.h>

// Local
#include <GLFW/glfw3.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef int32_t   i32;
typedef uint64_t  u64;
typedef int64_t   i64;
typedef size_t    sz;
#ifndef bool
typedef u32       bool;
#endif

#define Zero {0}
#define true 1
#define false 0
#define MsgLen 10000


#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_GLFW_GL2_IMPLEMENTATION
#define NK_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_glfw_gl2.h"


#define TIMER_EPOCH 1517696211

#pragma pack(push, 1)
typedef struct TimeBlock_s {
   u32 begin;  // timestamp relative to TIMER_EPOCH
   u16 duration;
} TimeBlock;

// 100 years of 32 daily blocks of time.
#define BLOCK_COUNT (100 * 365 * 32)

typedef struct State_s {
   TimeBlock blocks[BLOCK_COUNT];
   i64       n_blocks;
} State;
#pragma pack(pop)


void
handle_errno_err(int err) {
  switch(err) {
#define ErrCase(err) case(err): { fprintf(stderr, "POSIX error: " #err "\n"); } break
    ErrCase(E2BIG);
    ErrCase(EACCES);
    ErrCase(EBADF);
    ErrCase(EBUSY);
    ErrCase(ECHILD);
    ErrCase(EDEADLK);
    ErrCase(EEXIST);
    ErrCase(EFAULT);
    ErrCase(EFBIG);
    ErrCase(EINTR);
    ErrCase(EINVAL);
    ErrCase(EIO);
    ErrCase(EISDIR);
    ErrCase(EMFILE);
    ErrCase(EMLINK);
    ErrCase(ENFILE);
    ErrCase(ENODEV);
    ErrCase(ENOENT);
    ErrCase(ENOEXEC);
    ErrCase(ENOMEM);
    ErrCase(ENOSPC);
    ErrCase(ENOTBLK);
    ErrCase(ENOTDIR);
    ErrCase(ENOTTY);
    ErrCase(ENXIO);
    ErrCase(EOVERFLOW);
    ErrCase(EPERM);
    ErrCase(EPIPE);
    ErrCase(EROFS);
    ErrCase(ESPIPE);
    ErrCase(ESRCH);
    ErrCase(ETXTBSY);
    ErrCase(EXDEV);
    default: {
      fprintf(stderr, "unknown mmap error\n");
    }
#undef ErrCase
  }
}

void
handle_errno() {
  int err = errno;
  handle_errno_err(err);
}

// TODO: Write a stb-style library that handles OS paths seamlessly
// TODO: Memory mapping on Windows.
State*
open_database(char* fname) {
   State* s = NULL;

   bool clear_mem = false;
   off_t file_len = sizeof(State);

   int fd = open(fname, O_RDWR, S_IRWXU);
   if (fd == -1) {
     int err = errno;
     if (err != ENOENT) {
       handle_errno_err(err);
     }
     else {
       clear_mem = true;
       fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
       if (fd == -1) {
         handle_errno();
       }
       else if (ftruncate (fd, file_len) == -1) {
         handle_errno();
         fd = -1;
       }
     }
   }

   if (fd != -1) {
     void* p = mmap(NULL, file_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
     if (s == MAP_FAILED) {
       handle_errno();
     }
     else {
       s = p;
       if (clear_mem) {
         memset(s, 0, file_len);
       }
     }
     close(fd);
   }
   return s;
}

void
error(int error, const char* descr) {
   fprintf(stderr, "Got GLFW error %d: %s\n", error, descr);
}

void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
       glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}


void
time_pretty(char* out, sz len, u64 unix) {
  u32 sec = unix % 60;
  u32 min = (unix / 60) % 60;
  u32 hr = (unix / (60*60));
  snprintf(out, len, "%02d:%02d:%02d", hr, min, sec);
}

NSAlert*
mac_alert(const char* info, const char* title) {
  NSAlert *alert = [[NSAlert alloc] init];
  alert.messageText = [NSString stringWithUTF8String:title ? title : ""];
  alert.informativeText = [NSString stringWithUTF8String:info ? info : ""];
  return alert;
}

void
show_message_box(char* info, char* title)
{
  @autoreleasepool {
    [mac_alert(info, title) runModal];
  }
}


int
main() {
  State* state = open_database("db");
  if (!glfwInit()) {
    fprintf(stderr, "Could not initialize GLFW.\n");
  }
  else if (!state) {
    fprintf(stderr, "Could not open database.\n");
  }
  else {
    int width = 400, height=300;
    glfwSetErrorCallback(error);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    GLFWwindow* window = glfwCreateWindow(width, height, "Timer", NULL, NULL);
    if (!window) {
      fprintf(stderr, "Could not create window.\n");
    }
    else {
      // TODO: Need to load GL functions?
      struct nk_colorf bg;

      glfwSetKeyCallback(window, key_callback);
      glfwMakeContextCurrent(window);
      glfwSwapInterval(1);

      struct nk_context* nk = nk_glfw3_init(window, NK_GLFW3_INSTALL_CALLBACKS);
      {struct nk_font_atlas *atlas;
        nk_glfw3_font_stash_begin(&atlas);
        /*struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0);*/
        /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
        /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
        /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
        /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
        /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
        nk_glfw3_font_stash_end();
        /*nk_style_load_all_cursors(nk, atlas->cursors);*/
        /*nk_style_set_font(nk, &droid->handle);*/}

#ifdef INCLUDE_STYLE
      /* set_style(nk, THEME_WHITE); */
      set_style(nk, THEME_RED);
      /*set_style(nk, THEME_BLUE);*/
      /*set_style(nk, THEME_DARK);*/
#endif
      bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;

      enum BlockFsm {
        WAITING,
        INSIDE_BLOCK,
        ERROR,
      };
      int blockfsm = WAITING;
      time_t block_begin = Zero;

      int bw1 = 125;
      int* bw = &bw1;
      while (!glfwWindowShouldClose(window)) {
        u64 now = (u64)time(0);
        /* Input */
        glfwPollEvents();
        nk_glfw3_new_frame();

        /* GUI */
        if (nk_begin(nk, "Timer", nk_rect(0, 0, width, height), 0)) {
          static char* error_message = "undefined";
          nk_layout_row_dynamic(nk, 30, 3);
          nk_label(nk, "Timer", NK_TEXT_LEFT);
          if (nk_button_label(nk, "more")) {
            *bw += 5;
            printf("bw is %d\n", *bw);
          }
          if (nk_button_label(nk, "less")) {
            *bw -= 5;
            printf("bw is %d\n", *bw);
          }

          if (blockfsm == WAITING) {
            nk_layout_row_static(nk, 30, bw1, 1);
            if (nk_button_label(nk, "Begin Time Block.")) {
              blockfsm = INSIDE_BLOCK;
              block_begin = time(0);
            }
            u64 timesum = 0; {
              u64 distance = 0;
              for (i64 n = state->n_blocks-1;
                   n >= 0 && distance < 24*60*60;
                   --n) {
                TimeBlock* b = &state->blocks[n];
                distance = now - (b->begin + TIMER_EPOCH);
                timesum += b->duration;
              }
            }
            char logmsg[MsgLen] = Zero;
            char timestr[MsgLen] = Zero;
            time_pretty(timestr, MsgLen, timesum);
            snprintf(logmsg, MsgLen, "Time elapsed last 24 hours: %s", timestr);
            nk_layout_row_dynamic(nk, 30, 1);
            nk_label(nk, logmsg, NK_TEXT_CENTERED);
          }
          else if (blockfsm == INSIDE_BLOCK) {
            bool end_block = false;
            if (now - block_begin > 10) {
              show_message_box("Time block done!", "Timer");
              end_block = true;
            }
            /*draw elapsed time*/ {
              nk_layout_row_dynamic(nk, 30, 1);
              time_t now = time(NULL);
              u32 relnow = now - block_begin;
              char msg[MsgLen] = Zero;
              time_pretty(msg,MsgLen, (u64)relnow);
              nk_label(nk, msg, NK_TEXT_CENTERED);
            }
            nk_layout_row_static(nk, 30, bw1, 2);
            if (nk_button_label(nk, "End Time Block.")) {
              end_block = true;
            }
            if (end_block) {
              if (now - block_begin >= 65536) {
                blockfsm = ERROR;
                error_message = "time block too long!";
              }
              else {
                TimeBlock* block = &state->blocks[state->n_blocks++];
                block->duration = now - block_begin;
                block->begin = block_begin - TIMER_EPOCH;
                blockfsm = WAITING;
              }
            }
          }
          else if (blockfsm == ERROR) {
            nk_layout_row_dynamic(nk, 30, 2);
            char errmsg[MsgLen] = Zero;
            snprintf(errmsg, MsgLen, "Error: %s", error_message);
            nk_label(nk, errmsg, NK_TEXT_CENTERED);
            if (nk_button_label(nk, "Ok")) {
              blockfsm = WAITING;
            }
          }

#if ORIG_NK_DEMO
          static int op = 1;
          nk_layout_row_dynamic(nk, 30, 3);
          if (nk_option_label(nk, "easy", op == 1)) op = 1;
          if (nk_option_label(nk, "hard", op == 2)) op = 2;
          if (nk_option_label(nk, "hardest", op == 3)) op = 3;

          nk_layout_row_dynamic(nk, 25, 1);
          nk_property_int(nk, "Compression:", 0, &property, 100, 10, 1);

          nk_layout_row_dynamic(nk, 20, 1);
          nk_label(nk, "background:", NK_TEXT_LEFT);
          nk_layout_row_dynamic(nk, 25, 1);
          if (nk_combo_begin_color(nk, nk_rgb_cf(bg), nk_vec2(nk_widget_width(nk),400))) {
            nk_layout_row_dynamic(nk, 120, 1);
            bg = nk_color_picker(nk, bg, NK_RGBA);
            nk_layout_row_dynamic(nk, 25, 1);
            bg.r = nk_propertyf(nk, "#R:", 0, bg.r, 1.0f, 0.01f,0.005f);
            bg.g = nk_propertyf(nk, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f);
            bg.b = nk_propertyf(nk, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f);
            bg.a = nk_propertyf(nk, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f);
            nk_combo_end(nk);
          }
#endif
        }
        nk_end(nk);

        /* Draw */
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        nk_glfw3_render(NK_ANTI_ALIASING_ON);

        glfwSwapBuffers(window);
      }
      glfwDestroyWindow(window);
    }
  }
  return 0;
}

