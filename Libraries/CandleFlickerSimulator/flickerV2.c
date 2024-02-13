/* Ported from
https://github.com/EternityForest/CandleFlickerSimulator/tree/master

Super-realistic candle flicker algorithms that can run on even the lowest-end
microcontrolers out there. Comes in two versions. The first doubles as a
jitter-free rainbow nightlight and uses the generation 1 algorithm for
flickering, wheras the second does not yet include the rainbow feature yet,but
uses the generation 2 algorithm which attempts to model wind gust patterns and
compile to around 200 bytes of ram.

The RNG in gen 1 is a custom one I stumbled on that generates very good
flickering without much filtering (it is not a true RNG) wheras gen 2 uses my
XABC rng design to drive a more physically-inspired cande simulation.
*/

#include <flicker.h>
#include <stdint.h>

#define WIND_VARIABILITY 5
#define FLAME_AGILITY 2
#define WIND_CALMNESS_CONSTANT 2
#define WIND_BASELINE 30

static unsigned char flame, flameprime, wind, x, a, b, c;

static unsigned char random() {
  x++; // x is incremented every round and is not affected by any other variable
  a = (a ^ c ^ x);            // note the mix of addition and XOR
  b = (b + a);                // And the use of very few instructions
  c = (((c + (b >> 1)) ^ a)); // the AES S-Box Operation ensures an even
                              // distributon of entropy
  return (c);
}

/// @brief uint8_t flickerV2() - call this function every ~5ms to get the next
/// brightness value
/// @param
/// @return uint8_t - brightness value
/// example usage:
/// while(1) {
///   SetPWM(flickerV2());
///   delay(5);
/// }
uint8_t flickerV2(void) {

  // We simulate a gust of wind by setting the wind var to a random value
  if (random() < WIND_VARIABILITY) {
    // Make a gust of wind less likely with two random teata because 255 is
    // not enough resolution
    if (random() > 220) {
      wind = random();
    }
  }

  // The wind constantly settles towards its baseline value
  if (wind > WIND_BASELINE) {
    wind--;
  }

  // The flame constantly gets brighter till the wind knocks it down
  if (flame < 255) {
    flame++;
  }

  // Depending on the wind strength and the calmnes modifer we calcuate the
  // odds of the wind knocking down the flame by setting it to random values
  if (random() < (wind >> WIND_CALMNESS_CONSTANT)) {
    flame = random();
  }

  // Real flames ook like they have inertia so we use this
  // constant-aproach-rate filter To lowpass the flame height
  if (flame > flameprime) {
    if (flameprime < (255 - FLAME_AGILITY)) {
      flameprime += FLAME_AGILITY;
    }
  } else {
    if (flameprime > (FLAME_AGILITY)) {
      flameprime -= FLAME_AGILITY;
    }
  }
  // How do we prevent jittering when the two are equal?
  // We don't. It adds to the realism.

  return flameprime;
}
