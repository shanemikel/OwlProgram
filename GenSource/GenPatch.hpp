#ifndef __GenPatch_hpp__
#define __GenPatch_hpp__

#include "Patch.h"
#include "gen.h"

#define GEN_OWL_PARAM_FREQ "freq"
#define GEN_OWL_PARAM_GAIN "gain"
#define GEN_OWL_PARAM_GATE "gate"
#define GEN_OWL_PARAM_BEND "bend"
#define GEN_OWL_PARAM_E "Exp"
#define GEN_OWL_PARAM_PUSH "Push"
#define GEN_OWL_PARAM_BUTTON_A "ButtonA"
#define GEN_OWL_PARAM_BUTTON_B "ButtonB"
#define GEN_OWL_PARAM_BUTTON_C "ButtonC"
#define GEN_OWL_PARAM_BUTTON_D "ButtonD"
#define GEN_OWL_MAX_PARAM_COUNT 20

class MonoVoiceAllocator {
  float& freq;
  float& gain;
  float& gate;
  float& bend;
  uint8_t notes[16];
  uint8_t lastNote = 0;
public:
  MonoVoiceAllocator(float& fq, float& gn, float& gt, float& bd): freq(fq), gain(gn), gate(gt), bend(bd) {
  }
  float getFreq(){
    return freq;
  }
  float getGain(){
    return gain;
  }
  float getGate(){
    return gate;
  }
  float getBend(){
    return bend;
  }
  void processMidi(MidiMessage msg){
    uint16_t samples = 0;
    if(msg.isNoteOn()){
      noteOn(msg.getNote(), (uint16_t)msg.getVelocity()<<5, samples);
    }else if(msg.isNoteOff()){
      noteOff(msg.getNote(), (uint16_t)msg.getVelocity()<<5, samples);      
    }else if(msg.isPitchBend()){
      setPitchBend(msg.getPitchBend());
    }else if(msg.isControlChange()){
      if(msg.getControllerNumber() == MIDI_ALL_NOTES_OFF)
	allNotesOff();
    }
  }
  void setPitchBend(int16_t pb){
    float fb = pb*(2.0f/8192.0f);
    bend = exp2f(fb);
  }
  float noteToHz(uint8_t note){
    return 440.0f*exp2f((note-69)/12.0);
  }
  float velocityToGain(uint16_t velocity){
    return exp2f(velocity/4095.0f) -1;
  }
  void noteOn(uint8_t note, uint16_t velocity, uint16_t delay){
    if(lastNote < 16)
      notes[lastNote++] = note;
    freq = noteToHz(note);
    gain = velocityToGain(velocity);
    gate = 1;
  }
  void noteOff(uint8_t note, uint16_t velocity, uint16_t delay){
    int i;
    for(i=0; i<lastNote; ++i){
      if(notes[i] == note)
    	break;
    }
    if(lastNote > 1){
      lastNote--;
      while(i<lastNote){
	notes[i] = notes[i+1];
	i++;
      }
      freq = noteToHz(notes[lastNote-1]);
    }else{
      gate = 0;
      lastNote = 0;
    }
  }
  void allNotesOff(){
    lastNote = 0;
    bend = 0;
  }
};

class GenParameterBase {
public:
  virtual void update(Patch* patch, CommonState *context){}
};

class GenParameter : public GenParameterBase {
public:
  PatchParameterId pid;
  int8_t index;
  float min = 0.0;
  float max = 1.0;
  GenParameter(Patch* patch, CommonState *context, const char* name, PatchParameterId id, int8_t idx) : pid(id), index(idx) {
    patch->registerParameter(id, name);
    if(gen::getparameterhasminmax(context, index)){
      min = gen::getparametermin(context, index);
      max = gen::getparametermax(context, index);
    }    
  }
  void update(Patch* patch, CommonState *context){
    float value = patch->getParameterValue(pid);
    value = value * (max-min) + min;
    gen::setparameter(context, index, value, NULL);
  }
};

class GenButton : public GenParameterBase  {
public:
  PatchButtonId bid;
  int8_t index;
  float min = 0.0;
  float max = 1.0;
  GenButton(Patch* patch, CommonState *context, const char* name, PatchButtonId id, int8_t idx) : bid(id), index(idx) {
    if(gen::getparameterhasminmax(context, index)){
      min = gen::getparametermin(context, index);
      max = gen::getparametermax(context, index);
    }
  }
  void update(Patch* patch, CommonState *context){
    float value = patch->isButtonPressed(bid);
    value = value * (max-min) + min;
    gen::setparameter(context, index, value, NULL);
  }
};

class GenVariableParameter : public GenParameterBase  {
public:
  float* fp;
  int8_t index;
  GenVariableParameter(Patch* patch, CommonState *context, const char* name, float* f, int8_t idx)
    : fp(f), index(idx) {}
  void update(Patch* patch, CommonState *context){
    float value = *fp;
    // clip value to min/max (not needed, done inside gen~ code)
    // value = value > min ? (value < max ? value : max) : min;
    gen::setparameter(context, index, value, NULL);
  }
};

class GenPatch : public Patch {
private:
  CommonState *context;
  GenParameterBase* params[GEN_OWL_MAX_PARAM_COUNT];
  size_t param_count = 0;
 float freq = 440.0f;
 float gain = 0.0f;
 float gate = 0.0f;
 float bend = 1.0;
  MonoVoiceAllocator allocator;
public:
  GenPatch() : allocator(freq, gain, gate, bend) {
    context = (CommonState*)gen::create(getSampleRate(), getBlockSize());
    int numParams = gen::num_params();
    for(int i = 0; i < numParams && param_count < GEN_OWL_MAX_PARAM_COUNT; i++){
      const char* name = gen::getparametername(context, i);
      if(strcasecmp(GEN_OWL_PARAM_FREQ, name) == 0){
	params[param_count++] = new GenVariableParameter(this, context, name, &freq, i);
      }else if(strcasecmp(GEN_OWL_PARAM_GAIN, name) == 0){
	params[param_count++] = new GenVariableParameter(this, context, name, &gain, i);
      }else if(strcasecmp(GEN_OWL_PARAM_GATE, name) == 0){
	params[param_count++] = new GenVariableParameter(this, context, name, &gate, i);
      }else if(strcasecmp(GEN_OWL_PARAM_BEND, name) == 0){
	params[param_count++] = new GenVariableParameter(this, context, name, &bend, i);
      }else if(strcasecmp(GEN_OWL_PARAM_E, name) == 0){
	params[param_count++] = new GenParameter(this, context, name, PARAMETER_E, i);
      }else if(strcasecmp(GEN_OWL_PARAM_PUSH, name) == 0){
	params[param_count++] = new GenButton(this, context, name, PUSHBUTTON, i);	
      }else if(strcasecmp(GEN_OWL_PARAM_BUTTON_A, name) == 0){
	params[param_count++] = new GenButton(this, context, name, BUTTON_A, i);
      }else if(strcasecmp(GEN_OWL_PARAM_BUTTON_B, name) == 0){
	params[param_count++] = new GenButton(this, context, name, BUTTON_B, i);
      }else if(strcasecmp(GEN_OWL_PARAM_BUTTON_C, name) == 0){
	params[param_count++] = new GenButton(this, context, name, BUTTON_C, i);
      }else if(strcasecmp(GEN_OWL_PARAM_BUTTON_D, name) == 0){
	params[param_count++] = new GenButton(this, context, name, BUTTON_D, i);
      }else if(strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'H'){
	uint8_t index = name[0]-'A';
	params[param_count++] = new GenParameter(this, context, name, (PatchParameterId)index, i);
      }else if(strlen(name) == 2 && name[0] >= 'A' && name[0] <= 'H'
	                         && name[1] >= 'A' && name[1] <= 'H'){
	uint8_t index = PARAMETER_H*(name[0]-'A') + name[1]-'A';
	params[param_count++] = new GenParameter(this, context, name, (PatchParameterId)index, i);
      }
    }
  }

  ~GenPatch() {
    gen::destroy(context);
    for(int i=0; i<param_count; ++i)
      delete params[i];
  }

  void processMidi(MidiMessage msg){
    allocator.processMidi(msg);
  }

  void processAudio(AudioBuffer &buffer) {
    for(int i=0; i<param_count; ++i)
      params[i]->update(this, context);
    float* outputs[] = {buffer.getSamples(0), buffer.getSamples(1) };
    gen::perform(context, outputs, 2, outputs, 2, buffer.getSize());
  }
};

#endif // __GenPatch_hpp__
