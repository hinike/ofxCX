#ifndef _CX_SLIDE_PRESENTER_H_
#define _CX_SLIDE_PRESENTER_H_

#include <stdint.h>

#include "ofFbo.h"

#include "CX_Display.h"

namespace CX {

	struct CX_Slide_t {

		CX_Slide_t () :
			slideStatus(NOT_STARTED)
		{}

		string slideName;

		ofFbo framebuffer;

		enum {
			NOT_STARTED,
			COPY_TO_BACK_BUFFER_PENDING,
			SWAP_PENDING,
			IN_PROGRESS,
			FINISHED
		} slideStatus;

		uint32_t intendedFrameCount;
		uint32_t intendedOnsetFrameNumber;
		uint32_t actualFrameCount;
		uint32_t actualOnsetFrameNumber;

		uint64_t intendedSlideDuration; //These durations (in microseconds) are good for about 600,000 years.
		uint64_t actualSlideDuration;
		uint64_t intendedSlideOnset; 
		uint64_t actualSlideOnset;

		uint64_t copyToBackBufferCompleteTime; //This is pretty useful to determine if there was an error on the trial (i.e. framebuffer copied late).
	
	};

	class CX_SlidePresenter {
	public:

		CX_SlidePresenter (void);

		void update (void);
		void setDisplay (CX_Display *display); //Maybe this should be called "setup"?

	
		void appendSlide (CX_Slide_t slide); //This is kind of sucky because people have to manually allocate the FBOs

		//Much easier way of doing things.
		void startNextSlide (string slideName, uint64_t duration);
		void finishCurrentSlide (void);

		void clearSlides (void);
	

		void startSlidePresentation (void);
		bool isPresentingSlides (void) { return _presentingSlides || _synchronizing; };


		vector<CX_Slide_t> getSlides (void);
		vector<uint64_t> getActualPresentationDurations (void);

		int checkForPresentationErrors (void);

	private:

		CX_Display *_display;

		bool _presentingSlides;
		bool _synchronizing;
		int _currentSlide;
		vector<CX_Slide_t> _slides;

		bool _awaitingFenceSync;
		GLsync _fenceSyncObject;
	
		bool _lastFramebufferActive;

	};

}

#endif //_CX_SLIDE_PRESENTER_H_