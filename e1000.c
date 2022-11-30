#include "e1000.h"



void e1000_init(void){
    for(int i=0;i<num_device;i++){
        if(ReadDeviceId(&pci_devices[i])== 0x10d3){
            cprintf("find e1000 nic!\n");
        }
    }
}