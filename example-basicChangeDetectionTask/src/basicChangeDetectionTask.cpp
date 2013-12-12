#include "CX_EntryPoint.h"

/*
This example shows how to do a simple change-detection experiment using CX.
The stimuli are colored circles which are presented in a 3X3 matrix.
*/


//This structure stores information about the trials in the experiment.
struct TrialData_t {
	int arraySize;
	vector<ofColor> colors;
	vector<ofPoint> locations;

	bool changeTrial;
	int changedObjectIndex;
	ofColor newColor;

	CX_KeyEvent_t response;
	int64_t responseTime;
	bool responseCorrect;
};

//Function declarations (could be in a header file, if preferred).
vector<TrialData_t> generateTrials (int trialCount);

void drawFixation (void);
void drawBlank (void);
void drawSampleArray (const TrialData_t &tr);
void drawTestArray (const TrialData_t &tr);

//Global variables (at least from the perspective of the experiment)
int objectSize = 60;
ofColor backgroundColor(50);

vector<ofColor> objectColors;
vector<ofPoint> objectLocations;

int trialIndex = 0;
vector<TrialData_t> trials;

string trialPhase = "drawStimuli";


void setupExperiment (void) {

	//Set up a vector of colors that will be sampled to make the objects.
	objectColors.push_back( ofColor(255,0,0) );
	objectColors.push_back( ofColor(0,255,0) );
	objectColors.push_back( ofColor(0,0,255) );
	objectColors.push_back( ofColor(255,255,0) );
	objectColors.push_back( ofColor(255,0,255) );
	objectColors.push_back( ofColor(0,255,255) );

	//Make a 3x3 grid of object locations around the center of the screen.
	ofPoint screenCenter( Display.getResolution().x/2, Display.getResolution().y/2 );
	for (int i = 0; i < 9; i++) {
		int col = i % 3;
		int row = i / 3;

		ofPoint p;
		p.x = screenCenter.x - 100 + (row * 100);
		p.y = screenCenter.y - 100 + (col * 100);

		objectLocations.push_back( p );
	}

	//Generate 8 trials
	trials = generateTrials(8);

	Input.setup(true, false); //Use the keyboard for this experiment, not the mouse.

	cout << "Press \'s\' for same, \'d\' for different. Press escape to quit." << endl;
}

/*
updateExperiment is where most of the experiment takes place. It is critical that
the code you put into updateExperiment does not block (i.e. you cannot call functions
like ofSleepMillis() that prevent program execution for a long amount of time). This is
possibly the largest downside to CX, which forces program flow to be nonlinear.
In most psychology experiment software, program flow is linear, which is to say
that you can do, e.g.:

drawThing1();
sleep(1000000); //Sleep for 1 sec (argument in microseconds)
drawThing2();
waitForResponse();

In CX, you can't just sleep whenever you want because the back end code needs to work,
which can only happen if your code returns from updateExperiment quickly. In CX you have
to do something more like this (within updateExperiment):

void updateExperiment (void) {
	if (phase == "draw thing1") {
		drawThing1();

		presentThing2Time = Clock.getTime() + 1000000; //Present thing2 1000000 microseconds from now.
		phase = "draw thing2";
	}
	if (phase == "draw thing2") {
		if (Clock.getTime() <= presentThing2Time) {
			drawThing2();
		}
		phase == "get response";
	}
	if (phase == "get response") {
		checkForResponse();
		phase = "draw thing1";
	}
}

There is an abstraction which reduces the pain associated with this design pattern, called CX_TrialController.
*/
void updateExperiment (void) {
	if (trialPhase == "drawStimuli") {
		//At this phase of the experiment, we want to draw all of our stimuli for the coming trial. POFL!

		//The SlidePresenter is a practically essential abstraction that is responsible for
		//displaying visual stimuli for specified durations. 

		//Start by clearing all slides (from the last trial).
		SlidePresenter.clearSlides();

		//To draw to a slide, call beginDrawingNextSlide() with the name of the slide and the duration
		//that you want the contents of the slide to be presented for. The time unit used in CX is
		//microseconds (10^-6 seconds; 10^-3 milliseconds), with no exceptions.
		SlidePresenter.beginDrawingNextSlide(1000000, "fixation");
		//After calling beginDrawingNextSlide(), all drawing commands will be directed to the current
		//slide until beginDrawingNextSlide() is called again or endDrawingCurrentSlide() is called.
		drawFixation();
	
		SlidePresenter.beginDrawingNextSlide(250000, "blank");
		drawBlank();

		SlidePresenter.beginDrawingNextSlide(500000, "sample");
		drawSampleArray( trials.at( trialIndex ) );

		SlidePresenter.beginDrawingNextSlide(1000000, "maintenance");
		drawBlank();

		//The duration given for the last slide must be > 0, but is otherwise ignored.
		//The last slide has an infinite duration: Once it is presented, it will stay
		//on screen until something else is drawn (i.e. the slide presenter does not
		//remove it from the screen after the duration is complete). If this is confusing
		//to you, consider the question of what the slide presenter should replace the
		//last frame with that will be generally (i.e. in all cases) correct.
		SlidePresenter.beginDrawingNextSlide(1, "test");
		drawTestArray( trials.at( trialIndex ) );
		SlidePresenter.endDrawingCurrentSlide(); //After drawing the last slide, it is good form to call endDrawingCurrentSlide().

		SlidePresenter.startSlidePresentation(); //Once all of the slides are ready to go for the next trial,
			//call startSlidePresentation() to do just that. The drawn slides will be drawn on the screen for
			//the specified duration.

		trialPhase = "presentStimuli";
	}
	if (trialPhase == "presentStimuli") {
		//Check that the slide presenter is still at work (i.e. not yet on the last slide).
		//As soon as the last slide is presented, isPresentingSlides() will return false.
		if (!SlidePresenter.isPresentingSlides()) {
			Input.Keyboard.clearEvents(); //Clear all keyboard responses made during the frame presentation.
			trialPhase = "getResponse";
		}
	}
	if (trialPhase == "getResponse") {
		while (Input.Keyboard.availableEvents() > 0) { //While there are available events, 

			CX_KeyEvent_t keyEvent = Input.Keyboard.getNextEvent(); //get the next event for processing.

			//Only examine key presses (as opposed to key releases or repeats). Everything would probably work just fine without this check.
			if (keyEvent.eventType == CX_KeyEvent_t::PRESSED) {

				//Ignore all responses that are not s or d.
				if (keyEvent.key == 's' || keyEvent.key == 'd') {
				
					trials.at( trialIndex ).response = keyEvent; //Store the raw response event.

					//Figure out the response time. CX does no automatic response time calculation. You have
					//to find out when the stimulus that the participant is responding to was presented. In
					//this case, that is easy to do because the SlidePresenter tracks that information for us.
					//The last slide (given by getSlides().back()) has the slide presentation time stored in
					//the actualSlideOnset member.
					uint64_t testArrayOnset = SlidePresenter.getSlides().back().actualSlideOnset;
					//One you have the onset time of the test array, you can subtract that from the time
					//of the response, giving the "response time" (better known as response latency).
					trials.at( trialIndex ).responseTime = keyEvent.eventTime - testArrayOnset;

					//Code the response. For a lot of keys, you can compare the CX_KeyEvent_t::key to a character literal.
					if ((trials.at( trialIndex ).changeTrial && keyEvent.key == 'd') || (!trials.at( trialIndex ).changeTrial && keyEvent.key == 's')) {
						trials.at(trialIndex).responseCorrect = true;
						cout << "Correct!" << endl;
					} else {
						trials.at(trialIndex).responseCorrect = false;
						cout << "Incorrect" << endl;
					}

					//This trial is now complete, so move on to the next trial, checking to see if 
					//you have completed all of the trials.
					if (++trialIndex >= trials.size()) {
						cout << "Experiment complete: exiting..." << endl;
						ofSleepMillis(3000);
						ofExit();
					}
					trialPhase = "drawStimuli";
				}
			}
		}
	}
}


vector<TrialData_t> generateTrials (int trialCount) {

	vector<TrialData_t> _trials;

	vector<int> changeCounts;
	changeCounts.push_back( trialCount/2 + (trialCount % 2) );
	changeCounts.push_back( trialCount/2 );
	vector<int> changeTrial = CX::intVectorByCount( changeCounts );

	for (int trial = 0; trial < trialCount; trial++) {

		TrialData_t tr;

		tr.arraySize = 4;

		//RNG is an instance of CX_RandomNumberGenerator that is instantiated for you. It is useful for a variety of randomization stuff.
		//This version of shuffleVector() returns a shuffled copy of the argument without changing the argument.
		vector<int> colorIndices = RNG.shuffleVector( CX::intVector(0, objectColors.size() - 1) );
		
		//sample() gives you count integers from the range [lowerBound, upperBound] with or without replacement.
		vector<int> locationIndices = RNG.sample( tr.arraySize, 0, objectLocations.size() - 1, false );

		for (int i = 0; i < tr.arraySize; i++) {
			tr.colors.push_back( objectColors[colorIndices[i]] );
			tr.locations.push_back( objectLocations[locationIndices[i]] );
		}

		tr.changeTrial = changeTrial[trial];
		if (tr.changeTrial) {
			//randomInt() returns an unsigned int from the given range (inclusive).
			//It is strictly unsigned; use randomSignedInt() for signed ints.
			tr.changedObjectIndex = RNG.randomInt(0, tr.arraySize - 1); 
			tr.newColor = objectColors[colorIndices.at(tr.arraySize)];
		} else {
			tr.changedObjectIndex = -1;
			tr.newColor = -1;
		}
		
		_trials.push_back( tr );
	}

	//This version of shuffleVector() (which takes the address of a vector) shuffles the argument, returning nothing.
	RNG.shuffleVector( &_trials );

	return _trials;
}

/*
Drawing stuff in CX just uses built in oF drawing functions.
This section gives some examples of such functions, although there
are many more, including 3D drawing stuff.
*/
void drawFixation (void) {
	ofBackground( ofColor( 50 ) );

	ofSetColor( ofColor( 255 ) );
	ofSetLineWidth( 3 );

	ofPoint centerpoint( Display.getResolution().x/2, Display.getResolution().y/2 );

	ofLine( centerpoint.x - 10, centerpoint.y, centerpoint.x + 10, centerpoint.y );
	ofLine( centerpoint.x, centerpoint.y - 10, centerpoint.x, centerpoint.y + 10 );
}

void drawBlank (void) {
	ofBackground( backgroundColor );
}

void drawSampleArray (const TrialData_t &tr) {
	ofBackground( backgroundColor );

	for (int i = 0; i < tr.colors.size(); i++) {
		ofSetColor( tr.colors.at(i) );
		ofCircle( tr.locations.at(i), objectSize/2 );
	}
}

void drawTestArray (const TrialData_t &tr) {

	vector<ofColor> testColors = tr.colors;
	
	if (tr.changeTrial) {
		testColors.at( tr.changedObjectIndex ) = tr.newColor;
	}
	
	ofBackground( backgroundColor );

	for (int i = 0; i < testColors.size(); i++) {
		ofSetColor( testColors.at(i) );
		ofCircle( tr.locations.at(i), objectSize/2 );
	}
}