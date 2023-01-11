#include "e1000.h"

#define RX_RING_SIZE 16
#define TX_RING_SIZE 16

struct e1000 {
    uint32_t mmio_base;
    struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
    struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
    uint8_t irq;
};

struct e1000 *the_e1000_device;

unsigned int e1000_reg_read(struct e1000 *dev, uint16_t reg)
{
    //mmio領域を読む
    return *(volatile uint32_t *)(dev->mmio_base + reg);
}

void e1000_reg_write(struct e1000 *dev, uint16_t reg, uint32_t val)
{
    //mmio領域に書き込み
    *(volatile uint32_t *)(dev->mmio_base + reg) = val;
}

void mem_hexdump(void *mem, int len){
    for(int i=0;i<len;i++){
        if(i%8==0){
            cprintf("%04x : ",(uint)mem+i);
        }
        unsigned char *buf=mem+i;
        cprintf("%02x ",0xff &(*buf));
        if(i%8==7){
            cprintf("\n");
        }
    }
    cprintf("\n");
}

void dump_test(){
    cprintf("dump test\n");
    char test[]="stringstringstringstring";
    mem_hexdump(test,24);
}

void
mem_hexdump2(void *data, size_t size)
{
    int offset, index;
    unsigned char *src;

    src = (unsigned char *)data;
    cprintf("+------+-------------------------------------------------+------------------+\n");
    for (offset = 0; offset < (int)size; offset += 16) {
        cprintf("| %04x | ", offset);
        for (index = 0; index < 16; index++) {
            if (offset + index < (int)size) {
                cprintf("%02x ", 0xff & src[offset + index]);
            } else {
                cprintf("   ");
            }
        }
        cprintf(" |\n");
    }
    cprintf("+------+-------------------------------------------------+------------------+\n");
}

uint8_t e1000_tx(void *buf, uint16_t length){
    //dump_test();
    struct e1000 *dev=the_e1000_device;
    //tailの位置を取得
    uint32_t tail=e1000_reg_read(dev,E1000_TDT);
    struct tx_desc *desc=&(dev->tx_ring[tail]);
    //tx_descriptorへの設定
    desc->buffer_address=(uint64_t)(V2P(buf));
    desc->length=length;
    desc->status=0;
    desc->command = (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
    //tailを進める
    e1000_reg_write(dev, E1000_TDT, (tail + 1) % TX_RING_SIZE);
    //nic側で処理が終わるまで待つ
    cprintf("before : desc_status=%d \n",desc->status);
    while(!(desc->status & 0x0fu)) {   
        //microdelay(1);
    }
    cprintf("after : desc_status=%d \n",desc->status);
    return desc->length;
}

void e1000_rx(struct e1000 *dev){
    struct e1000_dev* dev=the_e1000_device;
    while(1){
        uint32_t next_tail=(e1000_reg_read(dev,E1000_RDT)+1)%RX_RING_SIZE;
        struct rx_desc *desc=&(dev->rx_ring[next_tail]);
        if(!(desc->status&E1000_RXD_STAT_DD)){
            return;
        }
        if(desc->errors){
            cprintf("e1000 recv error\n");
        }
        else{
            mem_hexdump(desc->addr,desc->length);
        }
        e1000_reg_write(dev,E1000_RDT,next_tail);
    }
}

void e1000_intr_handler(void){
    struct e1000 *dev;
    dev=the_e1000_device;
    int icr=e1000_reg_read(dev,E1000_ICR);
    if(icr&E1000_ICR_RXT0){
        e1000_rx(dev);
        e1000_reg_read(dev, E1000_ICR);
    }
}

void e1000_tx_init(struct e1000 *dev){
    //データシート14章参照
    // initialize tx descriptors
    for (int n = 0; n < TX_RING_SIZE; n++) {
        memset(&dev->tx_ring[n], 0, sizeof(struct tx_desc));
    }
    //レジスタへ設定
    uint64_t tx_reg_base = (uint64_t)(V2P(dev->tx_ring));//nicには物理アドレスを渡す
    e1000_reg_write(dev, E1000_TDBAL, (uint32_t)(tx_reg_base & 0xffffffff));
    e1000_reg_write(dev, E1000_TDBAH, (uint32_t)(tx_reg_base >> 32) );
    // tx descriptor length
    e1000_reg_write(dev, E1000_TDLEN, (uint32_t)(TX_RING_SIZE * sizeof(struct tx_desc)));
    // setup head/tail
    e1000_reg_write(dev, E1000_TDH, 0);
    e1000_reg_write(dev, E1000_TDT, 0);
    //TCTL
    uint32_t tctl=(E1000_TCTL_PSP |
                    E1000_TCTL_CT_HERE |
                    E1000_TCTL_COLD_HERE|
                    0);            
    e1000_reg_write(dev,E1000_TCTL,tctl); 
}

void e1000_rx_init(struct e1000 *dev){
    //データシート14章参照
    //レジスタへ設定
    for(int n = 0; n < RX_RING_SIZE; n++) {
        memset(&dev->rx_ring[n], 0, sizeof(struct rx_desc));
        //受信のときは受けとったデータを一時格納するための領域をドライバ側で確保
        dev->rx_ring[n].addr = (uint64_t)V2P(kalloc());
    }
    uint64_t rcv_ring_base=(uint64_t)V2P(dev->rx_ring);//nicには物理アドレスを渡す
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
    //データシート14章参照
    //general setting
    uint32_t ctl_val=E1000_CTL_FD | E1000_CTL_ASDE | E1000_CTL_SLU | E1000_CTL_FRCDPLX | E1000_CTL_SPEED | E1000_CTL_FRCSPD;
    e1000_reg_write(dev,E1000_CTL,ctl_val);
    //割り込み
    uint32_t ims_val=E1000_IMS_LSC | E1000_IMS_RXDMT0 | E1000_IMS_RXSEQ | E1000_IMS_RXO | E1000_IMS_RXT0;
    e1000_reg_write(dev, E1000_IMS, ims_val);
    //clear
    e1000_reg_read(dev, E1000_ICR);
    //rx_enabale
    uint32_t current_rctl=e1000_reg_read(dev,E1000_RCTL);
    uint32_t rctl=(E1000_RCTL_EN |
                    current_rctl);
    e1000_reg_write(dev,E1000_RCTL,rctl);
    //tx_enable
    uint32_t current_tctl=e1000_reg_read(dev,E1000_TCTL);
    uint32_t tctl=(E1000_TCTL_EN |
                    current_tctl);
    e1000_reg_write(dev,E1000_TCTL,tctl);
    if(e1000_reg_read(dev,E1000_TCTL)&E1000_TCTL_EN){
        cprintf("open \n");
    }
}

void e1000_close(struct e1000 *dev){
    //データシート14章参照
    //clear intr
    e1000_reg_read(dev, E1000_ICR);
    //rx_enabale
    uint32_t rctl=(~E1000_RCTL_EN &
                    e1000_reg_read(dev,E1000_RCTL));
    e1000_reg_write(dev,E1000_RCTL,rctl);
    //tx_enable
    uint32_t tctl=(~E1000_TCTL_EN |
                    e1000_reg_read(dev,E1000_TCTL));
    e1000_reg_write(dev,E1000_TCTL,tctl); 
}

void e1000_test_tx(void){
    //send test packet
    char buf[]="Hello,World";
    cprintf("test tx : buf_size=0x%x \n",sizeof(buf));
    e1000_tx(buf,sizeof(buf));
}

void e1000_init(void){
    uint32_t mmio_base=0;
    struct e1000 *e1000_dev = (struct e1000 *)kalloc();
    for(int i=0;i<num_device;i++){ //別関数に移す
        if(ReadDeviceId(&pci_devices[i])== 0x100e){
            struct PCIDevice *dev=&(pci_devices[i]);
            PCIFuncEnable(dev);
            uint32_t base = ReadBar32(dev,0);
            mmio_base= base & ~(uint32_t)(0xf);
            cprintf("find e1000 nic! : mmio_base=%x \n",mmio_base);
            e1000_dev->mmio_base=mmio_base;
            uint32_t irq_buf=ReadConfReg(&pci_devices[i],PCI_INTERRUPT_REG);
            e1000_dev->irq=PCI_INTERRUPT_LINE(irq_buf);
            cprintf("nic_irq=0x%x \n",e1000_dev->irq);
            ioapicenable(e1000_dev->irq,ncpu-1);//割り込みを有効化
            /*for (int n = 0; n < 128; n++){
                e1000_reg_write(e1000_dev, E1000_MTA + (n << 2), 0);   
            }*/
            e1000_rx_init(e1000_dev);
            e1000_tx_init(e1000_dev);
            the_e1000_device=e1000_dev;
            e1000_open(e1000_dev);
            break;
        }
    }
}