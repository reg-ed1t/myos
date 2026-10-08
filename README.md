# myos

A tiny hobby OS I'm making to learn how operating systems work.

It's still very much a work in progress.

Currently contains some basic things and whatever else I happen to be experimenting with.

Don't expect anything particularly useful yet.

## Features

- VGA text output
- GDT and IDT setup
- Interrupt handling
- Physical memory management
- Virtual memory management
- Pages
- Basic commands support
- Basic usermode
- Basic file operations

## Building

`make release`

Or, if you want to debug it:

`make debug`

### Other makefile arguments

| Argument                 | Description                                |
|:-------------------------|:-------------------------------------------|
| `TOOLCHAIN=clang`        | Compile project using Clang toolchain      |
| `TOOLCHAIN=gcc`          | Compile project using GCC (`i386-elf-gcc`) |
| `CROSS_PREFIX=i386-elf-` | Set target architecture prefix for GCC     |
