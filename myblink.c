
#include <stdint.h>
#include <inttypes.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/flash.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int _write(int file, char *ptr, int len);
/*
 * playing around
 */

void hard_fault_handler(void){
   reset_handler();
}

/*
 * stlinky
 */
#define STLINKY_MAGIC 0xDEADF00D
#define STLINKY_BUFSIZE 256
typedef struct stlinky {
   uint32_t magic;
   uint32_t bufsize;
   uint32_t up_tail;
   uint32_t up_head;
   uint32_t dwn_tail;
   uint32_t dwn_head;
   char upbuff[STLINKY_BUFSIZE];
   char dwnbuff[STLINKY_BUFSIZE];
} __attribute__ ((packed)) stlinky_t;

volatile stlinky_t stl = {
   .magic = STLINKY_MAGIC,
   .bufsize = STLINKY_BUFSIZE,
   .up_tail = 0,
   .up_head = 0,
   .dwn_tail = 0,
   .dwn_head = 0,
};

static void stl_putchar(char c) {
   uint32_t next_head;
   next_head = (stl.up_head +1) % STLINKY_BUFSIZE;
   // wait for buffer space
//   while (next_head == stl.up_tail) ;
   if (next_head ==stl.up_tail)
      return; // no DBG frontend, eat messages....
   stl.upbuff[stl.up_head] = c;
   stl.up_head = next_head;
}

int _write(int file, char *ptr, int len) {
   int i;

   if (file == STDOUT_FILENO || file == STDERR_FILENO) {
      for (i = 0; i < len; i++) {
         stl_putchar(ptr[i]);
      }
      return i;
   }
   errno = EIO;
   return -1;
}


/*
 * delay via systick (1KHz)
 */
volatile uint32_t systick_millis = 0;
volatile uint32_t systick_delay_ctr = 0;

void sys_tick_handler(void) {
   systick_millis++;
   if (systick_delay_ctr)
      systick_delay_ctr--;
}

uint32_t minops = 0xffffffff;

static void delay(uint32_t ms) {
   uint32_t target = systick_millis + ms;
   uint32_t ops = 0;
   while (systick_millis != target) {
      asm("nop");
      ops++;
   }
   if (ops < minops) {
      minops = ops;
   }
}

static void systick_setup(void) {
   systick_set_reload(72000);
   systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
   systick_counter_enable();
   systick_interrupt_enable();
}

// FROM: https://de.wikipedia.org/wiki/Xorshift
uint32_t x = 123456789;
uint32_t y = 362436069;
uint32_t z = 521288629;
uint32_t w = 88675123;
uint32_t xorshift128(void) {

  uint32_t t = x ^ (x << 11);
  x = y; y = z; z = w;
  w ^= (w >> 19) ^ t ^ (t >> 8);

  return w;
}

/* WAVEPATTERNGENERATOR
 *
 * we use timer2 to generate basic timing
 * and a dma channels to transfer data to PA0..PA7
 * also use half-transfer interrupt to calculate (over) next block
 */

#define HALF_BUFFER_SIZE 8192

uint8_t buffer[2][HALF_BUFFER_SIZE]; // two buffers of 8K each

static int8_t calc_buffer_ctr = 0;

static void calc_buffer(int half) {
   uint32_t i, t;
   uint8_t *p = buffer[half];
   for(i = 0; i < HALF_BUFFER_SIZE; i++) {
      t = xorshift128() >> 1;
      t &= (t >> 16);
      t &= (t >> 8);
      *p++ = t & (uint32_t) p;
   }
   // mark refclock if old enough
   if (!calc_buffer_ctr) {
      buffer[half][0] |= 128;
      calc_buffer_ctr = 5;
   } else calc_buffer_ctr--;
}

static void gpio_setup(void) {
   rcc_periph_clock_enable(RCC_GPIOA);
//   rcc_periph_clock_enable(RCC_GPIOB);
   rcc_periph_clock_enable(RCC_GPIOC);

   // PA0..7 -> Pulses via DMA
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO0);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO1);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO2);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO3);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO5);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO6);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO7);

   // PC13 -> LED
   gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

}

static void timer2_setup(void) {
   rcc_periph_clock_enable(RCC_TIM2);

   timer_reset(TIM2);

   timer_disable_preload(TIM2);
   timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,
                  TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

   timer_set_prescaler(TIM2, 35); // 72MHz / 36 -> 2MHz
   timer_continuous_mode(TIM2);
   timer_set_counter(TIM2, 0);
   timer_set_period(TIM2, 1);  // -> 2MHz / 2
   timer_set_dma_on_update_event(TIM2);

   // Update DMA Request enable (no timer IRQ's...)
   timer_enable_irq(TIM2, TIM_DIER_UDE);

   timer_generate_event(TIM2, 1); // set UG bit
}


static void dma_setup(void) {
   rcc_periph_clock_enable(RCC_DMA1);

   // Channel 2 is triggered by timer2 overflow
   dma_channel_reset(DMA1, DMA_CHANNEL2);
   dma_disable_channel(DMA1, DMA_CHANNEL2);
   dma_set_priority(DMA1, DMA_CHANNEL2, DMA_CCR_PL_VERY_HIGH);
   dma_set_memory_size(DMA1, DMA_CHANNEL2, DMA_CCR_MSIZE_8BIT);
   dma_set_peripheral_size(DMA1, DMA_CHANNEL2, DMA_CCR_PSIZE_8BIT);
   dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL2);
   dma_disable_peripheral_increment_mode(DMA1, DMA_CHANNEL2);
   dma_enable_circular_mode(DMA1, DMA_CHANNEL2);
   dma_set_read_from_memory(DMA1, DMA_CHANNEL2);
   dma_set_peripheral_address(DMA1, DMA_CHANNEL2, (uint32_t) & GPIOA_ODR);
   dma_set_memory_address(DMA1, DMA_CHANNEL2, (uint32_t) & buffer);
   dma_set_number_of_data(DMA1, DMA_CHANNEL2, 2*HALF_BUFFER_SIZE);

   dma_enable_half_transfer_interrupt(DMA1, DMA_CHANNEL2);
   dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL2);
   nvic_enable_irq(NVIC_DMA1_CHANNEL2_IRQ);
   // no enable_channel yet!
}

void dma1_channel2_isr(void) {
   if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL2, DMA_HTIF)) {
      // half transfer interrupt -> re-calculate first block
      dma_clear_interrupt_flags(DMA1, DMA_CHANNEL2, DMA_HTIF | DMA_GIF);
      calc_buffer(0);
   } else
   if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL2, DMA_TCIF)) {
      // transfer complete interrupt -> recalculate second block + re-enable dma
      dma_clear_interrupt_flags(DMA1, DMA_CHANNEL2, DMA_TCIF | DMA_GIF);
      dma_disable_channel(DMA1, DMA_CHANNEL2);
      dma_enable_channel(DMA1, DMA_CHANNEL2);
      calc_buffer(1);
   }
}

static void buffer_setup(void) {
   int i, ctr = 0, mask;
   calc_buffer(0);
   calc_buffer(1);
   printf("buffer initialized\n");
   // DBG: output buffer pattern analysis
   for (i=0;i<HALF_BUFFER_SIZE;i++) {
      for(mask=0x40;mask;mask >>= 1) {
         if (buffer[0][i] & mask) ctr++;
         if (buffer[1][i] & mask) ctr++;
      }
   }
   printf("starting with %d pulses/frame\n", ctr*3);

   dma_setup();
   printf("dma ready\n");
   timer2_setup();
   printf("timer ready\n");
   dma_enable_channel(DMA1, DMA_CHANNEL2);
   timer_enable_counter(TIM2);
}


int main(void) {
   rcc_clock_setup_in_hse_8mhz_out_72mhz();
   systick_setup();

   printf("systick ok\n");
   gpio_setup();
   printf("gpio ok\n");
   buffer_setup();
   printf("buffer ok\n");

//   cm_enable_interrupts(); // needs cm3/cortex.h
//   cm_disable_faults();

   while(1) {
      gpio_toggle(GPIOC, GPIO13);
      delay(500);
      printf("free ops: %lu\n", minops);
   }
}

