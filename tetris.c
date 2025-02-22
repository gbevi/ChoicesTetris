#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "ssd1306/ssd1306.h"
#include "ws2812b.pio.h"

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Botões
#define BTN_LEFT 5
#define BTN_RIGHT 6
#define BTN_ROTATE 22

// Buzzer
#define BUZZER_PIN 10

// Display OLED
ssd1306_t disp;
#define I2C_SDA 14
#define I2C_SCL 15

// Cores
#define COLOR_BG 0x000000
#define COLOR_BLUE 0x0000FF

// Notas musicais
typedef enum {
    C4 = 262, D4 = 294, E4 = 330, F4 = 349,
    G4 = 392, A4 = 440, B4 = 494, C5 = 523
} Notes;

void init_hardware() {
    stdio_init_all();
    
    // Botões
    gpio_init(BTN_LEFT); gpio_pull_up(BTN_LEFT);
    gpio_init(BTN_RIGHT); gpio_pull_up(BTN_RIGHT);
    
    // Display
    i2c_init(i2c1, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    
    // Buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0); // Desliga o PWM inicialmente
}

void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

void npInit(uint pin) {
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
  
    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
      np_pio = pio1;
      sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }
  
    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
  
    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i) {
      leds[i].R = 0;
      leds[i].G = 0;
      leds[i].B = 0;
    }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}
  
/**
 * Limpa o buffer de pixels.
 */
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}
  
/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24-(y * 5 + x); // Linha par (esquerda para direita).
    } else {
        return 24-(y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
    }
}

void play_tone(uint pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys); 
    uint32_t top = clock_freq / frequency - 1;
    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2); // 50% de duty cycle

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
    sleep_ms(70); // Pausa entre notas
}

void draw_neutral() {
    npClear();
    npSetLED(getIndex(1, 1), 0, 0, 255); // Olho esquerdo
    npSetLED(getIndex(3, 1), 0, 0, 255); // Olho direito
    npSetLED(getIndex(1, 3), 0, 0, 255); // Boca esquerda
    npSetLED(getIndex(2, 3), 0, 0, 255); // Boca meio
    npSetLED(getIndex(3, 3), 0, 0, 255); // Boca direita
    npWrite();
}

void draw_angry() {
    npClear();
    npSetLED(getIndex(1, 1), 255, 0, 0); // Olho esquerdo
    npSetLED(getIndex(3, 1), 255, 0, 0); // Olho direito
    npSetLED(getIndex(0, 2), 255, 0, 0); // Sobrancelha esquerda
    npSetLED(getIndex(4, 2), 255, 0, 0); // Sobrancelha direita
    npSetLED(getIndex(1, 3), 255, 0, 0); // Boca esquerda
    npSetLED(getIndex(2, 3), 255, 0, 0); // Boca meio
    npSetLED(getIndex(3, 3), 255, 0, 0); // Boca direita
    npWrite();
}

void draw_smiley() {
    npClear();
    npSetLED(getIndex(1, 1), 0, 255, 0); // Olho esquerdo
    npSetLED(getIndex(3, 1), 0, 255, 0); // Olho direito
    npSetLED(getIndex(0, 3), 0, 255, 0); // Boca esquerda
    npSetLED(getIndex(1, 4), 0, 255, 0); // Boca meio esquerda
    npSetLED(getIndex(2, 4), 0, 255, 0); // Boca meio
    npSetLED(getIndex(3, 4), 0, 255, 0); // Boca meio direita
    npSetLED(getIndex(4, 3), 0, 255, 0); // Boca direita
    npWrite();
}

void positive_sound() {
    play_tone(BUZZER_PIN, E4, 100); 
    play_tone(BUZZER_PIN, G4, 100);
    play_tone(BUZZER_PIN, C5, 200);
}

void negative_sound() {
    play_tone(BUZZER_PIN, C4, 100); 
    play_tone(BUZZER_PIN, G4, 100);
    play_tone(BUZZER_PIN, C4, 200);
}

typedef struct {
    char dialogue[128];
    char choices[2][32];
    int next_state[2];
    void (*led_effect)();
    void (*sound)();
} GameState;

GameState game_states[] = {
   {// Estado 0 - Início
    "Voce esta preso! O que faz?",
    {"Procurar saida", "Pedir ajuda"},
    {1, 2}, draw_neutral, NULL
},
{// Estado 1 - Procurar saida
    "Encontrou uma porta!",
    {"Abrir porta", "Voltar"},
    {3, 0}, draw_smiley, positive_sound
},
{// Estado 2 - Pedir ajuda
    "Ninguem responde...",
    {"Procurar saida", "Gritar mais"},
    {1, 4}, draw_angry, negative_sound
},
{// Estado 3 - Porta trancada
    "Porta precisa de senha!",
    {"Tentar 0000", "Tentar 1234"},
    {5, 6}, draw_neutral, NULL
},
{// Estado 4 - Fim ruim
    "Algo ruim aconteceu...",
    {"Reiniciar", ""},
    {0, -1}, draw_angry, negative_sound
},
{// Estado 5 - Senha correta
    "Porta aberta! Escapou!",
    {"Jogar novamente", ""},
    {0, -1}, draw_smiley, positive_sound
},
{// Estado 6 - Senha errada
    "Alarme disparado!",
    {"Reiniciar", ""},
    {0, -1}, draw_angry, negative_sound
}
};

#define MAX_LINES 4
#define CHARS_PER_LINE 21

// Função para quebra de texto em múltiplas linhas
void wrap_text(const char *text, char lines[MAX_LINES][CHARS_PER_LINE + 1]) {
    memset(lines, 0, sizeof(lines[0][0]) * MAX_LINES * (CHARS_PER_LINE + 1));
    
    int line = 0, pos = 0, last_space = -1;
    for(int i = 0; text[i] && line < MAX_LINES; i++) {
        if(text[i] == ' ') last_space = i;
        
        if(pos >= CHARS_PER_LINE) {
            if(last_space != -1) {
                lines[line][last_space % CHARS_PER_LINE] = '\0';
                i = last_space;
                last_space = -1;
            }
            line++;
            pos = 0;
        }
        
        if(line >= MAX_LINES) break;
        lines[line][pos++] = text[i];
    }
}

// Função para exibir texto com efeito de digitação
void type_text(const char *text, int x, int y, int scale, int typing_duration) {
    int len = strlen(text);
    int delay_per_char = typing_duration / (len > 0 ? len : 1);

    for(int i = 0; i < len; i++) {
        char buffer[2] = {text[i], '\0'};
        ssd1306_draw_string(&disp, x + (i * 6 * scale), y, scale, buffer);
        ssd1306_show(&disp);
        sleep_ms(delay_per_char);
    }
}

// Função modificada para atualizar display com efeito de digitação
void update_display(int state, int typing_duration) {
    ssd1306_clear(&disp);
    GameState *s = &game_states[state];
    
    char dialogue_lines[MAX_LINES][CHARS_PER_LINE + 1];
    wrap_text(s->dialogue, dialogue_lines);
    
    // Exibir diálogo com efeito de digitação
    int y_pos = 0;
    for(int i = 0; i < MAX_LINES && dialogue_lines[i][0]; i++) {
        type_text(dialogue_lines[i], 0, y_pos, 1, typing_duration);
        y_pos += 10; // Espaçamento entre linhas
    }
     // Exibir opções sem efeito de digitação
     ssd1306_draw_string(&disp, 0, 40, 1, "A:");
     ssd1306_draw_string(&disp, 16, 40, 1, s->choices[0]);
     ssd1306_draw_string(&disp, 0, 54, 1, "B:");
     ssd1306_draw_string(&disp, 16, 54, 1, s->choices[1]);
     
     ssd1306_show(&disp);
 }

int main() {
    init_hardware();
    npInit(LED_PIN);
    int current_state = 0;
    
    // Mostrar estado inicial com efeito de digitação
    update_display(current_state, 3000);
    game_states[current_state].led_effect();
    
    while(1) {
        if(!gpio_get(BTN_LEFT) || !gpio_get(BTN_RIGHT)) {
            int choice = !gpio_get(BTN_LEFT) ? 0 : 1;
            GameState *s = &game_states[current_state];
            
            if(s->sound) s->sound();
            
            current_state = s->next_state[choice];
            
            // Tempo de digitação baseado na escolha
            int duration = (choice == 0) ? 2000 : 1500;
            update_display(current_state, duration);
            
            if(s->led_effect) s->led_effect();
            
            if(current_state == -1) {
                // Animação final
                for(int i = 0; i < 3; i++) {
                    ssd1306_clear(&disp);
                    ssd1306_draw_string(&disp, 40, 28, 2, "FIM!");
                    ssd1306_show(&disp);
                    play_tone(BUZZER_PIN, C5, 200);
                    sleep_ms(500);
                }
                while(1);
            }
            
            sleep_ms(500); // Debounce
        }
        sleep_ms(10);
    }
}