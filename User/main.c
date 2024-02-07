/**
  ******************************************************************************
  * @file       main.c
  * @author     Brad
  * @version    V1.0
  * @date       2024-xx-xx
  * @brief      IkeaDimmer - reads value from a rotary encoder, and sets a PWM
                output
  ******************************************************************************
  */
#include "hk32f030m.h"
#include <stdint.h>

// PWM: PC6(TIM1_CH1 (AF3), pin 16)
#define PWM_GPIO_PORT GPIOC
#define PWM_GPIO_CLK RCC_AHBPeriph_GPIOC
#define PWM_GPIO_PIN GPIO_Pin_6
#define PWM_GPIO_PINSOURCE GPIO_PinSource6
#define PWM_GPIO_AF GPIO_AF_3
// Encoder 1: PD1, pin 18
#define ENC1_EXTI_IRQ EXTI1_IRQn
#define ENC1_LINE EXTI_Line1
#define ENC1_IRQHandler EXTI1_IRQHandler
#define ENC1_GPIO_PORT GPIOD
#define ENC1_GPIO_CLK RCC_AHBPeriph_GPIOD
#define ENC1_PORTSOURCE EXTI_PortSourceGPIOD
#define ENC1_GPIO_PIN GPIO_Pin_1
#define ENC1_PINSOURCE GPIO_PinSource1
// Encoder 2: PD2, pin 19
#define ENC2_GPIO_PORT GPIOD
#define ENC2_GPIO_CLK RCC_AHBPeriph_GPIOD
#define ENC2_GPIO_PIN GPIO_Pin_2
// Encoder Button: PD3, pin 20
#define ENC3_EXTI_IRQ EXTI3_IRQn
#define ENC3_LINE EXTI_Line3
#define ENC3_IRQHandler EXTI3_IRQHandler
#define ENC3_GPIO_PORT GPIOD
#define ENC3_GPIO_CLK RCC_AHBPeriph_GPIOD
#define ENC3_PORTSOURCE EXTI_PortSourceGPIOD
#define ENC3_GPIO_PIN GPIO_Pin_3
#define ENC3_PINSOURCE GPIO_PinSource3

// Global variables for ISRs
volatile uint8_t encoder_btn_event = 0;
volatile int16_t encoder_direction = 0;
volatile uint32_t encoder_polled = 0;
volatile uint32_t millis_ticks = 0;

/// @brief Configure TIM1CH1 for 2kHz 14-bit PWM
/// @param  none
void TIM1_Config(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  TIM_OCInitTypeDef TIM_OCInitStructure;

  TIM_DeInit(TIM1);
  // Enable peripheral Clocks
  RCC_AHBPeriphClockCmd(PWM_GPIO_CLK, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

  // PWM: Output, push-pull, speed 10Mhz
  GPIO_PinAFConfig(PWM_GPIO_PORT, PWM_GPIO_PINSOURCE,
                   PWM_GPIO_AF); // Assign to AF3
  GPIO_InitStructure.GPIO_Pin = PWM_GPIO_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Schmit = GPIO_Schmit_Disable;
  GPIO_Init(PWM_GPIO_PORT, &GPIO_InitStructure);

  // Auto-Reload ARR, Timer1 period, Timer1 frequency = Timer1 clock / (ARR + 1)
  TIM_TimeBaseStructure.TIM_Period = (16384 - 1);
  // Prescaler PSC, Timer1 clock = FCLK / (PSC + 1)
  TIM_TimeBaseStructure.TIM_Prescaler = (0);
  // CR1_CKD, for dead-time insertion
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  // Counter mode
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Down;
  // RCR, Repetition Counter value
  TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
  TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

  // Output Compare Mode 1
  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
  // Enable output
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
  // CCR1, Specifies the pulse value to be loaded into the Capture Compare
  // Register
  TIM_OCInitStructure.TIM_Pulse = 0;
  // Output polarity
  TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
  // Pin state during Idle state
  TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;

  TIM_OC1Init(TIM1, &TIM_OCInitStructure);
  TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

  // Enable Timer1
  TIM_Cmd(TIM1, ENABLE);
  // Enable PWM output
  TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

/// @brief EXTI_Config(void) - configures GPIOs, EXTI, and NVIC to enable pin
/// change interrupts
/// @param  none
void EXTI_Config(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  EXTI_InitTypeDef EXTI_InitStructure;

  // Enable peripheral Clocks
  RCC_AHBPeriphClockCmd(ENC1_GPIO_CLK | ENC2_GPIO_CLK | ENC3_GPIO_CLK, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

  // Set up NVIC
  // common
  NVIC_InitStructure.NVIC_IRQChannelPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  // D1 - encoder
  NVIC_InitStructure.NVIC_IRQChannel = ENC1_EXTI_IRQ;
  NVIC_Init(&NVIC_InitStructure);
  // D3 - encoder button
  NVIC_InitStructure.NVIC_IRQChannel = ENC3_EXTI_IRQ;
  NVIC_Init(&NVIC_InitStructure);

  // set up GPIOs
  GPIO_InitStructure.GPIO_Pin = ENC1_GPIO_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
  GPIO_InitStructure.GPIO_Schmit = GPIO_Schmit_Enable;
  GPIO_Init(ENC1_GPIO_PORT, &GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = ENC2_GPIO_PIN;
  GPIO_Init(ENC2_GPIO_PORT, &GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = ENC3_GPIO_PIN;
  GPIO_Init(ENC3_GPIO_PORT, &GPIO_InitStructure);

  // Set up EXTIs
  EXTI_DeInit();
  // common
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  // ENC1
  SYSCFG_EXTILineConfig(ENC1_PORTSOURCE, ENC1_PINSOURCE);
  EXTI_InitStructure.EXTI_Line = ENC1_LINE;
  EXTI_Init(&EXTI_InitStructure);
  // ENC3 - encoder button
  SYSCFG_EXTILineConfig(ENC3_PORTSOURCE, ENC3_PINSOURCE);
  EXTI_InitStructure.EXTI_Line = ENC3_LINE;
  EXTI_Init(&EXTI_InitStructure);
}

// Set up SysTick to interrupt every millisecond
void SysTick_Init(void) {
  // set up systick and enable interrupt
  SysTick_Config(SystemCoreClock / 8000); // trigger ISR (32MHz/8000 Hz)
  // set SysTick clock source to 1/8 of HCLK (SysTick_Config seems to set to
  // Div1)
  SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
  // will now run whatever is in SysTick_Handler() every millisecond.
}

// SysTick ISR
void SysTick_Handler(void) {
  millis_ticks++; // rolls over every ~50 days
}

/// @brief millis() - provides timestamps
/// @param  none
/// @return returns the current count (in milliseconds) since boot
volatile uint32_t millis(void) { return millis_ticks; }

/// @brief SysTick_DelayMs(uint16_t ms) - blocking delay
/// @param ms - number of seconds to delay for
void SysTick_DelayMs(uint16_t ms) {
  uint32_t start = millis();
  while ((millis() - start) < ms)
    ;
}

// ISR to handle rotation events from the encoder
void ENC1_IRQHandler(void) {
  if (EXTI_GetITStatus(ENC1_LINE) != RESET) {
    if (!encoder_direction)
      encoder_polled = millis();
    if ((!GPIO_ReadInputDataBit(ENC2_GPIO_PORT, ENC2_GPIO_PIN)))
      encoder_direction++;
    else
      encoder_direction--;
    EXTI_ClearITPendingBit(ENC1_LINE);
  }
}

// ISR to handle button press events from the encoder button
void ENC3_IRQHandler(void) {
  if (EXTI_GetITStatus(ENC3_LINE) != RESET) {
    encoder_btn_event = 1;
    EXTI_ClearITPendingBit(ENC3_LINE);
  }
}

/// @brief enc_read(void) - read and process rotary encoder inputs
/// @param  none (reads global 'encoder_direction', updated by EXTI ISR)
/// @return Shaped value from the encoder (shape is linear, *8)
int16_t enc_read(void) {
#define POLLRATE 10
  // ms, how often to check for input and
  // accelerate/decelerate
  //(acceleration/velocity detection not currently implemented)

  int16_t Delta = 0;
  if ((millis() - encoder_polled) > POLLRATE) {
    Delta = encoder_direction * 8;
    encoder_direction = 0; // reset ISR counter
  }
  return Delta; // e.g. A cubic acceleration curve could be 'return
                // Delta*Delta*Delta'
}

int main(void) {
  // initialize Systick and start interrupt
  SysTick_Init();
  SysTick_DelayMs(100); // not sure why this is necessary,
  //  but TIM PWM polarity is unpredictable without
  //  a delay before init. Power glitching?
  // initialize TIM1, and start PWM output on tim1ch1
  TIM1_Config();
  // initialize exti interrupts on encoder gpio inputs
  EXTI_Config();

  uint8_t brightness = 0;
  uint8_t saved_brightness = 255;
  int16_t new_brightness = 0;
  TIM_SetCompare1(TIM1, brightness);

  const uint32_t FadeTime = 3600000; // 3600000; //ms; 1hr
  uint32_t FadeInterval = (FadeTime / (brightness + 1));
  uint32_t FadeEvent = millis();

  for (uint8_t i = 0; i < 254; i += 2) {
    TIM_SetCompare1(TIM1, (((uint16_t)i * (uint16_t)i) >> 2));
    SysTick_DelayMs(1);
  }
  TIM_SetCompare1(TIM1, brightness);

  while (1) {
    new_brightness += enc_read();
    if (new_brightness < 0)
      new_brightness = 0;
    if (new_brightness > 0xFF)
      new_brightness = 0xFF;

    if (encoder_btn_event) {
      encoder_btn_event = 0;
      if (brightness) {
        new_brightness = 0;
        saved_brightness = brightness;
      } else
        new_brightness = saved_brightness;
    }

    if ((uint8_t)new_brightness != brightness) {
      brightness = (uint8_t)new_brightness;
      TIM_SetCompare1(TIM1, ((uint16_t)brightness * (uint16_t)brightness) >> 2);
      FadeInterval = FadeTime / (brightness + 1);
    }
    if ((millis() - FadeEvent) > FadeInterval) {
      if (brightness > 1)
        new_brightness--;
      FadeEvent = millis();
    }
    SysTick_DelayMs(10);
    // Is there a sleep mode I can enter here instead that leaves TIM1 running?
  }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(char *file, uint32_t line) {
  /* User can add his own implementation to report the file name and line
     number, tex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* Infinite loop */

  while (1) {
  }
}
#endif /* USE_FULL_ASSERT */
