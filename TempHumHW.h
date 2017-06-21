/*
 * TempHumHW for Particle
 *
 * Optimus Systems
 * written by Robert Vinci
 *
 */

 #ifndef THHW_H
 #define THHW_H

 #include "application.h"
 #include "math.h"

//Hardware Devices used for Temperature and Humidity Sensor Monitoring
 #define SSR01  01   //G3MB-202P Solid State Relay 2A
 #define SPDT01 02   //SPDT 5V relay for temperature control

 #define LOW  0     //low of off state
 #define HIGH 1     //high on on state

 /*struct TIMEofDAY {
     uint8_t WAKE;
     uint8_t MORN;
     uint8_t EVEN;
     uint8_t NGHT;
 } timeofday;   // Define object of type PERSON
*/
 class RLY {
 	private:
 		uint8_t _pin, _type, _count, _currstate;
 		unsigned long _lastwritetime;
 		boolean firstwrite;

 	public:
 		RLY(uint8_t pin, uint8_t type, uint8_t count=6);
 		void begin(void);
 		void setRelayOut(boolean state);
    boolean getRlyState();
 };
/*
 TIMEofDAY getTimeDateSP(Time.hour(), Time.minute()){

 }
 */
 #endif
