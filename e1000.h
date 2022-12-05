#pragma once
#include "pci.h"
#include "defs.h"
#include "types.h"


//defined in pci.c
extern struct PCIDevice pci_devices[32];
extern int num_device;


struct tx_desc
{
    uint64_t buffer_address;
    uint16_t length;
    uint8_t  checksum_offset;
    uint8_t  command;
    uint8_t  status : 4;
    uint8_t  reserved : 4;
    uint8_t  checksum_start_field;
    uint16_t special;
}__attribute__((packed));


struct rx_desc
{
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t status;
    uint8_t errors; 
    uint16_t special;
}__attribute__((packed));