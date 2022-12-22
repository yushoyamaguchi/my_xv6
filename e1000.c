#include "e1000.h"

#define RX_RING_SIZE 16
#define TX_RING_SIZE 16

struct e1000 {
    uint32_t mmio_base;
    struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
    struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
    uint8_t irq;
};

unsigned int e1000_reg_read(struct e1000 *dev, uint16_t reg)
{
    return *(volatile uint32_t *)(dev->mmio_base + reg);
}

void e1000_reg_write(struct e1000 *dev, uint16_t reg, uint32_t val)
{
    *(volatile uint32_t *)(dev->mmio_base + reg) = val;
}

void e1000_intr_handler(void){

}

void e1000_tx_init(struct e1000 *dev){
    //レジスタへ設定
    uint64_t tx_reg_base = (uint64_t)(V2P(dev->tx_ring));//memmap_io領域(レジスタ)とのやりとりは物理アドレスで行う
    e1000_reg_write(dev, E1000_TDBAL, (uint32_t)(tx_reg_base & 0xffffffff));
    e1000_reg_write(dev, E1000_TDBAH, (uint32_t)(tx_reg_base >> 32) );
    // tx descriptor length
    e1000_reg_write(dev, E1000_TDLEN, (uint32_t)(TX_RING_SIZE * sizeof(struct tx_desc)));
    // setup head/tail
    e1000_reg_write(dev, E1000_TDH, 0);
    e1000_reg_write(dev, E1000_TDT, 0);
    //TCTL
    uint32_t tctl=(E1000_TCTL_PSP |
                    E1000_TCTL_CT |
                    E1000_TCTL_COLD|
                    0);
    e1000_reg_write(dev,E1000_TCTL,tctl); 
}

void e1000_rx_init(struct e1000 *dev){
    //レジスタへ設定
    for(int n = 0; n < RX_RING_SIZE; n++) {
        memset(&dev->rx_ring[n], 0, sizeof(struct rx_desc));
        //受信のときは受けとったデータを一時格納するための領域をドライバ側で確保
        dev->rx_ring[n].addr = (uint64_t)V2P(kalloc());
    }
    uint64_t rcv_ring_base=(uint64_t)V2P(dev->rx_ring);//memmap_io領域(レジスタ)とのやりとりは物理アドレスで行う
    e1000_reg_write(dev, E1000_RDBAL, (uint32_t)(rcv_ring_base & 0xffffffff));
    e1000_reg_write(dev, E1000_RDBAH, (uint32_t)(rcv_ring_base >> 32));
    // rx descriptor lengh
    e1000_reg_write(dev, E1000_RDLEN, (uint32_t)(RX_RING_SIZE * sizeof(struct rx_desc)));
    // setup head/tail
    e1000_reg_write(dev, E1000_RDH, 0);
    e1000_reg_write(dev, E1000_RDT, RX_RING_SIZE-1);
    //RCTL
    uint32_t rctl=(E1000_RCTL_BAM |
                    0);
    e1000_reg_write(dev,E1000_RCTL,rctl);

}

void e1000_open(struct e1000 *dev){
    //割り込み
    uint32_t ims_val=E1000_IMS_LSC | E1000_IMS_RXDMT0 | E1000_IMS_RXSEQ | E1000_IMS_RXO | E1000_IMS_RXT0;
    e1000_reg_write(dev, E1000_IMS, ims_val);
    //rx_enabale
    uint32_t rctl=(E1000_RCTL_EN |
                    0);
    e1000_reg_write(dev,E1000_RCTL,rctl);
    //tx_enable
    uint32_t tctl=(E1000_TCTL_EN |
                    0);
    e1000_reg_write(dev,E1000_TCTL,tctl); 
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
            e1000_dev->mmio_base=mmio_base;
            uint32_t irq_buf=ReadConfReg(&pci_devices[i],PCI_INTERRUPT_REG);
            e1000_dev->irq=PCI_INTERRUPT_LINE(irq_buf);
            cprintf("nic_irq=%x \n",e1000_dev->irq);
            ioapicenable(e1000_dev->irq,ncpu-1);//割り込みを有効化
            for (int n = 0; n < 128; n++){
                e1000_reg_write(dev, E1000_MTA + (n << 2), 0);   
            }
            e1000_rx_init(e1000_dev);
            e1000_tx_init(e1000_dev);
        }
    }
    e1000_dev->mmio_base=mmio_base;
}