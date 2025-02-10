#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "inc/ssd1306.h"
#include "ws2812.pio.h"
#include "inc/font.h"
#include <stdio.h>
#include <ctype.h>

#define I2C_PORT         i2c1
#define I2C_SDA_PIN      14
#define I2C_SCL_PIN      15
#define SSD1306_ADDR     0x3C
#define WIDTH            128
#define HEIGHT           64
#define WS2812_PIN       7
//#define WS2812_SM        0    - mudei para a variável sm
//#define WS2812_FREQ      800000  - está sendo tratado no ws2812.pio

// Botões
#define BUTTON_A_PIN     5
#define BUTTON_B_PIN     6

// Pinos do LED RGB
#define LED_R_PIN        13
#define LED_G_PIN        11
#define LED_B_PIN        12

#define DEBOUNCE_DELAY_US 150000  // 150 ms de debounce
uint sm;  // Variável global para a state machine do PIO

// Variáveis globais de estado para os LEDs RGB
volatile bool led_green_state = false;  // volatile: variável que pode ser alterada por uma ISR
volatile bool led_blue_state  = false;

// Variáveis globais para debouncing

//volatile uint64_t last_button_a_time = 0;   
//volatile uint64_t last_button_b_time = 0;

// Instância global do display SSD1306
ssd1306_t ssd;  // ssd1306_t: estrutura de dados para o display SSD1306

// Número de LEDs da matriz WS2812 (5x5)
#define MATRIX_LED_COUNT 25
uint32_t led_matrix[MATRIX_LED_COUNT] = {0};    // Matriz de LEDs para a matriz WS2812

// Matrizes dos dígitos para o WS2812 (cada dígito é uma matriz 5x5 – 25 elementos)
const uint8_t digits[10][25] = {
    {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, 	// 0
    {0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0}, 	// 1
    {1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0}, 	// 2
    {1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0},	// 3
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0},  	// 4
    {1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0}, 	// 5
    {1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0}, 	// 6
    {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0}, 	// 7
    {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, 	// 8
    {1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0}  	// 9
};


//--------------------------------------------------------------------------------------------
// Função para atualizar a matriz WS2812 com os dados recebidos
void ws2812_update(PIO pio, uint sm, const uint32_t *led_colors, uint count) {
    for (uint i = 0; i < count; i++) {
        pio_sm_put_blocking(pio, sm, led_colors[i] << 8u);
    }
}

//--------------------------------------------------------------------------------------------
// Função para atualizar a matriz WS2812 com o dígito recebido
void display_digit_on_matrix(uint8_t digit) {
    if (digit > 9) return;  // Retorna se o dígito for maior que 9
    // Define a cor para LED ligado e desligado (por exemplo, ligado = branco)
    uint32_t color_on = 0x00001F00;  // Red
    uint32_t color_off = 0x00000000; // Desligado
    for (int i = 0; i < MATRIX_LED_COUNT; i++) {    // Atualiza a matriz WS2812 com o dígito recebido
        led_matrix[i] = (digits[digit][i]) ? color_on : color_off;  // Se o dígito for 1, liga o LED, senão, desliga
    }
    // Envia os dados para a matriz via PIO
    ws2812_update(pio0, sm, led_matrix, MATRIX_LED_COUNT);   // Atualiza a matriz WS2812 com a matriz led_matrix
}
//--------------------------------------------------------------------------------------------

void button_callback(uint gpio, uint32_t events) {
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);  // Desativa a interrupção temporariamente
    //Debouncing é computado verificando as diferenças de tempo entre as interrupções, com o auxílio da função time_us_64()
    //Caso a diferença seja maior que o tempo de debounce, a interrupção é considerada válida
    static uint64_t last_interrupt_time = 0;
    uint64_t current_time = time_us_64();

    if ((current_time - last_interrupt_time) > (DEBOUNCE_DELAY_US)) {   // Verifica se o tempo de debounce foi atingido
        //testa qual botão foi pressionado e incrementa ou decrementa o dígito atual
        if (gpio == BUTTON_A_PIN) {
            //atua no led G RGB e manda mensagem para o display SSD1306 e serial
            led_green_state = !led_green_state; // Alterna o estado do LED verde
            gpio_put(LED_G_PIN, led_green_state);   // Atualiza o estado do LED verde
            char message[50];
            // snprintf: formata a string de saída e armazena em um buffer - message: buffer de saída, sizeof(message): tamanho do buffer,
            //"Button A: LED Green %s": string de formatação, led_green_state ? "ON" : "OFF": valor a ser formatado
            snprintf(message, sizeof(message), "ButA:Verde %s",led_green_state ?"ON" :"OFF");
            printf("%s\n", message);    // Imprime a mensagem no terminal serial
            ssd1306_fill(&ssd, false);       // Preenche o display SSD1306 com a cor preta
            ssd1306_send_data(&ssd);         // Envia os dados para o display SSD1306
            ssd1306_draw_string(&ssd, message, 0, 0);   // Desenha a mensagem no display SSD1306
            ssd1306_send_data(&ssd);            // Envia a mensagem para o display SSD1306
        } else if (gpio == BUTTON_B_PIN) {
            //atua no led B RGB e manda mensagem para o display SSD1306 e serial 
            led_blue_state = !led_blue_state;
            gpio_put(LED_B_PIN, led_blue_state);
            char message[50];
            // snprintf: formata a string de saída e armazena em um buffer - message: buffer de saída, sizeof(message): tamanho do buffer, 
            //"Button B: LED Blue %s": string de formatação, led_blue_state ? "ON" : "OFF": valor a ser formatado
            //Antes, limpar display ssd1306
            ssd1306_fill(&ssd, false);       // Preenche o display SSD1306 com a cor preta
            ssd1306_send_data(&ssd);         // Envia os dados para o display SSD1306
            snprintf(message, sizeof(message), "ButB:Azul %s", led_blue_state ?"ON" :"OFF");
            printf("%s\n", message);    // Imprime a mensagem no terminal serial
            ssd1306_draw_string(&ssd, message, 0, 16);  // Desenha a mensagem no display SSD1306
            ssd1306_send_data(&ssd);                // Envia a mensagen para o display SSD1306
        }
        
        last_interrupt_time = current_time;
    }
     gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, true);  // Reativa a interrupção
    
}


//--------------------------------------------------------------------------------------------
int main() {
    PIO pio = pio0;
    stdio_init_all();
    sleep_ms(2000);  // Aguarda a conexão USB/serial
    printf("Pico-W: SSD1306 e WS2812\n");

    // Inicializa I2C no i2c1 (SDA: GPIO14, SCL: GPIO15) para o display SSD1306
    // Usa a frequência padrão de 400 kHz
    i2c_init(I2C_PORT, 400000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);  //muda a função do pino para I2C
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);  //habilita o pull-up para os pinos SDA e SCL 
    gpio_pull_up(I2C_SCL_PIN);

    // Inicializa o display SSD1306
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, SSD1306_ADDR, I2C_PORT);   // Inicializa o display SSD1306 com largura, altura, modo de página, endereço e porta I2C
    ssd1306_config(&ssd);                                            // Configura o display SSD1306
    ssd1306_fill(&ssd, false);                                     // Preenche o display SSD1306 com a cor preta
    ssd1306_send_data(&ssd);                                  // Envia os dados para o display SSD1306
    ssd1306_draw_string(&ssd, "Hello   World", 8, 10); // Desenha uma string
    ssd1306_send_data(&ssd); // Atualiza o display

    // Inicializa o WS2812 via PIO (usando ws2812_init do módulo ws2812)
    uint offset = pio_add_program(pio, &ws2812_program);	// Adiciona o programa ws2812 ao pio0 na posição de memória offset
    sm = pio_claim_unused_sm(pio, true);				// Reivindica um state machine livre no pio0
    ws2812_program_init(pio, sm, offset,WS2812_PIN);        // Inicializa o programa ws2812 no pio0, state machine sm, offset e pino WS2812_PIN
                                                            // a frequência é definida no programa ws2812.pio

    // Configura os pinos do LED RGB
    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_put(LED_R_PIN, false);
    gpio_init(LED_G_PIN);
    gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_put(LED_G_PIN, led_green_state);
    gpio_init(LED_B_PIN);
    gpio_set_dir(LED_B_PIN, GPIO_OUT);
    gpio_put(LED_B_PIN, led_blue_state);

    // Configura os botões com pull-up e interrupção (borda de descida)
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback); // Habilita a interrupção para o botão A

    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    gpio_set_irq_enabled(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true); // Habilita a interrupção para o botão B

    // Variáveis para controle da posição do texto no SSD1306
    uint8_t text_x = 0;
    uint8_t text_y = 32;  // Começa na linha 2 (pode ajustar)

    while (true) {
        // Recebe caracteres do terminal (timeout de 10ms)
        int ch = getchar_timeout_us(10000);
        if (ch != PICO_ERROR_TIMEOUT) {
            char c = (char)ch;
            printf("Recebido: %c\n", c);
            
            // Desenha o caractere no SSD1306
            ssd1306_draw_char(&ssd, c, text_x, text_y);
            ssd1306_send_data(&ssd);
            text_x += 8;
            if (text_x + 8 > ssd.width) {
                text_x = 0;
                text_y += 8;
                if (text_y + 8 > ssd.height) {
                    text_y = 0;
                    ssd1306_fill(&ssd, false);
                }
            }
            
            // Se for dígito, atualiza a matriz WS2812
            if (c >= '0' && c <= '9') {
                display_digit_on_matrix(c - '0');
            }
        }
    }
    return 0;
}
