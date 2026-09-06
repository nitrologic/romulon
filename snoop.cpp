// snoop.cpp
// (c)2026 nitrologic
// mit license

#include "snoop.h"
#include "picosdk/picosdk.h"
#include "snoop.pio.h"

static PIO snoop_pio = pio0;
static uint snoop_sm  = 0;
static uint snoop_offset;
static int snoop_dma_chan = -1;

size_t snoopRequested=0;

void initSnoop() {
	snoop_offset = pio_add_program(snoop_pio, &snoop_program);
	pio_sm_config c = snoop_program_get_default_config(snoop_offset);
	for(uint pin=0;pin<23;pin++){
		pio_gpio_init(snoop_pio, pin);
		gpio_set_pulls(pin,false,false);
//		gpio_init(pin);
//		gpio_set_dir(pin,GPIO_IN);
//		gpio_put(pin, 0);
	}
	for(uint pin=26;pin<29;pin++){
		pio_gpio_init(snoop_pio, pin);
		gpio_set_pulls(pin,false,false);
	//		gpio_init(pin);
	//		gpio_set_dir(pin,GPIO_IN);
	}
	pio_sm_config config = snoop_program_get_default_config(snoop_offset);
	// Shift right, autopush disabled (we push manually in PIO)
	sm_config_set_in_pins(&c, 0);
	sm_config_set_in_shift(&c, true, false, 32);   // shift right, no autopush
	sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX); // more RX depth
	pio_sm_init(snoop_pio, snoop_sm, snoop_offset, &c);
	pio_sm_set_enabled(snoop_pio, snoop_sm, true);
	snoop_dma_chan = dma_claim_unused_channel(true);
}

bool beginSnoop(uint32_t* buffer, size_t max_samples) {
	if (snoop_dma_chan < 0) return false;
	dma_channel_config cfg = dma_channel_get_default_config(snoop_dma_chan);
	channel_config_set_read_increment(&cfg, false);           // read from PIO RX
	channel_config_set_write_increment(&cfg, true);
	channel_config_set_dreq(&cfg, pio_get_dreq(snoop_pio, snoop_sm, false));
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
	snoopRequested=max_samples;
	auto fifo=&snoop_pio->rxf[snoop_sm];
	dma_channel_configure(snoop_dma_chan,&cfg,buffer,fifo,max_samples*2,true);
	return true;
}

bool snoopComplete() {
	if (snoop_dma_chan < 0) return true;
	return !dma_channel_is_busy(snoop_dma_chan);
}

size_t getCapturedSampleCount() {
	if (snoop_dma_chan < 0) return 0;
	return snoopRequested-(dma_channel_hw_addr(snoop_dma_chan)->transfer_count/2);
}

void stopBusCapture() {
	if (snoop_dma_chan >= 0) {
		dma_channel_abort(snoop_dma_chan);
	}
}

