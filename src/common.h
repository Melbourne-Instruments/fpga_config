/**
 *-----------------------------------------------------------------------------
 * Copyright (c) 2021-2024 Melbourne Instruments, Australia
 *-----------------------------------------------------------------------------
 * @file  common.h
 * @brief Common definitions.
 *-----------------------------------------------------------------------------
 */
#ifndef _COMMON_H
#define _COMMON_H

#include <chrono>

// MACRO to show a string on the console
#define MSG(str) do { std::cout << str << std::endl; } while( false )

// MACRO to show a debug string on the console
#ifndef NDEBUG
#define DEBUG_MSG(str) MSG(str)
#else
#define DEBUG_MSG(str) do { } while ( false )
#endif

// MACRO to get the full path of an FPGA Config file
#if MELBINST_PI_HAT == 0
constexpr char FPGA_BINARIES_DIR[] = "/home/root/nina/firmware/";
#elif MELBINST_PI_HAT == 1
constexpr char FPGA_BINARIES_DIR[] = "/home/root/delia/firmware/";
#else
#error Unknown Melbourne Instruments RPi target device
#endif
#define FPGA_BINARY_FILE_PATH(filename)  (FPGA_BINARIES_DIR + std::string(filename))

#endif  // _COMMON_H
