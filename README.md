# oled_ws2812

## Tarefa da Aula Síncrona de 03/02/2025 - Combinando ws2812, SSD1306, botões, interrupção, debouncing, i2c, uart-usb e tabela de letras minúsculas

Aqui é mostrado um resumo do funcionamento do programa produzido. Os comentários ao longo de todo o programa são mais esclarecedores.

**No corpo da main()**

Inicializa I2C no i2c1
Inicializa o display SSD1306
Inicializa o WS2812 via PIO
Configura os pinos do LED RGB
Configura os botões com pull-up e interrupção (borda de descida)
Looping infinito - trata da transmissão dos caracteres entre o monitor serial e o display SSD1306 / WS2812 (no caso de dígitos)

**Na interrupção dos botões**

Uma mesma callback trata ambos os botões, determinando aquele que foi pressionado e executando as funções pertinentes.

**O que ocorre na interrupção?**

1) Desativa temporariamente as interrupções para o botão envolvido
2) Realiza o debouncing com o auxilio da função time_us_64() - a interrupção só é atendida se o intervalo entre a última interrupção e a atual for maior que 150ms - isto é, descarta as interrupções causadas por ruído de bouncing.
3) Determina o botão responsável pela interrupção e executa as seguintes funções:
            4)  Alterna o estado do LED RGB e o atualiza no componente físico (on/off)
            5)  Prepara a mensagem que combina o nome do botão, a cor do led e o estado
            6)  Envia essa mensagem para a interface uart-usb e para o display SSD1306
7) Reativa as interrupções.

**Funcionamento do PIO/ws2812**

Esta parte já foi descrita em tarefas anteriores, manteve-se a mesma ideia. Para maiores esclarecimentos, vale ler os comentários ao longo do programa.

**Comunicação UART-USB e I2C (SSD1306)**

Foi necessário complementar a tabela de carecteres, incluindo letras minúsculas. A construção dessas fontes foram baseadas no code page 437 do IBM-PC disponível em https://github.com/susam/pcface.
A "conversão" de caracteres ASCII do teclado para caracteres do SSD1306 é implementada a partir de uma modificação da função  *void ssd1306_draw_char(ssd1306_t *ssd, char c, uint8_t x, uint8_t y)* contida no programa ssd1306.c disponibilizado pelo Prof. Wilton.

## Vídeos Demonstrativos

Foram produzidos dois vídeos. Por quê?
Os caracteres mostrados no display SSD1306 são muito pequenos, difíceis de serem enxergados em um vídeo. Portanto, foi necessário criar um vídeo com a simulação no wokwi, permitindo uma melhor visualização.


[![Assista no YouTube](https://img.youtube.com/vi/wvjZRoPPudQ/maxresdefault.jpg)](https://youtu.be/wvjZRoPPudQ)



