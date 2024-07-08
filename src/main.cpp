/**
 *-----------------------------------------------------------------------------
 * Copyright (c) 2021-2024 Melbourne Instruments, Australia
 *-----------------------------------------------------------------------------
 * @file  main.cpp
 * @brief Main entry point to the FPGA Config utility.
 *-----------------------------------------------------------------------------
 */
#include <iostream>
#include <cstring>
#include <csignal>
#include <condition_variable>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <thread>
#include "common.h"
#include "version.h"
#include <sys/mman.h>

// Constants
constexpr uint NUM_CONSECUTIVE_GPIO_WRITES = 5;
constexpr char MEM_DEV_NAME[]              = "/dev/mem";
#if MELBINST_PI_HAT == 0
constexpr char FPGA1_BINARY_FILENAME[]     = "synthia_fpga_1.rbf";
constexpr char FPGA2_BINARY_FILENAME[]     = "synthia_fpga_2.rbf";
constexpr uint FPGA2_NCE_GPIO_PIN          = 2;
#elif MELBINST_PI_HAT == 1
constexpr char FPGA1_BINARY_FILENAME[]      = "monique.rbf";
#endif
constexpr uint DCLK_GPIO_PIN               = 3;
constexpr uint DATA0_GPIO_PIN              = 16;
constexpr uint NCONFIG_GPIO_PIN            = 17;
constexpr uint BOARD_REV_GPIO_PIN_1        = 20;
constexpr uint BOARD_REV_GPIO_PIN_2        = 21;
constexpr uint PAGE_SIZE                   = 4096;
constexpr uint BCM2711_PI4_PERIPHERAL_BASE = 0xFE000000;
constexpr uint GPIO_REGISTER_BASE          = 0x200000;
constexpr uint GPIO_SET_OFFSET             = 0x1C;
constexpr uint GPIO_CLR_OFFSET             = 0x28;
constexpr uint GPIO_RD_OFFSET              = 0x34;
constexpr uint GPIO_PULL_BASE_OFFSET       = 0xE4;
constexpr uint PHYSICAL_GPIO_BUS           = (0x7E000000 + GPIO_REGISTER_BASE);

// MACROs
#define RD_GPIO_PIN(pin)    (((*gpio_rd_reg) >> pin) & 0x01)
#define SET_GPIO_PIN(pin)   *gpio_set_reg = (1 << pin)
#define CLR_GPIO_PIN(pin)   *gpio_clr_reg = (1 << pin)
#define SET_DCLK_PIN()      { for (uint volatile i=0; i<NUM_CONSECUTIVE_GPIO_WRITES; i++) \
                                  SET_GPIO_PIN(DCLK_GPIO_PIN); }
#define CLR_DCLK_PIN()      { for (uint volatile i=0; i<NUM_CONSECUTIVE_GPIO_WRITES; i++) \
                                  CLR_GPIO_PIN(DCLK_GPIO_PIN); }

// Global variables
bool exit_flag = false;
bool exit_condition() {return exit_flag;}
std::condition_variable exit_notifier;
uint32_t *gpio_port;
volatile uint32_t *gpio_set_reg;
volatile uint32_t *gpio_clr_reg;
volatile uint32_t *gpio_rd_reg;
uint8_t *binary_data = 0;
uint binary_data_size = 0;

// Local functions
void _open_and_setup_gpio();
void *_mmap_bcm_register_base(off_t register_base);
void _init_gpio_pin(int pin, bool output);
void _close_gpio();
void _config_fpga1();
#if MELBINST_PI_HAT == 0
void _config_fpga2();
#endif
void _transfer_data();
void _print_app_info();
void _print_board_rev_info();
void _sigint_handler([[maybe_unused]] int sig);

//----------------------------------------------------------------------------
// main
//----------------------------------------------------------------------------
int main(void)
{
    // Setup the exit signal handler (e.g. ctrl-c, kill)
    ::signal(SIGINT, _sigint_handler);
    ::signal(SIGTERM, _sigint_handler);

    // Show the app info
    _print_app_info();

    // Open and setup the GPIO
    _open_and_setup_gpio();
    
    // Show the board rev info
    _print_board_rev_info();

    // Was the GPIO open and setup setup ok?
    if (gpio_port)
    {
        // Configure FPGA1
        _config_fpga1();

#if MELBINST_PI_HAT == 0
        // Free any allocated memory
        if (binary_data)
        {
            // Free it
            delete [] binary_data;
        }
        binary_data = 0;

        // Configure FPGA2
        _config_fpga2();
#endif
    }

    // Free any allocated memory
    if (binary_data)
    {
        // Free it
        delete [] binary_data;
    }

    // Close the GPIO port
    _close_gpio();

    // FPGA Config finished
    MSG("\nFPGA Config completed");
    return 0;
}

//----------------------------------------------------------------------------
// _open_and_setup_gpio
//----------------------------------------------------------------------------
void _open_and_setup_gpio()
{
    // Get a pointer to the GPIO registers
    gpio_port = reinterpret_cast<uint32_t *>(_mmap_bcm_register_base(GPIO_REGISTER_BASE));
    if (gpio_port)
    {
        // Set the set/clr registers pointers
        gpio_set_reg = gpio_port + (GPIO_SET_OFFSET / sizeof(uint32_t));
        gpio_clr_reg = gpio_port + (GPIO_CLR_OFFSET / sizeof(uint32_t));
        gpio_rd_reg = gpio_port + (GPIO_RD_OFFSET / sizeof(uint32_t));

        // Initialise each required GPIO pin
        _init_gpio_pin(NCONFIG_GPIO_PIN, true);
#if MELBINST_PI_HAT == 0
        _init_gpio_pin(FPGA2_NCE_GPIO_PIN, true);
#endif
        _init_gpio_pin(DCLK_GPIO_PIN, true);
        _init_gpio_pin(BOARD_REV_GPIO_PIN_1, false);
        _init_gpio_pin(BOARD_REV_GPIO_PIN_2, false);

        // Set the initial state of each pin
#if MELBINST_PI_HAT == 0
        SET_GPIO_PIN(FPGA2_NCE_GPIO_PIN);
#endif
        CLR_GPIO_PIN(DCLK_GPIO_PIN);
        CLR_GPIO_PIN(DATA0_GPIO_PIN);
        CLR_GPIO_PIN(NCONFIG_GPIO_PIN);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  
        MSG("GPIO open and setup");
    }
    else
    {
        // An error occurred setting up the GPIO
        MSG("GPIO open/setup error");
    }
}

//----------------------------------------------------------------------------
// _mmap_bcm_register_base
//----------------------------------------------------------------------------
void *_mmap_bcm_register_base(off_t register_base)
{
    uint32_t *addr = nullptr;
    int mem_fd;

    // Open the memory device
    mem_fd = ::open(MEM_DEV_NAME, O_RDWR|O_SYNC);
    if (mem_fd < 0)
    {
        // Error opening the device
        return nullptr;
    }

    // Map the register base
    addr = reinterpret_cast<uint32_t *>(::mmap(NULL, PAGE_SIZE, (PROT_READ|PROT_WRITE),
                                MAP_SHARED, mem_fd,
                                (BCM2711_PI4_PERIPHERAL_BASE + register_base)));
    ::close(mem_fd);

    // Was the map successful?    
    if (addr == MAP_FAILED)
    {
        // Error mapping the register base
        return nullptr;
    }
    return addr;
}

//----------------------------------------------------------------------------
// _init_gpio_pin
//----------------------------------------------------------------------------
void _init_gpio_pin(int pin, bool output) 
{
    // Set as an output or input pin
    if (output)
    {
        // Set the pin as an output
        *(gpio_port + (pin / 10)) |= (1 << ((pin % 10) * 3));
    }
    else
    {
        // Set the pin as an input with pull-up
        *(gpio_port + (pin / 10)) &= ~(7 << ((pin % 10) * 3));
        uint32_t pull_reg_offset = (GPIO_PULL_BASE_OFFSET / sizeof(uint32_t)) + ((pin / 16) * 4);
        uint32_t pull_bits_offset = (pin % 16) * 2;
        *(gpio_port + pull_reg_offset) &= ~(3 << pull_bits_offset);
        *(gpio_port + pull_reg_offset) |= (1 << pull_bits_offset);
    }
}

//----------------------------------------------------------------------------
// _config_fpga1
//----------------------------------------------------------------------------
void _config_fpga1()
{
    // Open the FPGA1 binary image
    std::ifstream file (FPGA_BINARY_FILE_PATH(FPGA1_BINARY_FILENAME), (std::ios::in|std::ios::binary|std::ios::ate));
    if (!file.is_open())
    {
        MSG("Could not open the FPGA1 binary file");
        return;           
    }

    // Allocate memory to read the image into
    // Note: due to the file size of ~200kB, the entire file is read into RAM
    auto file_size = file.tellg();
    binary_data = new uint8_t[file_size];
    if (!binary_data)
    {
        MSG("Could not allocate memory to read the FPGA1 binary file");
        return;          
    }
    binary_data_size = file_size;
    MSG("FPGA1 binary file size: "<< binary_data_size << " bytes");

    // Read the binary image into memory
    file.seekg (0, std::ios::beg);
    file.read (reinterpret_cast<char *>(binary_data), file_size);
    file.close();

    // Set nCONFIG high to put the FPGAs into config mode, and wait 1ms
    SET_GPIO_PIN(NCONFIG_GPIO_PIN);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // Transfer the data
    auto start = std::chrono::system_clock::now();
    _transfer_data();
    auto end = std::chrono::system_clock::now();
    std::cout << "FPGA1 configured, " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
}

#if MELBINST_PI_HAT == 0
//----------------------------------------------------------------------------
// _config_fpga2
//----------------------------------------------------------------------------
void _config_fpga2()
{
    // Open the FPGA2 binary image
    std::ifstream file (FPGA_BINARY_FILE_PATH(FPGA2_BINARY_FILENAME), (std::ios::in|std::ios::binary|std::ios::ate));
    if (!file.is_open())
    {
        MSG("Could not open the FPGA2 binary file");
        return;           
    }

    // Allocate memory to read the image into
    // Note: due to the file size of ~200kB, the entire file is read into RAM
    auto file_size = file.tellg();
    binary_data = new uint8_t[file_size];
    if (!binary_data)
    {
        MSG("Could not allocate memory to read the FPGA2 binary file");
        return;          
    }
    binary_data_size = file_size;
    MSG("FPGA2 binary file size: "<< binary_data_size << " bytes");

    // Read the binary image into memory
    file.seekg (0, std::ios::beg);
    file.read (reinterpret_cast<char *>(binary_data), file_size);
    file.close();

    // Set FPGA2 nCE low to select the second FPGA, and wait 1ms
    CLR_GPIO_PIN(FPGA2_NCE_GPIO_PIN);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // Transfer the data
    auto start = std::chrono::system_clock::now();
    _transfer_data();
    auto end = std::chrono::system_clock::now();
    std::cout << "FPGA2 configured, " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
}
#endif

//----------------------------------------------------------------------------
// _transfer_data
//----------------------------------------------------------------------------
void _transfer_data()
{
    uint8_t *data = binary_data;

    // Do until all file data has been processed, or the program exited
    while (!exit_flag && (data < (binary_data + binary_data_size)))
    {
        uint8_t byte = *data++;

        // Send each bit in the byte, LS bit first
        for (int i=0; i<8; i++)
        {
            // Get the bit and either set/clear the GPIO pin
            uint8_t bit = (byte >> i) & 0x01;
            if (bit)
            {
                SET_GPIO_PIN(DATA0_GPIO_PIN);
            }
            else
            {
                CLR_GPIO_PIN(DATA0_GPIO_PIN);
            }

            // Set the DCLK rising edge
            SET_DCLK_PIN();

            // Set the DCLK falling edge
            CLR_DCLK_PIN();
        }
    }

    // We need to keep clocking DCLK once the FPGA has accepted the data and set
    // CONF_DONE high. It needs "at least" 2 falling DCLK edges after setting CONF_DONE, 
    // but to be safe lets send 10
    int dclk_count = 10;
    while (!exit_flag && dclk_count--)
    {
        // Set the DCLK rising edge
        SET_DCLK_PIN();

        // Set the DCLK falling edge
        CLR_DCLK_PIN();
    }
}

//----------------------------------------------------------------------------
// _close_gpio
//----------------------------------------------------------------------------
void _close_gpio()
{
    // Ummap the GPIO device if open
    if (gpio_port)
    {
        // Set the default GPIO values
        CLR_GPIO_PIN(DCLK_GPIO_PIN);
        CLR_GPIO_PIN(DATA0_GPIO_PIN);

        // Unmap it
        ::munmap(gpio_port, PAGE_SIZE);
        gpio_port = nullptr;
        MSG("GPIO port closed");
    }
}

//----------------------------------------------------------------------------
// _print_app_info
//----------------------------------------------------------------------------
void _print_app_info()
{
    MSG("FPGA CONFIG - Copyright (c) 2023-2024 Melbourne Instruments, Australia");
    MSG("Version " << FPGA_CONFIG_MAJOR_VERSION << "." << FPGA_CONFIG_MINOR_VERSION << "." << FPGA_CONFIG_PATCH_VERSION);
    MSG("");
}

//----------------------------------------------------------------------------
// _print_board_rev_info
//----------------------------------------------------------------------------
void _print_board_rev_info()
{
    uint rev = RD_GPIO_PIN(BOARD_REV_GPIO_PIN_1) + (RD_GPIO_PIN(BOARD_REV_GPIO_PIN_2) << 1);
    switch(rev)
    {
        case 0:
            MSG("Detected Board Rev D");
            break;
        
        case 1:
            MSG("Detected Board Rev B");
            break;

        case 2:
            MSG("Detected Board Rev C");
            break;

        case 3:
            MSG("Detected Board Rev A");
            break;

        default:
            break;
    }
}

//----------------------------------------------------------------------------
// _sigint_handler
//----------------------------------------------------------------------------
void _sigint_handler([[maybe_unused]] int sig)
{
    // Signal to exit the app
    exit_flag = true;
    exit_notifier.notify_one();
}
