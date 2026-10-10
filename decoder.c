#ifndef F_CPU
#define F_CPU 16000000UL // 16 MHz clock speed for delay functions
#endif

#include <stdint.h>
#include <util/delay.h>
#include <string.h>

// --- Baremetal ATmega328P Definitions ---

// Status Register and Interrupt Control
#define SREG             (*(volatile uint8_t *)0x5F)
#define sei()            __asm__ __volatile__ ("sei" ::: "memory")
#define cli()            __asm__ __volatile__ ("cli" ::: "memory")

// ISR macro definition
#define ISR(vector) \
    void vector (void) __attribute__ ((signal, used, externally_visible)); \
    void vector (void)

// Interrupt Vectors
#define INT0_vect          __vector_1
#define TIMER1_COMPA_vect  __vector_11

// Port D Registers
#define PIND             (*(volatile uint8_t *)0x29)
#define DDRD             (*(volatile uint8_t *)0x2A)
#define PORTD            (*(volatile uint8_t *)0x2B)

#define PD2              2
#define PD3              3

// External Interrupt Registers
#define EICRA            (*(volatile uint8_t *)0x69)
#define EIMSK            (*(volatile uint8_t *)0x3D)

#define ISC00            0
#define ISC01            1
#define INT0             0

// Timer 1 Registers
#define TCCR1A           (*(volatile uint8_t *)0x80)
#define TCCR1B           (*(volatile uint8_t *)0x81)
#define OCR1A            (*(volatile uint16_t *)0x88)
#define TIMSK1           (*(volatile uint8_t *)0x6F)

#define WGM12            3
#define CS11             1
#define CS10             0
#define OCIE1A           1

// --- Application Logic ---

volatile uint32_t ms = 0;
volatile uint32_t press_start = 0;
volatile uint16_t press_duration = 0;
volatile uint8_t button_event = 0;

char morse[8];
uint8_t morse_idx = 0;

uint32_t last_symbol = 0;
uint8_t word_sent = 0;

// timer interrupt
ISR(TIMER1_COMPA_vect)
{
  ms++;
}

// external interrupt
ISR(INT0_vect)
{
  static uint32_t last_irq = 0;

  // debouncing
  if ((ms - last_irq) < 30)
    return;

  last_irq = ms;

  if (!(PIND & (1 << PD2)))
  {
    press_start = ms;
  }
  else
  {
    press_duration = ms - press_start;
    button_event = 1;
  }
}

// timer initialization
void timer_init()
{
  TCCR1A = 0;

  TCCR1B = (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);

  OCR1A = 249;

  TIMSK1 |= (1 << OCIE1A);
}

// gpio initialization
void gpio_init()
{
  DDRD &= ~(1 << PD2);
  PORTD |= (1 << PD2);

  DDRD |= (1 << PD3);
  PORTD |= (1 << PD3);
}

// external interrupt initialization
void int0_init()
{
  EICRA |= (1 << ISC00);
  EICRA &= ~(1 << ISC01);

  EIMSK |= (1 << INT0);
}

//Software UART
void soft_uart_tx(char c)
{
  PORTD &= ~(1 << PD3);
  _delay_us(104);

  for (uint8_t i = 0; i < 8; i++)
  {
    if (c & (1 << i))
      PORTD |= (1 << PD3);
    else
      PORTD &= ~(1 << PD3);

    _delay_us(104);
  }

  PORTD |= (1 << PD3);
  _delay_us(104);
}

// morse decoder
char decode_morse(char *s)
{
  if (!strcmp(s, ".")) return 'E';
  if (!strcmp(s, "-")) return 'T';
  if (!strcmp(s, "...")) return 'S';
  if (!strcmp(s, "---")) return 'O';
  if (!strcmp(s, "....")) return 'H';

  return '?';
}

//setup function
void setup()
{
  cli();

  gpio_init();
  timer_init();
  int0_init();

  sei();
}

// main loop
void loop()
{
  if (button_event)
  {
    button_event = 0;

    morse[morse_idx++] =
      (press_duration < 300) ? '.' : '-';

    last_symbol = ms;
    word_sent = 0;
  }

  // letter gap detection
  if (morse_idx > 0 && (ms - last_symbol) > 1000)
  {
    morse[morse_idx] = '\0';

    char c = decode_morse(morse);

    soft_uart_tx(c);

    morse_idx = 0;
  }

  // word gap detection
  if (!word_sent &&
      morse_idx == 0 &&
      (ms - last_symbol) > 2000)
  {
    soft_uart_tx(' ');
    word_sent = 1;
  }
}
