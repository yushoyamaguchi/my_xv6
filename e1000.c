#include "e1000.h"

#define RX_RING_SIZE 16
#define TX_RING_SIZE 16

struct e1000 {
    uint32_t mmio_base;
    struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));;
    struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));;
    uint8_t irq;
};

void e1000_intr_handler(void){

}

void e1000_init(void){
    uint64_t mmio_base=0;
    struct e1000 *e1000_dev = (struct e1000 *)kalloc();
    for(int i=0;i<num_device;i++){ //別関数に移す
        if(ReadDeviceId(&pci_devices[i])== 0x10d3){
            struct PCIDevice *dev=&(pci_devices[i]);
            uint64_t base = ReadBar(dev,0);
            mmio_base= base & ~(uint64_t)(0xf);
            cprintf("find e1000 nic! : mmio_base=%x \n",mmio_base);
        }
    }
    e1000_dev->mmio_base=mmio_base;
}