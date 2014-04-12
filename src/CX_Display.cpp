#include "CX_Display.h"

#include "CX_Private.h" //glfwContext

using namespace CX;
using namespace CX::Instances;

CX_Display::CX_Display (void) :
	_framePeriod(0),
	_framePeriodStandardDeviation(0),
	_swapThread(nullptr),
	_manualBufferSwaps(0),
	_frameNumberOnLastSwapCheck(0),
	_softVSyncWithGLFinish(false)
{
}

CX_Display::~CX_Display (void) {
	_swapThread->stopThread();
	_swapThread->waitForThread(false);
	delete _swapThread;
}

/*! Set up the display. Must be called for the display to function correctly. */
void CX_Display::setup (void) {
	ofSetLogLevel("ofFbo", OF_LOG_WARNING); //It isn't clear that this should be here, but the fbos 
		//are really verbose when allocated and it is a lot of gibberish.

	//setFullScreen(false); //Default to windowed mode (for development). This has no effect, because it is in windowed mode already.

	_renderer = ofGetGLProgrammableRenderer();
	if (!_renderer) {
		//Log.warning("CX_Display") << "Programmable renderer not available. Standard renderer will be used instead.";
	}

	_swapThread = new Private::CX_VideoBufferSwappingThread(); //This is a work-around for some stupidity in oF or Poco (can't tell which) where 
		//objects inheriting from ofThread cannot be constructed "too early" in program execution (where the quotes mean I have no idea 
		//what too early means) or else there will be a crash.

	BLOCKING_estimateFramePeriod( CX_Millis(500) );
}

/*! Set whether the front and buffers of the display will swap automatically every frame or not.
You can check to see if a swap has occured by calling hasSwappedSinceLastCheck(). You can
check to see if the display is automatically swapping by calling isAutomaticallySwapping().
\param autoSwap If true, the front and back buffer will swap automatically every frame. 
\see \ref blockingCode */
void CX_Display::BLOCKING_setAutoSwapping (bool autoSwap) {
	if (autoSwap) {
		if (!_swapThread->isThreadRunning()) {
			_swapThread->startThread(true, false); //verbose is true only for testing.
		}
	} else {
		if (_swapThread->isThreadRunning()) {
			_swapThread->stopThread();
			_swapThread->waitForThread(false);
		}
	}
}

/*! Determine whether the display is configured to automatically swap the front and back buffers
every frame.
See \ref BLOCKING_setAutoSwapping for more information. */
bool CX_Display::isAutomaticallySwapping (void) {
	return _swapThread->isThreadRunning();
}

/*! Get the last time at which the front and back buffers were swapped. 
\return A time value that can be compared with CX::Instances::Clock.now(). */
CX_Millis CX_Display::getLastSwapTime(void) {
	return _swapThread->getLastSwapTime();
}

/*! Get an estimate of the next time the front and back buffers will be swapped.
This function depends on the precision of the frame period as estimated using 
BLOCKING_estimateFramePeriod(). If the front and back buffers are not swapped
every frame, the result of this function is meaningless because it uses the 
last buffer swap time as a reference.
\return A time value that can be compared to CX::Instances::Clock.now(). */
CX_Millis CX_Display::estimateNextSwapTime(void) {
	return this->getLastSwapTime() + this->getFramePeriod();
	//return _swapThread->estimateNextSwapTime();
}

/*! Gets the estimate of the frame period calculated with BLOCKING_estimateFramePeriod(). */
CX_Millis CX_Display::getFramePeriod(void) {
	return _framePeriod;
}

/*! Gets the estimate of the standard deviation of the frame period calculated with BLOCKING_estimateFramePeriod(). */
CX_Millis CX_Display::getFramePeriodStandardDeviation(void) {
	return _framePeriodStandardDeviation;
}

/*! Check to see if the display has swapped the front and back buffers since the last call to this function.
This is generally used in conjuction with automatic swapping of the buffers (BLOCKING_setAutoSwapping())
or with an individual threaded swap of the buffers (swapFrontAndBackBuffers()). This technically works
with BLOCKING_swapFrontAndBackBuffers(), but given that that function only returns once the buffers have
swapped, checking that the buffers have swapped is redundant.
\return True if a swap has been made since the last call to this function, false otherwise. */
bool CX_Display::hasSwappedSinceLastCheck (void) {
	bool hasSwapped = false;
	uint64_t currentFrameNumber = this->getFrameNumber();
	if (currentFrameNumber != _frameNumberOnLastSwapCheck) {
		_frameNumberOnLastSwapCheck = currentFrameNumber;
		hasSwapped = true;
	}
	return hasSwapped;
}

/*! This function returns the number of the last frame presented, as determined by 
number of front and back buffer swaps. It tracks buffer swaps that result from 
1) the front and back buffer swapping automatically (as a result of BLOCKING_setAutoSwapping() with true as the argument) and 
2) manual swaps resulting from a call to BLOCKING_swapFrontAndBackBuffers() or swapFrontAndBackBuffers().
\return The number of the last frame. This value can only be compared with other values 
returned by this function. */
uint64_t CX_Display::getFrameNumber (void) {
	return _swapThread->getFrameNumber() + _manualBufferSwaps;
}

/*! Copies an ofFbo to the back buffer using a highly efficient blitting operation. This copies
over the contents of the back buffer, it does not draw over them. For this reason, transparaency
is ignored.
\param fbo The framebuffer to copy. It will be drawn starting from (0, 0) and will be drawn at
the full dimensions of the fbo (whatever size was chosen at allocation of the fbo). 
*/
void CX_Display::copyFboToBackBuffer(ofFbo &fbo) {
	copyFboToBackBuffer(fbo, ofPoint(0, 0));
}

/*! Copies an ofFbo to the back buffer using a highly efficient blitting operation.
\param fbo The framebuffer to copy.
\param destination The point on the back buffer where the fbo will be placed.
*/
void CX_Display::copyFboToBackBuffer(ofFbo &fbo, ofPoint destination) {

	ofRectangle res = this->getResolution();

	GLint copyWidth = std::min(fbo.getWidth(), res.width); //Dimensions must be the same
	GLint copyHeight = std::min(fbo.getHeight(), res.height);

	ofRectangle source(0, 0, copyWidth, copyHeight);
	ofRectangle dest(destination.x, destination.y, copyWidth, copyHeight);

	_blitFboToBackBuffer(fbo, source, dest);
}

/*! Copies an ofFbo to the back buffer using a highly efficient blitting operation.
\param fbo The framebuffer to copy.
\param source A rectangle giving an area of the fbo to copy.
\param destination The point on the back buffer where the area of the fbo will be placed.

If this function does not provide enough flexibility, you can always draw ofFbo's using the following
technique:
\code{.cpp}
Display.beginDrawingToBackBuffer();
ofSetColor( 255 );
fbo.draw( x, y, width, height ); //Replace these variables with the destination location (x,y) and dimensions of the 
Display.endDrawingToBackBuffer();
\endcode
*/
void CX_Display::copyFboToBackBuffer(ofFbo &fbo, ofRectangle source, ofPoint destination) {
	ofRectangle dest(destination.x, destination.y, source.width, source.height);

	_blitFboToBackBuffer(fbo, source, dest);
}



void CX_Display::_blitFboToBackBuffer(ofFbo& fbo, ofRectangle sourceCoordinates, ofRectangle destinationCoordinates) {

	ofRectangle res = this->getResolution();

	GLint sx0 = sourceCoordinates.x;
	GLint sy0 = fbo.getHeight() - sourceCoordinates.y;
	GLint sx1 = sourceCoordinates.x + sourceCoordinates.width;
	GLint sy1 = fbo.getHeight() - sourceCoordinates.y - sourceCoordinates.height;

	GLint dx0 = destinationCoordinates.x;
	GLint dy0 = res.height - destinationCoordinates.y;
	GLint dx1 = destinationCoordinates.x + destinationCoordinates.width;
	GLint dy1 = res.height - destinationCoordinates.y - destinationCoordinates.height;

	ofOrientation orient = ofGetOrientation();

	switch (orient) {
	case OF_ORIENTATION_DEFAULT:
		std::swap(sy0, sy1);
		break;
	case OF_ORIENTATION_180:
		std::swap(sx0, sx1);
		//std::swap(sy0, sy1);
		break;
	case OF_ORIENTATION_90_LEFT:
		//std::swap(y0, y1);
		//break;
	case OF_ORIENTATION_90_RIGHT:
		//std::swap(y0, y1);
		Log.error("CX_Display") << "drawFboToBackBuffer: FBO copy attempted while the orientation was in an unsupported mode."
			" Supported orientations are OF_ORIENTATION_DEFAULT and OF_ORIENTATION_180.";
		break;
	}

	glDrawBuffer(GL_BACK);
	//glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.getFbo());
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GL_BACK);

	glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

/*! Prepares a rendering context for using drawing functions. Must be paired with
a call to endDrawingToBackBuffer(). */
void CX_Display::beginDrawingToBackBuffer (void) {
	if(_renderer){
		_renderer->startRender();
	}

	ofViewport();
	ofSetupScreen();
}

/*! Finish rendering to the back buffer. Must be paired with a call to beginDrawingToBackBuffer(). */
void CX_Display::endDrawingToBackBuffer (void) {
	if(_renderer){
		_renderer->finishRender();
	}
}

/*! This function queues up a swap of the front and back buffers then
blocks until the swap occurs. This usually should not be used if 
`isAutomaticallySwapping() == true`. If it is, a warning will be logged.
\see \ref blockingCode */
void CX_Display::BLOCKING_swapFrontAndBackBuffers (void) {
	if (isAutomaticallySwapping()) {
		Instances::Log.warning("CX_Display") << "Manual buffer swap requested with BLOCKING_swapFrontAndBackBuffers while auto swapping mode was in use.";
	}

	//glFlush();
	glfwSwapBuffers(CX::Private::glfwContext);
	if (_softVSyncWithGLFinish) {
		glFinish();
	}
	_manualBufferSwaps++;

}

/*! This function cues a swap of the front and back buffers. It avoids blocking
(like BLOCKING_swapFrontAndBackBuffers()) by spawning a thread in which the 
swap is waited for. This does not make it obviously better than BLOCKING_swapFrontAndBackBuffers(),
because spawning a thread has a cost and may introduce synchronization problems. Also,
because this function does not block, in order to know when the buffer swap took place,
you need to check hasSwappedSinceLastCheck() in order to know when the buffer swap has taken place. */
void CX_Display::swapFrontAndBackBuffers (void) {
	_swapThread->swapNFrames(1);
}

/*!
Wait until all OpenGL instructions that were given before this was called to complete. 
This can be useful if you are trying to determine how long a set of rendering commands takes
or need to make sure that all rendering is complete before moving on. 
\see \ref blockingCode
*/
void CX_Display::BLOCKING_waitForOpenGL (void) {
	glFinish();
	/*
	if (CX::Private::glFenceSyncSupported()) {
		GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glFlush();
		glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
	} else {
		glFinish(); //I don't see why not to just use glFinish()
	}
	*/
}

/*! Returns the resolution of the current window, not the resolution of the monitor (unless you are in full screen mode).
\return An ofRectangle containing the resolution. The width in pixels is stored in both the width
and x members and the height in pixles is stored in both the height and y members, so you can
use whichever makes the most sense to you. */
ofRectangle CX_Display::getResolution (void) {
	return ofRectangle( ofGetWidth(), ofGetHeight(), ofGetWidth(), ofGetHeight() );
}

/*! Returns an ofPoint representing the center of the display. Works in either windowed or full screen mode. */
ofPoint CX_Display::getCenterOfDisplay (void) {
	ofPoint rval;
	rval.x = getResolution().x/2;
	rval.y = getResolution().y/2;
	return rval;
}

/*!
Sets the resolution of the window. Has no effect if called while in full screen mode.
\param width The desired width of the window, in pixels.
\param height The desired height of the window, in pixels.
*/
void CX_Display::setWindowResolution (int width, int height) {
	if (ofGetWindowMode() == OF_WINDOW) {
		ofSetWindowShape( width, height );
	}
}

/*! Sets the title of the experiment window.
\param title The new window title.
*/
void CX_Display::setWindowTitle(std::string title) {
	CX::Private::window->setWindowTitle(title);
}

/*!
This function estimates the typical period of the display refresh.
This function blocks for estimationInterval while the swapping thread swaps in the background.
This function is called with an argument of 300 ms during construction of this class, so
there will always be some information about the frame period. If more precision of the estimate
is desired, this function can be called again with a longer wait duration.
\param estimationInterval The length of time to spend estimating the frame period.
\see \ref blockingCode
*/
void CX_Display::BLOCKING_estimateFramePeriod(CX_Millis estimationInterval) {
	bool wasSwapping = isAutomaticallySwapping();
	BLOCKING_setAutoSwapping(false);

	vector<CX_Millis> swapTimes;

	//For some reason, frame period estimation gets screwed up because the first few swaps are way too fast
	//if the buffers haven't been swapping for some time, so swap a few times to clear out the "bad" initial swaps.
	for (int i = 0; i < 3; i++) {
		BLOCKING_swapFrontAndBackBuffers();
	}

	CX_Millis startTime = CX::Instances::Clock.now();
	while (CX::Instances::Clock.now() - startTime < estimationInterval) {
		BLOCKING_swapFrontAndBackBuffers();
		swapTimes.push_back(CX::Instances::Clock.now());
	}
	
	if (swapTimes.size() > 1) {

		std::vector<CX_Millis> durations(swapTimes.size() - 1);
		
		for (unsigned int i = 1; i < swapTimes.size(); i++) {
			durations[i - 1] = swapTimes[i] - swapTimes[i - 1];
		}
		_framePeriodStandardDeviation = CX_Millis::standardDeviation(durations);
		_framePeriod = Util::mean(durations);
		
	} else {
		Log.warning("CX_Display") << "BLOCKING_estimateFramePeriod: Not enough swaps occured during the " << estimationInterval << " ms estimation interval.";
	}
	
	BLOCKING_setAutoSwapping(wasSwapping);
}

/*! Set whether the display is full screen or not. If the display is set to full screen, 
the resolution may not be the same as the resolution of display in windowed mode, and vice
versa.
*/
void CX_Display::setFullScreen (bool fullScreen) {
	//ofSetFullscreen( fullScreen );
	Private::window->setFullscreen(fullScreen);
	/*
	if (fullScreen) {
		setVSync(true);
	} else {
		setVSync(false); //No v-sync in windowed mode
	}
	*/
}

/*! \brief Returns true if the display is in full screen mode. */
bool CX_Display::isFullscreen(void) {
	return (CX::Private::window->getWindowMode() == OF_FULLSCREEN);
}

/*! Sets whether the display is using Vsync to control frame presentation.
Without Vsync, vertical tearing can occur.
\param useHardwareVSync If true, "hardware" Vsync will be used.
\param useSoftwareVSync If true, instead of trying to use hardware to control Vsync,
the display will attempt to do Vsync in software.
\note This may not work, depending on your video card settings. Modern video card
drivers allow you to contorl whether Vsync is used for all applications or not,
or whether the applications are allowed to choose from themselves whether to use
Vsync. If your drivers are set to force Vsync to a particular setting, this function
is unlikely to have an effect. Even when the drivers allow applications to choose 
a Vsync setting, it is still possible that this function will have no effect.
\see \ref framebufferSwapping for information on what Vsync is.
*/
void CX_Display::setVSync(bool useHardwareVSync, bool useSoftwareVSync) {
	if (useHardwareVSync) {
		glfwSwapInterval(1);
	} else {
		glfwSwapInterval(0);
	}
	
	_softVSyncWithGLFinish = useSoftwareVSync;
	_swapThread->setGLFinishAfterSwap(useSoftwareVSync);
}

/*! This function tests buffer swapping under various combinations of Vsync setting and whether the swaps
are requested in the main thread or in a secondary thread. The tests combine visual inspection and automated 
time measurement. The visual inspection is important because what the computer is told to put on the screen
and what is actually drawn on the screen are not always the same. It is best to run the tests in full screen
mode, although that is not enforced.

In the resulting CX_DataFrame, there are three columns that give the test conditions. "thread" indicates
whether the main thread or a secondary thread was used. "hardVSync" and "softVSync" indicate whether
hardware or software Vsync were enabled for the test (see \ref setVSync()). Other columns, giving data
from the tests, are explained below.

Continuous swapping test
--------------------------------
This test examines the case of constantly swapping the fron and back buffers. It measures the amount of time
between swaps, which should always approximately equal the frame period. The data from this test are found
in columns of the data frame beginning with "cs": "csDurations" gives the raw between-swap durations, and 
"csDurationMean" and "csDurationStdDev" give the mean and standard deviation of the durations.

During this test, you should see the screen very rapidly flickering between black and white. If you see
slow flickering or a constant color, that is an error.

Wait swap test
--------------------------------
One case that this function checks for is what happens if a swap is requested after a long period of
no swaps being requested. In particular, this function swaps, waits for 2.5 swap periods and then swaps twice in a row.
The idea is that there is a long delay between the first swap (the "long" swap) and the second swap (the "short" swap),
followed by a standard delay before the third swap (the "normal" swap).

There are graded levels of success in this test. Complete success is when the duration of the first swap is 3P, 
where P is the standard swap period, and the duration of both of the second two swaps is 1P. 
Partial success is if the duration of the long swap is ~2.5P, the duration of the short swap is ~.5P, and the duration
of the normal swap is 1P. In this case, the short swap at least gets things back on the right track.
Failure occurs if the short swap duration is ~0P. Mega failure occurs if the normal swap duration is ~0P. In this
case, it is taking multiple repeated swaps in order to regain vertical synchronization, which is unacceptable behavior.

You can visually check these results. During this test, an attempt is made to draw three bars on the left, 
middle, and right of the screen. The left bar is drawn for the long duration, the middle bar for the short duration,
and the right bar for the normal duration.
Complete success results in all three bars flickering (although you still need to check the timing data). 
Partial success results in only the left and right bars flickering with the middle bar location flat black.
For the partial success case, the middle bar is never visible because at the time at which it is swapped in, 
the screen is in the middle of a refresh cycle. When the next refresh cycle starts, then the middle bar can 
start to be drawn to the screen. However, before it has a chance to be drawn, the right rectangle is drawn 
to the back buffer, overwriting the middle bar (or at least, this is my best explanation for why it isn't visible).

The timing data from the wait swap test can be found in columns of the data frame beginning with "ws". "wsLongMean",
"wsShortMean", and "wsNormalMean" are the averages of the long, short, and normal swap durations, respectively.
"wsTotalMean" is the sum of wsLongMean, wsShortMean, and wsNormalMean. But also be sure to check the raw data in 
"wsDurations", which goes along with the duration type in "wsTypes".

The wait swap test is not performed for the secondary thread, because the assumption is that if the secondary
thread is used, in that thread the front and back buffers will be swapped constantly so there will be no wait swaps.

\param desiredTestDuration An approximate amount of time to spend performing the tests.
\param testSecondaryThread If true, buffer swapping from within a secondary thread will be tested.
\return A CX_DataFrame containing timing results from the tests.

\note This function blocks for approximately `desiredTestDuration` or more. See \ref blockingCode.
*/
CX_DataFrame CX_Display::testBufferSwapping(CX_Millis desiredTestDuration, bool testSecondaryThread) {

	//lamdas
	auto drawScreenData = [](CX_Display* display, ofColor color, string information) {
		display->beginDrawingToBackBuffer();
		ofBackground(color);
		ofDrawBitmapStringHighlight(information, ofPoint(100, 50), ofColor::black, ofColor::white);
		display->endDrawingToBackBuffer();
	};

	auto drawWaitSwapScreenData = [](CX_Display* display, ofColor background, ofColor rectColor, ofRectangle rect, string information) {
		display->beginDrawingToBackBuffer();
		ofBackground(background);
		ofSetColor(rectColor);
		ofRect(rect);
		ofDrawBitmapStringHighlight(information, ofPoint(100, 50), ofColor::black, ofColor::white);
		display->endDrawingToBackBuffer();
	};


	bool wasSwapping = isAutomaticallySwapping();

	CX_Millis testSegmentDuration = desiredTestDuration / 12; //8 continuous swapping tests, but only 4 wait swap tests
	testSegmentDuration *= testSecondaryThread ? 1 : 1.5; //If not doing the secondary thread, make everything else go longer.

	CX_DataFrame data;

	for (int thread = (testSecondaryThread ? 0 : 1); thread < 2; thread++) {

		bool mainThread = (thread == 1);
		BLOCKING_setAutoSwapping(!mainThread);

		for (int hardV = 0; hardV < 2; hardV++) {
			for (int softV = 0; softV < 2; softV++) {
				CX_DataFrameRow row;
				row["thread"] = mainThread ? "main" : "secondary";
				row["hardVSync"] = hardV;
				row["hardVSync"] = softV;

				//Configure V-Sync for the current test.
				setVSync(hardV == 1, softV == 1);
		
				std::stringstream ss;
				ss << "Thread: " << row["thread"].toString() << "\nHardV: " << hardV << "\nSoftV: " << softV;
				std::string conditionString = ss.str();

				vector<CX_Millis> swapTimes;

				//////////////////////////////
				// Continuous swapping test //
				//////////////////////////////
				if (mainThread) {
					//In order to give a fair test, each main thread test should start with some swaps.
					for (unsigned int i = 0; i < 5; i++) {
						BLOCKING_swapFrontAndBackBuffers();
					}

					CX_Millis startTime = Clock.now();
					while ((Clock.now() - startTime) < testSegmentDuration) {
						BLOCKING_swapFrontAndBackBuffers();
						swapTimes.push_back(Clock.now());

						drawScreenData(this, (swapTimes.size() % 2) ? ofColor(255) : ofColor(0), "Continuous swapping test\n" + conditionString);
					}

				} else {

					Clock.wait(200);

					CX_Millis startTime = Clock.now();
					while ((Clock.now() - startTime) < testSegmentDuration) {
						if (this->hasSwappedSinceLastCheck()) {
							swapTimes.push_back(this->getLastSwapTime());

							drawScreenData(this, (swapTimes.size() % 2) ? ofColor(255) : ofColor(0), "Continuous swapping test\n" + conditionString);
						}
					}
				}

				vector<CX_Millis> durations(swapTimes.size() - 1);
				for (unsigned int i = 0; i < swapTimes.size() - 1; i++) {
					durations[i] = swapTimes[i + 1] - swapTimes[i];
				}

				row["csDurations"] = durations;
				row["csDurationMean"] = Util::mean(durations);
				row["csDurationStdDev"] = CX_Millis::standardDeviation(durations);

				
				////////////////////
				// Wait swap test //
				////////////////////
				if (mainThread) {
					swapTimes.clear();
					vector<string> durationType;

					ofRectangle resolution = this->getResolution();

					CX_Millis startTime = Clock.now();
					CX_Millis period = Util::mean(durations);
					while ((Clock.now() - startTime) < testSegmentDuration) {

						drawWaitSwapScreenData(this, ofColor::black, ofColor::white, 
											   ofRectangle(0, 0, resolution.width / 3, resolution.height),
											   "Wait swap test\n" + conditionString);
						BLOCKING_swapFrontAndBackBuffers();
						swapTimes.push_back(Clock.now());
						durationType.push_back("long");


						drawWaitSwapScreenData(this, ofColor::black, ofColor::white,
											   ofRectangle(resolution.width / 3, 0, resolution.width / 3, resolution.height),
											   "Wait swap test\n" + conditionString);

						Clock.wait(period * 2.5);

						BLOCKING_swapFrontAndBackBuffers();
						swapTimes.push_back(Clock.now());
						durationType.push_back("short");

						drawWaitSwapScreenData(this, ofColor::black, ofColor::white,
											   ofRectangle(resolution.width * 2 / 3, 0, resolution.width / 3, resolution.height),
											   "Wait swap test\n" + conditionString);
						BLOCKING_swapFrontAndBackBuffers();
						swapTimes.push_back(Clock.now());
						durationType.push_back("normal");
					}

					durationType.pop_back(); //Make it so that durations lines up with swapType in terms of length.

					durations.resize(swapTimes.size() - 1);
					for (unsigned int i = 0; i < swapTimes.size() - 1; i++) {
						durations[i] = swapTimes[i + 1] - swapTimes[i];
					}

					row["wsDurations"] = durations;
					row["wsType"] = durationType;

					CX_Millis longSwapSum = 0;
					unsigned int longSwapCount = 0;
					CX_Millis shortSwapSum = 0;
					unsigned int shortSwapCount = 0;
					CX_Millis normalSwapSum = 0;
					unsigned int normalSwapCount = 0;

					for (unsigned int i = 0; i < durationType.size(); i++) {
						if (durationType[i] == "long") {
							longSwapSum += durations[i];
							longSwapCount++;
						} else if (durationType[i] == "short") {
							shortSwapSum += durations[i];
							shortSwapCount++;
						} else if (durationType[i] == "normal") {
							normalSwapSum += durations[i];
							normalSwapCount++;
						}
					}

					row["wsLongMean"] = longSwapSum / longSwapCount;
					row["wsShortMean"] = shortSwapSum / shortSwapCount;
					row["wsNormalMean"] = normalSwapSum / normalSwapCount;
					row["wsTotalMean"] = row["wsLongMean"].toDouble() + row["wsShortMean"].toDouble() + row["wsNormalMean"].toDouble();
				}

				data.appendRow(row);
			}
		}
	}

	BLOCKING_setAutoSwapping(wasSwapping);

	return data;
}