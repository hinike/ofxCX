#ifndef _CX_JOYSTICK_H_
#define _CX_JOYSTICK_H_

#include <queue>
#include <string>
#include <vector>

#include "CX_Clock.h"

namespace CX {

	/*! This struct contains information about joystick events. Joystick events are either a button
	press or release or a change in the axes of the joystick. 
	
	\see CX::CX_Joystick::getNextEvent() provides access to joystick events.
	\ingroup input */
	struct CX_JoystickEvent_t {

		int buttonIndex; //!< If eventType is BUTTON_PRESS or BUTTON_RELEASE, this contains the index of the button that was changed.
		unsigned char buttonState; //!< If eventType is BUTTON_PRESS or BUTTON_RELEASE, this contains the current state of the button.

		int axisIndex; //!< If eventType is AXIS_POSITION_CHANGE, this contains the index of the axis which changed.
		float axisPosition; //!< If eventType is AXIS_POSITION_CHANGE, this contains the amount by which the axis changed.

		CX_Micros eventTime; //!< The time at which the event was registered. Can be compared to the result of CX::Clock::getTime().
		CX_Micros uncertainty; //!< The uncertainty in eventTime. The event occured some time between eventTime and eventTime minus uncertainty.

		enum JoystickEventType {
			BUTTON_PRESS, //!< A button on the joystick has been pressed. See \ref buttonIndex and \ref buttonState for the event data.
			BUTTON_RELEASE, //!< A button on the joystick has been released. See \ref buttonIndex and \ref buttonState for the event data.
			AXIS_POSITION_CHANGE //!< The joystick has been moved in one of its axes. See \ref axisIndex and \ref axisPosition for the event data.
		} eventType; //!< The type of the event.

	};

	/*! This class manages a joystick that is attached to the system (if any). If more than one joystick is needed
	for the experiment, you can create more instances of CX_Joystick other than the one in CX::Instances::Input.
	\ingroup input */
	class CX_Joystick {
	public:
		CX_Joystick (void);
		~CX_Joystick (void);

		bool setup (int joystickIndex);
		std::string getJoystickName (void);

		//Preferred interface. This interface collects response time data.
		bool pollEvents (void);
		int availableEvents (void);
		CX_JoystickEvent_t getNextEvent (void);
		void clearEvents (void);

		//Direct access functions. The preferred interface is pollEvents() and getNextEvent().
		std::vector<float> getAxisPositions (void);
		std::vector<unsigned char> getButtonStates (void);

	private:
		int _joystickIndex;
		std::string _joystickName;

		std::queue<CX_JoystickEvent_t> _joystickEvents;

		std::vector<float> _axisPositions;
		std::vector<unsigned char> _buttonStates;

		CX_Micros _lastEventPollTime;
	};

	std::ostream& operator<< (std::ostream& os, const CX_JoystickEvent_t& ev);
	std::istream& operator>> (std::istream& is, CX_JoystickEvent_t& ev);

}

#endif //_CX_JOYSTICK_H_