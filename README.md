# Timer

This is a simple time tracking app for OSX

I don't think anyone other than me will ever use it, but I am making open source because it's a good reference for a self-contained OpenGL app for macOS with retina-display support and icons. Also, it's a pretty good example of why memory-mapped file I/O is awesome.

## Requirements

- clang
- imagemagick for generating icons
- macos only for now but that will probably change soon

## Building

It's a self building C-file. Do 

```
./timer.c
```
and after it's done, there should be a `build/Timer.app` bundle.
