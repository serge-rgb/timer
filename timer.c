#if defined(BUILD_SCRIPT)

if [ ! -d build ]; then
   mkdir build
fi

glfw_lib=glfw/build/src/libglfw3.a


if [ ! -f $glfw_lib ]; then
   pushd glfw
   if [ ! -d build ]; then
   mkdir build
   fi
   pushd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-g -DGLFW_USE_RETINA=1 -DGLFW_EXPOSE_NATIVE_COCOA=1
   make -j
   popd
   popd
fi

cflags="-ObjC -g -Iglfw/include -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo"

clang $cflags timer.c $glfw_lib -o build/timer

if [ ! -f icons/timer.icns ]; then
   cd icons
   ./mkicons.sh
   cd ..
fi

err=$?
if [ $err = 0 ]; then
   mkdir -p build/Timer.app/Contents/MacOS
   mkdir -p build/Timer.app/Contents/Resources
   cp Info.plist build/Timer.app/Contents/Info.plist
   cp DroidSans.ttf build/Timer.app/Contents/MacOS/DroidSans.ttf
   cp icons/timer.icns build/Timer.app/Contents/Resources/Timer.icns
   mv build/timer build/Timer.app/Contents/MacOS/timer
fi


exit $err

#endif


// POSIX
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// Local
#include <GLFW/glfw3.h>

// macOS
#if defined(__MACH__)
#include <AppKit/AppKit.h>
#include <mach-o/dyld.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#endif  // __MACH__


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
#define true 1
#define false 0
#define Zero {0}
#define MsgLen 10000
#define Type(name) struct name; typedef struct name name

#define Minutes(v) (v*60)
#define Hours(v) (Minutes(60*v))
#define Days(v) (24*(Hours(v)))
#define Weeks(v) (7*Days(v))
#define Months(v) (30*Days(v))
#define Years(v) (365.25f*Days(v))

#define TimeBlockMinutes 25
#define TimerEpoch 1517696211

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


#pragma pack(push, 1)
   struct TimeBlock {
      u32 begin;  // timestamp relative to TimerEpoch
      u16 duration;
   };
   Type(TimeBlock);

   // 100 years of 32 daily blocks of time.
   #define BlockCount (100 * 365 * 32)

   struct State {
      TimeBlock blocks[BlockCount];
      i64       n_blocks;
   };
   Type(State);
#pragma pack(pop)

enum blockFsm {
   WAITING,
   INSIDE_BLOCK,
   ERROR,
};
struct GuiState {
   u64 lookbackDistance_s;
   int blockFsm;
};
Type(GuiState);

void
init_gui_state(GuiState* gui) {
   gui->lookbackDistance_s = Hours(8);
   gui->blockFsm = WAITING;
}

void
print_errno_err(int err) {
   switch(err) {
#define ErrCase(err) case(err): { fprintf(stderr, "POSIX error: " #err "\n"); } break
      ErrCase(E2BIG); ErrCase(EACCES); ErrCase(EBADF); ErrCase(EBUSY); ErrCase(ECHILD);
      ErrCase(EDEADLK); ErrCase(EEXIST); ErrCase(EFAULT); ErrCase(EFBIG); ErrCase(EINTR);
      ErrCase(EINVAL); ErrCase(EIO); ErrCase(EISDIR); ErrCase(EMFILE); ErrCase(EMLINK);
      ErrCase(ENFILE); ErrCase(ENODEV); ErrCase(ENOENT); ErrCase(ENOEXEC); ErrCase(ENOMEM);
      ErrCase(ENOSPC); ErrCase(ENOTBLK); ErrCase(ENOTDIR); ErrCase(ENOTTY); ErrCase(ENXIO);
      ErrCase(EOVERFLOW); ErrCase(EPERM); ErrCase(EPIPE); ErrCase(EROFS); ErrCase(ESPIPE);
      ErrCase(ESRCH); ErrCase(ETXTBSY); ErrCase(EXDEV);
      default: {
         fprintf(stderr, "unhandled POSIX error\n");
      }
#undef ErrCase
   }
}

void
print_errno() {
   int err = errno;
   print_errno_err(err);
}

// TODO: Write a stb-style library that handles OS paths seamlessly
// TODO: Memory mapping on Windows.
State*
open_database(char* fname) {
   State* s = NULL;

   bool clearMem = false;
   off_t fileLen = sizeof(State);

   int fd = open(fname, O_RDWR, S_IRWXU);
   if (fd == -1) {
      int err = errno;
      if (err != ENOENT) {
         print_errno_err(err);
      }
      else {
         clearMem = true;
         fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
         if (fd == -1) {
            print_errno();
         }
         else if (ftruncate(fd, fileLen) == -1) {
            print_errno();
            fd = -1;
         }
      }
   }

   if (fd != -1) {
      void* p = mmap(NULL, fileLen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (s == MAP_FAILED) {
         print_errno();
      }
      else {
         s = p;
         if (clearMem) {
            memset(s, 0, fileLen);
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
time_pretty_roughly(char* out, sz len, u64 sec) {
   int val = sec;
   char* s = "";
   if (sec < Minutes(1)) {
      s = "seconds";
   }
   else if (sec < Hours(1)) {
      val /= Minutes(1);
      s = "minutes";
   }
   else if (sec <  Days(1)) {
      val /= Hours(1);
      s = "hours";
   }
   else if (sec < Weeks(1)) {
      val /= Days(1);
      s = "days";
   }
   else if (sec < Months(1)) {
      val /= Weeks(1);
      s = "weeks";
   }
   else if (sec < Years(1)) {
      val /= Months(1);
      s = "months";
   }
   else {
      val /= Years(1);
      s = "years";
   }
   snprintf(out, len, "%d %s", val, s);
}

void
time_pretty(char* out, sz len, u64 unix) {
   u32 sec = unix % 60;
   u32 min = (unix / 60) % 60;
   u32 hr  = (unix / (60*60));
   snprintf(out, len, "%02d:%02d:%02d", hr, min, sec);
}

#if defined(__MACH__)
NSAlert*
mac_alert(const char* info, const char* title) {
   NSAlert *alert        = [[NSAlert alloc] init];
   alert.messageText     = [NSString stringWithUTF8String:title ? title : ""];
   alert.informativeText = [NSString stringWithUTF8String:info ? info : ""];
   return alert;
}

void
show_message_box(char* info, char* title) {
   @autoreleasepool {
      [mac_alert(info, title) runModal];
   }
}

void
fname_at_binary(char* fname, sz lenFname) {
   char buffer[MsgLen] = Zero; {
      strncpy(buffer, fname, MsgLen);
   }
   u32 intLen = lenFname;
   _NSGetExecutablePath(fname, &intLen);
   /* Remove the executable name */ {
      char* last_slash = fname;
      for(char* iter = fname; *iter != '\0'; ++iter) {
         if (*iter == '/') {
            last_slash = iter;
         }
      }
      *(last_slash+1) = '\0';
   }
   strncat(fname, "/", lenFname);
   strncat(fname, buffer, lenFname);
}

#endif // __MACH__

void
set_style(struct nk_context* nk) {
   struct nk_color table[NK_COLOR_COUNT];
   table[NK_COLOR_TEXT]                    = nk_rgba(70, 70, 70, 255);
   table[NK_COLOR_WINDOW]                  = nk_rgba(175, 175, 175, 255);
   table[NK_COLOR_HEADER]                  = nk_rgba(175, 175, 175, 255);
   table[NK_COLOR_BORDER]                  = nk_rgba(0, 0, 0, 255);
   table[NK_COLOR_BUTTON]                  = nk_rgba(185, 185, 185, 255);
   table[NK_COLOR_BUTTON_HOVER]            = nk_rgba(170, 170, 170, 255);
   table[NK_COLOR_BUTTON_ACTIVE]           = nk_rgba(160, 160, 160, 255);
   table[NK_COLOR_TOGGLE]                  = nk_rgba(150, 150, 150, 255);
   table[NK_COLOR_TOGGLE_HOVER]            = nk_rgba(120, 120, 120, 255);
   table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(175, 175, 175, 255);
   table[NK_COLOR_SELECT]                  = nk_rgba(190, 190, 190, 255);
   table[NK_COLOR_SELECT_ACTIVE]           = nk_rgba(175, 175, 175, 255);
   table[NK_COLOR_SLIDER]                  = nk_rgba(190, 190, 190, 255);
   table[NK_COLOR_SLIDER_CURSOR]           = nk_rgba(80, 80, 80, 255);
   table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(70, 70, 70, 255);
   table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(60, 60, 60, 255);
   table[NK_COLOR_PROPERTY]                = nk_rgba(175, 175, 175, 255);
   table[NK_COLOR_EDIT]                    = nk_rgba(150, 150, 150, 255);
   table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(0, 0, 0, 255);
   table[NK_COLOR_COMBO]                   = nk_rgba(175, 175, 175, 255);
   table[NK_COLOR_CHART]                   = nk_rgba(160, 160, 160, 255);
   table[NK_COLOR_CHART_COLOR]             = nk_rgba(45, 45, 45, 255);
   table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba( 255, 0, 0, 255);
   table[NK_COLOR_SCROLLBAR]               = nk_rgba(180, 180, 180, 255);
   table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(140, 140, 140, 255);
   table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = nk_rgba(150, 150, 150, 255);
   table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(160, 160, 160, 255);
   table[NK_COLOR_TAB_HEADER]              = nk_rgba(180, 180, 180, 255);
   nk_style_from_table(nk, table);
}

int
main() {
   char dbPath[MsgLen] = "db"; {
      fname_at_binary(dbPath, MsgLen);
   }
   State* state = open_database(dbPath);
   if (!glfwInit()) {
      fprintf(stderr, "Could not initialize GLFW.\n");
   }
   else if (!state) {
      fprintf(stderr, "Could not open database.\n");
   }
   else {
      int width = 500, height=150;
      glfwSetErrorCallback(error);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
      GLFWwindow* window = glfwCreateWindow(width, height, "Timer", NULL, NULL);
      if (!window) {
         fprintf(stderr, "Could not create window.\n");
      }
      else {
         glfwSetKeyCallback(window, key_callback);
         glfwMakeContextCurrent(window);
         glfwSwapInterval(1);

         struct nk_context* nk = nk_glfw3_init(window, NK_GLFW3_INSTALL_CALLBACKS);
         /*NK font atlas*/ {
            char fontPath[MsgLen] = "DroidSans.ttf"; {
               fname_at_binary(fontPath, MsgLen);
            }
            struct nk_font_atlas *atlas;
            nk_glfw3_font_stash_begin(&atlas);
            struct nk_font *droid = nk_font_atlas_add_from_file(atlas, fontPath, 14, 0);
            nk_glfw3_font_stash_end();
            // nk_style_load_all_cursors(nk, atlas->cursors);
            nk_style_set_font(nk, &droid->handle);
         }

         set_style(nk);

         time_t blockBegin = Zero;

         GuiState gui; {
            init_gui_state(&gui);
         }
         static int buttonWidth = 135;

         while (!glfwWindowShouldClose(window)) {
            u64 now = (u64)time(0);
            /* Input */
            glfwPollEvents();
            nk_glfw3_new_frame();

            /* GUI */
            if (nk_begin(nk, "Timer", nk_rect(0, 0, width, height), 0)) {
               static char* errorMessage = "undefined";
               if (gui.blockFsm == WAITING) {
                  u64 timeSum = 0; {
                     u64 distance = 0;
                     for (i64 n = state->n_blocks-1;
                          n >= 0 && distance < gui.lookbackDistance_s;
                          --n) {
                        TimeBlock* b = &state->blocks[n];
                        distance = now - (b->begin + TimerEpoch);
                        timeSum += b->duration;
                     }
                  }
                  char elapsedMsg[MsgLen] = Zero; {
                     char timeRoughly[MsgLen] = Zero; {
                        time_pretty_roughly(timeRoughly, MsgLen, gui.lookbackDistance_s);
                     }
                     char timeStr[MsgLen] = Zero; {
                        time_pretty(timeStr, MsgLen, timeSum);
                     }
                     snprintf(elapsedMsg, MsgLen, "Time elapsed past %s: %s", timeRoughly, timeStr);
                  }
                  // nk_layout_row_static(nk, 20, width, 0);  /*Vertical space*/
                  nk_layout_row_begin(nk, NK_STATIC, 30, 4); {
                     nk_layout_row_push(nk, 50);
                     nk_label(nk, "Window:", NK_TEXT_LEFT);

                     nk_layout_row_push(nk, 60);
                     if (nk_button_label(nk, "More")) {
                        gui.lookbackDistance_s *= 2;
                     }
                     nk_layout_row_push(nk, 60);
                     if (nk_button_label(nk, "1 hour")) {
                        gui.lookbackDistance_s = Hours(1);
                     }

                     nk_layout_row_push(nk, 250);
                     nk_label(nk, elapsedMsg, NK_TEXT_CENTERED);
                     nk_layout_row_end(nk);
                  }

                  nk_layout_row_dynamic(nk, 30, 1); {
                     nk_label(nk, "Timer", NK_TEXT_LEFT);
                  }

                  nk_layout_row_static(nk, 30, buttonWidth, 1); {
                     if (nk_button_label(nk, "Begin Time Block.")) {
                        gui.blockFsm = INSIDE_BLOCK;
                        blockBegin = time(0);
                     }
                  }
               }
               else if (gui.blockFsm == INSIDE_BLOCK) {
                  bool endBlock = false;
                  if (now - blockBegin > TimeBlockMinutes*60) {
                     show_message_box("Time block done!", "Timer");
                     endBlock = true;
                  }
                  /*draw elapsed time*/ {
                     nk_layout_row_dynamic(nk, 30, 1);
                     time_t now = time(NULL);
                     u32 relnow = now - blockBegin;
                     char msg[MsgLen] = Zero;
                     time_pretty(msg,MsgLen, (u64)relnow);
                     nk_label(nk, msg, NK_TEXT_CENTERED);
                  }
                  nk_layout_row_static(nk, 30, buttonWidth, 2);
                  if (nk_button_label(nk, "End Time Block.")) {
                     endBlock = true;
                  }
                  if (endBlock) {
                     if (now - blockBegin >= 65536) {
                        gui.blockFsm = ERROR;
                        errorMessage = "time block too long!";
                     }
                     else {
                        TimeBlock* block = &state->blocks[state->n_blocks++];
                        block->duration = now - blockBegin;
                        block->begin = blockBegin - TimerEpoch;
                        gui.blockFsm = WAITING;
                     }
                  }
               }
               else if (gui.blockFsm == ERROR) {
                  nk_layout_row_dynamic(nk, 30, 2);
                  char errmsg[MsgLen] = Zero;
                  snprintf(errmsg, MsgLen, "Error: %s", errorMessage);
                  nk_label(nk, errmsg, NK_TEXT_CENTERED);
                  if (nk_button_label(nk, "Ok")) {
                     gui.blockFsm = WAITING;
                  }
               }
            }
            nk_end(nk);

            /* Draw */
            nk_glfw3_render(NK_ANTI_ALIASING_ON);

            glfwSwapBuffers(window);
         }
         glfwDestroyWindow(window);
      }
   }
   return 0;
}

