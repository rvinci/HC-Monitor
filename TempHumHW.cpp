/*
 * TempHumHW for Particle
 *
 * Optimus Systems
 * written by Robert Vinci
 *
 */
 #include "TempHumHW.h"

 RLY::RLY(uint8_t pin, uint8_t type, uint8_t count) {
 	_pin = pin;
 	_type = type;
 	_count = count;
  _currstate = LOW;
 	firstwrite = true;
 }

 void RLY::begin(void) {
 // set up the pins!
 	pinMode(_pin, OUTPUT);
 	digitalWrite(_pin, LOW);
 	_lastwritetime = 0;
 }

 void RLY::setRelayOut(boolean state) {
 		switch (_type) {
 			case SSR01:
 				 digitalWrite(_pin, state);
         _currstate = state;
 			case SPDT01:
 				 digitalWrite(_pin, state);
         _currstate = state;
 		}
 }

 boolean RLY::getRlyState() {
 	return _currstate;
 }
