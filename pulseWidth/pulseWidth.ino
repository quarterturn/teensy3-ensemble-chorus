#include <Wire.h>
#include <Audio.h>
#include <MIDI.h>
#include <BALibrary.h>
#include <effect_ensemble.h>

// demonstrate pulse with slow changes in pulse width

using namespace BALibrary;

AudioOutputI2S i2s1;
BAAudioControlWM8731 codec;

// GUItool: begin automatically generated code
AudioSynthWaveform       waveform1;      //xy=188,240
AudioEffectEnvelope      envelope1;      //xy=371,237
AudioEffectEnsemble      ensemble1;     //xy=379,489

AudioConnection          patchCord1(waveform1, envelope1);
AudioConnection          patchCord2(envelope1, ensemble1);
AudioConnection          leftOut(ensemble1, 0, i2s1, 0);
AudioConnection          rightOut(ensemble1, 0, i2s1, 1);

// GUItool: end automatically generated code


void setup(void)
{

  // Set up
  delay(100);
  Serial.begin(57600); // Start the serial port

  // Disable the codec first
  codec.disable();
  delay(100);
  AudioMemory(128);
  delay(5);

  // Enable the codec
  Serial.println("Enabling codec...\n");
  codec.enable();
  delay(100);

  waveform1.pulseWidth(0.5);
  waveform1.begin(0.4, 220, WAVEFORM_PULSE);

  envelope1.attack(50);
  envelope1.decay(50);
  envelope1.release(250);

}

void loop() {
  
  float w;
  for (uint32_t i =1; i<20; i++) {
    w = i / 20.0;
    waveform1.pulseWidth(w);
    envelope1.noteOn();
    delay(1800);
    envelope1.noteOff();
    delay(600);
  }
}
