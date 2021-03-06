/*
 * main.c
 *
 * Created: 05/03/2019 18:00:58
 *  Author: eduardo
 */ 

#include <asf.h>
#include "tfont.h"
#include "sourcecodepro_28.h"
#include "calibri_36.h"
#include "arial_72.h"

// butaum
#define BUT_PIO           PIOD
#define BUT_PIO_ID        ID_PIOD
#define BUT_PIO_IDX       28
#define BUT_IDX_MASK      (1u << BUT_PIO_IDX)


// LED
#define LED_PIO      PIOC
#define LED_PIO_ID   ID_PIOC
#define LED_IDX      8u
#define LED_IDX_MASK (1u << LED_IDX)



struct ili9488_opt_t g_ili9488_display_opt;


void pin_toggle(Pio *pio, uint32_t mask);
void io_init(void);
static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);
//int *pulsos = 0;

void configure_lcd(void){
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);

	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
	
}

volatile Bool f_rtt_alarme = false;
volatile int buttonpress = 0;

void pin_toggle(Pio *pio, uint32_t mask){
	if(pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio,mask);
}

void but_callback(void)
{
	buttonpress = 1;
}

void font_draw_text(tFont *font, const char *text, int x, int y, int spacing) {
	char *p = text;
	while(*p != NULL) {
		char letter = *p;
		int letter_offset = letter - font->start_char;
		if(letter <= font->end_char) {
			tChar *current_char = font->chars + letter_offset;
			ili9488_draw_pixmap(x, y, current_char->image->width, current_char->image->height, current_char->image->data);
			x += current_char->image->width + spacing;
		}
		p++;
	}	
}

void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		pin_toggle(LED_PIO, LED_IDX_MASK);    // BLINK Led
		f_rtt_alarme = true;                  // flag RTT alarme
	}
}

static float get_time_rtt(){
	uint ul_previous_time = rtt_read_timer_value(RTT);
}

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
	uint32_t ul_previous_time;

	/* Configure RTT for a 1 second tick interrupt */
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT));
	
	rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
}


void io_init(void)
{
	// Configura led
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_configure(LED_PIO, PIO_OUTPUT_0, LED_IDX_MASK, PIO_DEFAULT);
	
	// Inicializa clock do perif�rico PIO responsavel pelo botao
	pmc_enable_periph_clk(BUT_PIO_ID);

	// Configura PIO para lidar com o pino do bot�o como entrada
	// com pull-up
	pio_configure(BUT_PIO, PIO_INPUT, BUT_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);

	// Configura interrup��o no pino referente ao botao e associa
	// fun��o de callback caso uma interrup��o for gerada
	// a fun��o de callback � a: but_callback()
	pio_handler_set(BUT_PIO,
	BUT_PIO_ID,
	BUT_IDX_MASK,
	PIO_IT_FALL_EDGE,
	but_callback);

	// Ativa interrup��o
	pio_enable_interrupt(BUT_PIO, BUT_IDX_MASK);

	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais pr�ximo de 0 maior)
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4); // Prioridade 4
}


int main(void) {
	char buffer[32];
	int N = 0;
	int Naux = 0;
	int velang = 0;
	int vel = 0;
	int dist = 0;
	f_rtt_alarme = true;
	board_init();
	sysclk_init();	
	configure_lcd();
	
	io_init();
	
	sprintf(buffer,"%02d",N);
	font_draw_text(&calibri_36, "Pulsos", 25, 50, 1);
	font_draw_text(&calibri_36, buffer, 245, 50, 1);
	
	sprintf(buffer,"%02d",velang);
	font_draw_text(&calibri_36, "VEL_ang:", 25, 100, 1);
	font_draw_text(&calibri_36, buffer, 245, 100, 1);
	
	sprintf(buffer,"%02d",vel);
	font_draw_text(&calibri_36, "Vel[km/h]:", 25, 150, 1);
	font_draw_text(&calibri_36, buffer, 245, 150, 1);
	
	sprintf(buffer,"%02d",dist);
	font_draw_text(&calibri_36, "Dist[m]:", 25, 200, 1);
	font_draw_text(&calibri_36, buffer, 245, 200, 1);
		
	buttonpress = 0;	
	
	while(1) {
		if(buttonpress) {
			N += 1;
			Naux += 1;
			buttonpress = 0;
			sprintf(buffer,"%02d",N);
			font_draw_text(&calibri_36, buffer, 245, 50, 1);
			
		}
		if (f_rtt_alarme) {
			uint16_t pllPreScale = (int) (((float) 32768) / 2.0);
			uint32_t irqRTTvalue  = 8;
      
			// reinicia RTT para gerar um novo IRQ
			RTT_init(pllPreScale, irqRTTvalue);
		  
			dist = (int) ((N) * 2 * 3.14 * 0.325);
			velang = (int) (2 * 3.14 * Naux / 4);
			vel = (int) (velang * 0.325 * 3.6);
			     
			sprintf(buffer,"%02d",velang);
			font_draw_text(&calibri_36, buffer, 245, 100, 1);
			sprintf(buffer,"%02d",vel);
			font_draw_text(&calibri_36, buffer, 245, 150, 1);
			sprintf(buffer,"%02d",dist);
			font_draw_text(&calibri_36, buffer, 245, 200, 1);
			
			Naux = 0;
			f_rtt_alarme = false;
		}
	}
	  
}
