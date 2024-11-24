
#include "stm32f0xx.h"
#include <stdint.h>
#include <stdio.h>

int msg_index = 0;
uint16_t msg[8] = {0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700};
extern const char font[];
extern char keymap;
char *keymap_arr = &keymap;
extern uint8_t col;
uint8_t col = 0;
char falling_key;
const char arrow_chars[4] = {'5', '0', '7', '9'};
int checker = 0;

void internal_clock();

//look buddy

#define STEP6
#define SHELL

void init_usart5() {
    // TODO
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    RCC->AHBENR |= RCC_AHBENR_GPIODEN;

    RCC->APB1ENR |= RCC_APB1ENR_USART5EN;

    GPIOC->MODER &= ~(0x3000000); //configure pin PC12 to be routed to USART5_TX
    GPIOC->MODER |= 0x2000000;
    GPIOC->AFR[1] |= 0x20000;

    GPIOD->MODER &= ~(0xF0); //configure pin PD2 to be routed to USART5_RX
    GPIOD->MODER |= 0x20;
    GPIOD->AFR[0] |= 0x200;

    //Configure USART5:
    USART5->CR1 &= ~USART_CR1_UE;

    USART5->CR1 &= ~USART_CR1_M0;
    USART5->CR1 &= ~USART_CR1_M1;

    USART5->CR2 &= ~USART_CR2_STOP_0;
    USART5->CR2 &= ~USART_CR2_STOP_1;

    USART5->CR1 &= ~USART_CR1_PCE;
    USART5->CR1 &= ~USART_CR1_OVER8;

    USART5->BRR = 0x1A1;

    USART5->CR1 |= USART_CR1_TE;
    USART5->CR1 |= USART_CR1_RE;
    USART5->CR1 |= USART_CR1_UE;

    while(!(USART5->ISR & USART_ISR_TEACK) | !(USART5->ISR & USART_ISR_REACK));

}



#ifdef SHELL
#include "commands.h"
#include <stdio.h>

#include "fifo.h"
#include "tty.h"
#include "lcd.h"

int incL = 0;
int randomIndex = 0; 
int randomIndexR = 0;
int incR = -1;
int initial = -1;
int gameEnd = 0;

// TODO DMA data structures
#define FIFOSIZE 16
char serfifo[FIFOSIZE];
int seroffset = 0;

void USART3_8_IRQHandler(void) {
    while(DMA2_Channel2->CNDTR != sizeof serfifo - seroffset) {
        if (!fifo_full(&input_fifo))
            insert_echo_char(serfifo[seroffset]);
        seroffset = (seroffset + 1) % sizeof serfifo;
    }
}

void enable_tty_interrupt(void) {
    // TODO
    USART5->CR1 |= USART_CR1_RXNEIE;

    NVIC_EnableIRQ(USART3_8_IRQn);
    USART5->CR3 |= USART_CR3_DMAR;

    RCC->AHBENR |= RCC_AHBENR_DMA2EN;
    DMA2->CSELR |= DMA2_CSELR_CH2_USART5_RX;
    DMA2_Channel2->CCR &= ~DMA_CCR_EN;  // First make sure DMA is turned off

    // The DMA channel 2 configuration goes here
    DMA2_Channel2->CMAR = &serfifo;
    DMA2_Channel2->CPAR = &(USART5->RDR);
    DMA2_Channel2->CNDTR |= FIFOSIZE;
    DMA2_Channel2->CCR &= ~(DMA_CCR_DIR | DMA_CCR_HTIE | DMA_CCR_TCIE);
    DMA2_Channel2->CCR &= ~(DMA_CCR_MSIZE_1|DMA_CCR_MSIZE_0|DMA_CCR_PSIZE_1|DMA_CCR_PSIZE_0);
    DMA2_Channel2->CCR |= DMA_CCR_MINC;
    DMA2_Channel2->CCR &= ~DMA_CCR_PINC;
    DMA2_Channel2->CCR |= DMA_CCR_CIRC;
    DMA2_Channel2->CCR &= ~DMA_CCR_MEM2MEM;
    DMA2_Channel2->CCR |= DMA_CCR_PL_1 | DMA_CCR_PL_0;
    DMA2_Channel2->CCR |= DMA_CCR_EN;
}

// Works like line_buffer_getchar(), but does not check or clear ORE nor wait on new characters in USART
char interrupt_getchar() {
    // TODO
    while(fifo_newline(&input_fifo) == 0) {
        asm volatile ("wfi"); // wait for an interrupt
    }
    return fifo_remove(&input_fifo);
}

int __io_putchar(int c) {
    // TODO copy from STEP2
    if(c == '\n'){
        while(!(USART5->ISR & USART_ISR_TXE));
        USART5->TDR = '\r';
    }
    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    return c;
}

int __io_getchar(void) {
    // TODO Use interrupt_getchar() instead of line_buffer_getchar()
    return interrupt_getchar();
}

void my_command_shell(void)
{
  char line[100];
  int len = strlen(line);
  puts("This is the STM32 command shell.");
  for(;;) {
      printf("> ");
      fgets(line, 99, stdin);
      line[99] = '\0';
      len = strlen(line);
      if ((line[len-1]) == '\n')
          line[len-1] = '\0';
      parse_command(line);
  }
}

void init_spi1_slow ()
{
    //PB3 (SCK), PB4 (MISO), and PB5 (MOSI)
    //AFR 0

    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    GPIOB->MODER &= ~0xFC0;
    GPIOB->MODER |= 0xA80;
    GPIOB->AFR[0] &= ~(GPIO_AFRL_AFRL3 | GPIO_AFRL_AFRL4 | GPIO_AFRL_AFRL5);

    SPI1->CR1 &= ~SPI_CR1_SPE;

    SPI1->CR1 |= (SPI_CR1_BR | SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI);
    SPI1->CR2 |= 0x700;
    SPI1->CR2 &= ~SPI_CR2_DS_3;
    SPI1->CR2 |= SPI_CR2_FRXTH;

    SPI1->CR1 |= SPI_CR1_SPE;

}

void enable_sdcard()
{
    GPIOB->BRR |= GPIO_BRR_BR_2;
}

void disable_sdcard()
{
    GPIOB->BSRR |= GPIO_BSRR_BS_2;
}

void init_sdcard_io()
{
    init_spi1_slow();
    GPIOB->MODER |= 0x10;
    disable_sdcard();

}

void sdcard_io_high_speed()
{
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 |= 0x8;
    SPI1->CR1 &= ~(SPI_CR1_BR_2|SPI_CR1_BR_1);
    SPI1->CR1 |= SPI_CR1_SPE;
}

void init_lcd_spi()
{
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    GPIOB->MODER &= ~(0x30C30000);
    GPIOB->MODER |= (0x10410000);

    init_spi1_slow();
    sdcard_io_high_speed();

}


void initialLCD()
{
    LCD_DrawFillRectangle(0,0,320,320,WHITE);
    LCD_DrawLine(120,0,120,320,BLACK);

}

// screen is 240 x 320

void drawUP(int inc, int lr, int bw) // lr = left (0) or right (1) side arrow, inc = increment down by inc, bw = black or white
{
    if (lr == 0){ // left
        LCD_DrawLine(180, 320 - inc, 180, 270 - inc, (bw == 0) ? BLACK : WHITE);
        LCD_DrawLine(180, 320 - inc, 155, 295 - inc, (bw == 0) ? BLACK : WHITE); // up arrow
        LCD_DrawLine(180, 320 - inc, 205, 295 - inc, (bw == 0) ? BLACK : WHITE);
    } else { // right
        LCD_DrawLine(60, 320 - inc, 60, 270 - inc, (bw == 0) ? BLACK : WHITE);
        LCD_DrawLine(60, 320 - inc, 35, 295 - inc, (bw == 0) ? BLACK : WHITE); // up arrow
        LCD_DrawLine(60, 320 - inc, 85, 295 - inc, (bw == 0) ? BLACK : WHITE);
    }
}

void drawDOWN(int inc, int lr, int bw) // lr = left (0) or right (1) side arrow, inc = increment down by inc, bw = black or white
{
    if (lr == 0) {
        LCD_DrawLine(180, 320 - inc, 180, 270 - inc, (bw == 0) ? BLACK : WHITE); // line
        LCD_DrawLine(180, 270 - inc, 155, 295 - inc, (bw == 0) ? BLACK : WHITE); // down arrow
        LCD_DrawLine(180, 270 - inc, 205, 295 - inc, (bw == 0) ? BLACK : WHITE);
    } else {
        LCD_DrawLine(60, 320 - inc, 60, 270 - inc, (bw == 0) ? BLACK : WHITE); // line
        LCD_DrawLine(60, 270 - inc, 35, 295 - inc, (bw == 0) ? BLACK : WHITE); // down arrow
        LCD_DrawLine(60, 270 - inc, 85, 295 - inc, (bw == 0) ? BLACK : WHITE);
    }
}

void drawLEFT(int inc, int lr, int bw) // lr = left (0) or right (1) side arrow, inc = increment down by inc, bw = black or white
{
    if (lr == 0) {
        LCD_DrawLine(155, 295 - inc, 205, 295 - inc, (bw == 0) ? BLACK : WHITE);
        LCD_DrawLine(180, 320 - inc, 205, 295 - inc, (bw == 0) ? BLACK : WHITE); // left arrow
        LCD_DrawLine(205, 295 - inc, 180, 270 - inc, (bw == 0) ? BLACK : WHITE);
    } else {
        LCD_DrawLine(35, 295 - inc, 85, 295 - inc, (bw == 0) ? BLACK : WHITE);
        LCD_DrawLine(60, 320 - inc, 85, 295 - inc, (bw == 0) ? BLACK : WHITE); // left arrow
        LCD_DrawLine(85, 295 - inc, 60, 270 - inc, (bw == 0) ? BLACK : WHITE);
    }
}

void drawRIGHT(int inc, int lr, int bw) // lr = left (0) or right (1) side arrow, inc = increment down by inc, bw = black or white
{
    if (lr == 0) {
        LCD_DrawLine(155, 295 - inc, 205, 295 - inc, (bw == 0) ? BLACK : WHITE);
        LCD_DrawLine(155, 295 - inc, 180, 320 - inc, (bw == 0) ? BLACK : WHITE); // right arrow
        LCD_DrawLine(155, 295 - inc, 180, 270 - inc, (bw == 0) ? BLACK : WHITE);
    } else {
        LCD_DrawLine(35, 295 - inc, 85, 295 - inc, (bw == 0) ? BLACK : WHITE);
        LCD_DrawLine(35, 295 - inc, 60, 320 - inc, (bw == 0) ? BLACK : WHITE); // right arrow
        LCD_DrawLine(35, 295 - inc, 60, 270 - inc, (bw == 0) ? BLACK : WHITE);
    }
}

void TIM7_IRQHandler()
{
    TIM7 -> SR &= ~TIM_SR_UIF;

    if(gameEnd == 1) return;

    void (*arrowFunctions[])(int, int, int) = {drawUP, drawDOWN, drawLEFT, drawRIGHT};

    (*arrowFunctions[randomIndex])(incL,0, 1);
    (*arrowFunctions[randomIndexR])(incR,1, 1);

    incL++;
    incR++;
    checker = 0;

    if((incL - 1) == 0)
    {
        randomIndex = rand() % 4;
        (*arrowFunctions[randomIndex])(incL,0, 0);
    }
    else if(incL == 271)
    {
        incL = 0;
        falling_key = 'A';

    }
    else
    {
        (*arrowFunctions[randomIndex])(incL, 0, 0);
    }

    if(incL == 150 && initial == -1)
    {
        initial = 0;
        incR = 1;
    }

    if(initial == 0)
    {
    if((incR - 1) == 0)
        {
            randomIndexR = rand() % 4;
            (*arrowFunctions[randomIndexR])(incR, 1, 0);
        }
    else if(incR == 271)
        {
            incR = 0;
            falling_key = 'A';
        }
    else
        {
            (*arrowFunctions[randomIndexR])(incR, 1, 0);
        }
    }
    // hi

    if(incR <= 280 && incR >= 230) 
    {
        falling_key = arrow_chars[randomIndexR];
    }
    if(incL <= 280 && incL >= 230) 
    {
        falling_key = arrow_chars[randomIndex];
    }




}

void setup_tim7()
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    TIM7 -> PSC = 1000000 - 1;
    TIM7 -> ARR = 55 - 1;
    TIM7 -> DIER |= TIM_DIER_UIE;
    NVIC -> ISER[0] = 1 << TIM7_IRQn;
    TIM7 -> CR1 |= TIM_CR1_CEN;
}

void heart()
{
    LCD_Clear(WHITE);
        // Triangle 1: Left upper lobe
    LCD_DrawFillTriangle(100, 180, 80, 150, 120, 150, RED);

    // Triangle 2: Right upper lobe
    LCD_DrawFillTriangle(140, 180, 120, 150, 160, 150, RED);

    // Triangle 3: Bottom point
    LCD_DrawFillTriangle(80, 150, 160, 150, 120, 100, RED);

}

void printPress()
{
    
    LCD_DrawLine(160,90,160,70,BLACK);
    LCD_DrawRectangle(160,90,150,80,BLACK);  //p

    LCD_DrawLine(145,90,145,70,BLACK);
    LCD_DrawRectangle(145,90,135,80,BLACK); //r
    LCD_DrawLine(145,80,135,70,BLACK);

    LCD_DrawLine(130,90,130,70,BLACK);
    LCD_DrawLine(130,90,120,90,BLACK); //e
    LCD_DrawLine(130,80,120,80,BLACK);
    LCD_DrawLine(130,70,120,70,BLACK);

    LCD_DrawLine(115,90,115,80,BLACK);
    LCD_DrawLine(115,90,105,90,BLACK); //s
    LCD_DrawLine(115,80,105,80,BLACK);
    LCD_DrawLine(115,70,105,70,BLACK);
    LCD_DrawLine(105,80,105,70,BLACK);

    LCD_DrawLine(115-15,90,115-15,80,BLACK);
    LCD_DrawLine(115-15,90,105-15,90,BLACK); //s
    LCD_DrawLine(115-15,80,105-15,80,BLACK);
    LCD_DrawLine(115-15,70,105-15,70,BLACK);
    LCD_DrawLine(105-15,80,105-15,70,BLACK);

    LCD_DrawLine(120,65,120,45, BLACK);
    LCD_DrawLine(120,65,125,55, BLACK); //1
    LCD_DrawLine(125,45,115,45, BLACK);



}

#endif 

#ifdef STEP6

// int __io_putchar(int c)
// {
//     // TODO
//     if (c == '\n')
//     {
//         while (!(USART5->ISR & USART_ISR_TXE))
//             ;
//         USART5->TDR = '\r';
//     }
//     while (!(USART5->ISR & USART_ISR_TXE))
//         ;
//     USART5->TDR = c;
//     return c;
// }

// int __io_getchar(void)
// {
//     while (!(USART5->ISR & USART_ISR_RXNE))
//         ;
//     char c = USART5->RDR;
//     // TODO
//     if (c == '\r')
//     {
//         c = '\n';
//     }
//     __io_putchar(c);
//     return c;
// }

void init_spi2(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~(GPIO_MODER_MODER15_0 | GPIO_MODER_MODER12_0 | GPIO_MODER_MODER13_0);
    GPIOB->MODER |= GPIO_MODER_MODER15_1 | GPIO_MODER_MODER12_1 | GPIO_MODER_MODER13_1;
    GPIOB->AFR[1] &= ~0xf0ff0000;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CR1 |= SPI_CR1_BR;
    SPI2->CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0;
    SPI2->CR1 |= SPI_CR1_MSTR;
    SPI2->CR2 |= SPI_CR2_SSOE;
    SPI2->CR2 |= SPI_CR2_NSSP;
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    SPI2->CR1 |= SPI_CR1_SPE;
}

void spi2_setup_dma(void)
{
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel5->CMAR = (uint32_t)(&msg);
    DMA1_Channel5->CPAR = (uint32_t)(&(SPI2->DR));
    DMA1_Channel5->CNDTR = 8;
    DMA1_Channel5->CCR |= DMA_CCR_DIR;
    DMA1_Channel5->CCR |= DMA_CCR_MINC;
    DMA1_Channel5->CCR &= ~DMA_CCR_MSIZE_1;
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0;
    DMA1_Channel5->CCR &= ~DMA_CCR_PSIZE_1;
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0;
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;
}

void spi2_enable_dma(void)
{
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

void enable_ports()
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~(GPIO_MODER_MODER4 | GPIO_MODER_MODER5 | GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOC->MODER |= (GPIO_MODER_MODER4_0 | GPIO_MODER_MODER5_0 | GPIO_MODER_MODER6_0 | GPIO_MODER_MODER7_0);

    // inputs for port C
    GPIOC->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1 | GPIO_MODER_MODER2 | GPIO_MODER_MODER3);

    // pull down for port C
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR0_1;
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPDR0_0;
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR1_1;
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPDR1_0;
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR2_1;
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPDR2_0;
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR3_1;
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPDR3_0;
}

void enable_led_ports()
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    GPIOA->MODER &= ~(0x30000); //red, PA8
    GPIOA->MODER |= 0x30000;

    GPIOA->MODER &= ~(0xC0000); //green, PA9
    GPIOA->MODER |= 0xC0000;
}

void turnOffLed(GPIO_TypeDef *port, int n) {
    int32_t pin = 1 << n;
    int32_t check_val = port->ODR & pin; // we use and to find matching ones in the port->IDR and the pin value
    if (check_val != 0)
    {
        port->BRR = pin;
    }
}

void turnOnLed(GPIO_TypeDef *port, int n) {
    int32_t pin = 1 << n;
    int32_t check_val = port->ODR & pin; // we use and to find matching ones in the port->IDR and the pin value
    if (check_val = 0)
    {
        port->BSRR = pin;
    }
}

int c = 0;
void drive_column(int c)
{
    GPIOC->BSRR = (1 << (7 + 16) | 1 << (6 + 16) | 1 << (5 + 16) | 1 << (4 + 16));
    GPIOC->BSRR = 1 << ((0b11 & c) + 4);
}

int read_rows()
{
    return GPIOC->IDR &= (1 << 0 | 1 << 1 | 1 << 2 | 1 << 3);
}

char rows_to_key(int rows, int col)
{
    if (rows & 1)
    {
        rows = 0;
    }
    else if (rows & 2)
    {
        rows = 1;
    }
    else if (rows & 4)
    {
        rows = 2;
    }
    else if (rows & 8)
    {
        rows = 3;
    }
    int col_val = col * 4 + rows;
    return keymap_arr[col_val];
}

char handle_input()
{
    for (int col = 0; col < 4; col++)
    {
        drive_column(col);
        int rows = read_rows();
        if (rows)
        {
            char pressed_key = rows_to_key(rows, col);
            return pressed_key;
        }
    }
    return '\0';
}

int score = 0;
void update_score(char falling_key)
{
    char user_input = handle_input();
    static int key_pressed = 0;
    char one = '1';


    if (user_input != 0 && key_pressed == 0)
    {
        key_pressed = 1;
        // Check if the user input matches the falling key
        if (user_input == one)
        {
            gameEnd = 0;
            LCD_Clear(WHITE);
            initialLCD();
            setup_tim7();
        }
        else if(user_input == '2')
        {
            gameEnd = 1;
            LCD_Clear(WHITE);
            heart();
        }
        else{
        if (user_input == falling_key)
        {
            // turnOffLed(GPIOA, 8); // red off
            score++; // Correct key press, increment score
            // turnOnLed(GPIOA, 9); // green on
        }
        else
        {
            // turnOffLed(GPIOA, 9); // green off
            score--; // Incorrect key press, decrement score
            // turnOnLed(GPIOA, 8); // red on
        }

        if (score < 0)
        {
            score = 0;
        }

        if (score > 999)
        {
            score = 999;
        }
        }


        //Calculate the digits for score
        char updated_char_5 = font['0' + (score / 100) % 10];
        // char updated_char_5 = font[user_input];

        char updated_char_6 = font['0' + (score / 10) % 10];
        char updated_char_7 = font['0' + score % 10];

        msg[5] &= ~0xFF;
        msg[6] &= ~0xFF;
        msg[7] &= ~0xFF;
        msg[5] |= updated_char_5;
        msg[6] |= updated_char_6;
        msg[7] |= updated_char_7;
    }

    // else if(checker == 1 && user_input == '\0')
    // {
    //     checker = 0;
    //     score--;

    //     if(score < 0) score = 0;

    //     char updated_char_5 = font['0' + (score / 100) % 10];
    //     char updated_char_6 = font['0' + (score / 10) % 10];
    //     char updated_char_7 = font['0' + score % 10];

    //     msg[5] &= ~0xFF;
    //     msg[6] &= ~0xFF;
    //     msg[7] &= ~0xFF;
    //     msg[5] |= updated_char_5;
    //     msg[6] |= updated_char_6;
    //     msg[7] |= updated_char_7;
    // }

    if (user_input == 0) 
    {
        key_pressed = 0;
    }
}

void TIM14_IRQHandler()
{
    TIM14->SR &= ~TIM_SR_UIF;
    update_score(falling_key);
}

void setup_tim14()
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
    // set prescaler and arr
    TIM14->PSC = 1000000 - 1;
    TIM14->ARR = 24 - 1;
    TIM14->DIER |= TIM_DIER_UIE;
    NVIC->ISER[0] = 1 << TIM14_IRQn;
    TIM14->CR1 |= TIM_CR1_CEN;
}

void togglexn(GPIO_TypeDef *port, int n)
{
    int32_t pin = 1 << n;
    int32_t check_val = port->ODR & pin; // we use and to find matching ones in the port->IDR and the pin value
    if (check_val != 0)
    {
        port->BRR = pin;
    }
    else
    {
        port->BSRR = pin;
    }
}

#endif

int main()
{
    internal_clock();

    msg[0] |= font['S'];
    msg[1] |= font['C'];
    msg[2] |= font['O'];
    msg[3] |= font['R'];
    msg[4] |= font['E'];
    msg[5] |= font['0'];
    msg[6] |= font['0'];
    msg[7] |= font['0'];

  *init_usart5();
    enable_tty_interrupt();
    setbuf(stdin,0);
    setbuf(stdout,0);
    setbuf(stderr,0);
    // command_shell();
    LCD_Setup();
    srand(time(0));

    initialLCD();
    setup_tim7();

    // workerLCD();



    LCD_Setup();
    heart();
    printPress();

    srand(time(0));


    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    enable_ports();
    setup_tim14();
    enable_led_ports();

    togglexn(GPIOA, 9);


    return 0;
}
