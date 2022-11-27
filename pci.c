#include "types.h"
#include "defs.h"
#include "x86.h"
#include "pci.h"



/*
 *Respectful reference from https://github.com/uchan-nos/mikanos
 */

struct PCIDevice pci_devices[32];

int num_device;

uint32_t shl (uint32_t x, unsigned int bits) {
    return x << bits;
}

uint32_t MakeAddress(uint8_t bus, uint8_t device,
                    uint8_t function, uint8_t reg_addr) {

    return shl(1, 31)  // enable bit
        | shl(bus, 16)
        | shl(device, 11)
        | shl(function, 8)
        | (reg_addr & 0xfcu);
}

static uint32_t
ReadData()
{
	return inl(CONFIG_DATA);
}

void WriteAddress(uint32_t address) {
    outl(CONFIG_ADDRESS, address);
}

void WriteData(uint32_t value) {
    outl(CONFIG_DATA, value);
}

uint32_t ReadConfReg(struct PCIDevice *dev, uint8_t reg_addr) {
    WriteAddress(MakeAddress(dev->bus, dev->device, dev->function, reg_addr));
    return ReadData();
}

void WriteConfReg(struct PCIDevice *dev, uint8_t reg_addr, uint32_t value) {
    WriteAddress(MakeAddress(dev->bus, dev->device, dev->function, reg_addr));
    WriteData(value);
}

uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
    WriteAddress(MakeAddress(bus, device, function, 0x0c));
    return (ReadData() >> 16) & 0xffu;
}

bool IsSingleFunctionDevice(uint8_t header_type) {
    return (header_type & 0x80u) == 0;
}

uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function) {
    WriteAddress(MakeAddress(bus, device, function, 0x00));
    return ReadData() & 0xffffu;
}

int AddDevice(struct PCIDevice* device) {
    if (num_device == sizeof(pci_devices)/sizeof(struct PCIDevice)) {
      return (-1);
    }
    memcpy(&pci_devices[num_device],device,sizeof(struct PCIDevice));
    ++num_device;
    cprintf("add_a_pci_device\n");
    return 1;
}

int ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t header_type = ReadHeaderType(bus, device, function);
    struct PCIDevice dev={bus, device, function, header_type};
    int err;
    if ((err = AddDevice(&dev))<0) {
      return err;
    }
    //PCI-PCIブリッジは考慮しない

    return 1;
}


int ScanDevice(uint8_t bus, uint8_t device) {
    int err;
    if ((err = ScanFunction(bus, device, 0))<0) {
      return err;
    }
    if (IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))) {
      return 1;
    }

    for (uint8_t function = 1; function < 8; ++function) {
      if (ReadVendorId(bus, device, function) == 0xffffu) {
        continue;
      }
      if ((err = ScanFunction(bus, device, function))<0) {
        return err;
      }
    }
    return 1;
}


int ScanBus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; ++device) {
      if (ReadVendorId(bus, device, 0) == 0xffffu) {
        continue;
      }
      int err;
      if ((err = ScanDevice(bus, device))<0) {
        return err;
      }
    }
    return 1;
}

int ScanAllBus() {
    num_device = 0;

    uint8_t header_type = ReadHeaderType(0, 0, 0);
    if (IsSingleFunctionDevice(header_type)) {
        return ScanBus(0);
    }

    for (uint8_t function = 0; function < 8; ++function) {
      if (ReadVendorId(0, 0, function) == 0xffffu) {
        continue;
      }
      int err;
      if ((err = ScanBus(function))<0) {
        return err;
      }
    }
    return 1;
}


void pciinit(void){
    if(ScanAllBus()<0){
        exit();
    }
}
