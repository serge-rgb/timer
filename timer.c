#if defined(BUILD_SCRIPT)

if [ ! -d build ]; then
   mkdir build
fi

glfw_lib=glfw/build/src/libglfw3.a


if [ ! -f $glfw_lib ]; then
   ./build.sh
fi

cflags="-Iglfw/include -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo "

clang $cflags timer.c $glfw_lib -o build/timer

exit

#endif


#include <stdio.h>

#include <GLFW/glfw3.h>

#define NK_IMPLEMENTATION
#include "nuklear.h"

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


int
main() {
   puts("Well hello there!");
   if (!glfwInit()) {
      fprintf(stderr, "Could not initialize GLFW.");
   }
   else {
      glfwSetErrorCallback(error);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
      GLFWwindow* window = glfwCreateWindow(400, 300, "Timer", NULL, NULL);
      if (!window) {
         fprintf(stderr, "Could not create window.\n");
      }
      else {
         // TODO: Need to load GL functions?

         glfwSetKeyCallback(window, key_callback);
         glfwMakeContextCurrent(window);
         glfwSwapInterval(1);

         while (!glfwWindowShouldClose(window)) {
            glfwSwapBuffers(window);
            glClearColor(1,0,1,1);
            glClear(GL_COLOR_BUFFER_BIT);

            glfwPollEvents();
         }
         glfwDestroyWindow(window);
      }
   }
}
