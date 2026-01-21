Process Memory Allocation Kernel Module
Overview

This project implements a Linux kernel module that handles memory allocation and deallocation requests from user processes by directly managing virtual memory mappings. The module exposes a virtual device and services requests using an ioctl interface, similar to mmap and munmap.

Developed on Ubuntu Linux (x86_64) as part of an Operating Systems course.

Technologies

C, C++, Linux, Ubuntu, x86_64, GDB, ioctl, virtual memory

Key Features

Implemented a kernel module exposing /dev/memalloc for user–kernel communication

Walked multi-level page tables (PGD → PTE) to validate and map virtual addresses

Allocated physical pages and mapped them with correct permissions

Enforced allocation limits and returned precise error codes for safety

Why This Project Is Important

This project demonstrates low-level systems expertise beyond typical application development, including kernel programming, memory management, and safe user–kernel interaction. It reflects the ability to reason about correctness, performance, and resource isolation in environments where errors can crash the system, skills that directly translate to backend, infrastructure, and systems software roles.

Disclaimer

Academic project for educational purposes only.
