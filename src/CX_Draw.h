#pragma once

#include "ofPoint.h"
#include "ofPath.h"
#include "ofTrueTypeFont.h"
#include "ofGraphics.h"

#include "CX_Utilities.h"

namespace CX {

	/*! This namespace contains functions for drawing certain complex stimuli.
	\ingroup video */
	namespace Draw {

		
		struct CX_PathParams_t {

			CX_PathParams_t(void) :
				strokeColor(255),
				fillColor(127),
				filled(true),
				strokeWidth(1),
				rotationAmount(0),
				rotationAxes(0,0,1)
			{}

			void applyTo(ofPath& p) {
				p.setFillColor(fillColor);
				p.setFilled(filled);
				p.setStrokeColor(strokeColor);
				p.setStrokeWidth(strokeWidth);
				p.rotate(rotationAmount, rotationAxes);
			}

			float strokeWidth;
			ofColor strokeColor;
			bool filled;
			ofColor fillColor;

			float rotationAmount;
			ofVec3f rotationAxes;
		};
		

		ofPath arrowToPath(float length, float headOffsets, float headSize, float lineWidth);

		ofPath squircleToPath(double radius, double amount = 0.9);
		//void squircle(ofPoint center, double radius, double rotationDeg = 0, double amount = 0.9);

		ofPath starToPath(int numberOfPoints, double innerRadius, double outerRadius);
		void star(ofPoint center, int numberOfPoints, float innerRadius, float outerRadius,
				  ofColor lineColor, ofColor fillColor, float lineWidth = 1, float rotationDeg = 0);

		void centeredString(int x, int y, std::string s, ofTrueTypeFont &font);
		void centeredString(ofPoint center, std::string s, ofTrueTypeFont &font);

		struct CX_PatternProperties_t {
			CX_PatternProperties_t(void) :
				minValue(0),
				maxValue(255),
				angle(0),
				width(100),
				height(100),
				period(30),
				phase(0),
				maskType(CX_PatternProperties_t::SINE_WAVE),
				apertureType(CX_PatternProperties_t::AP_CIRCLE),
				fallOffPower(std::numeric_limits<double>::min())
			{}

			unsigned char minValue;
			unsigned char maxValue;
			
			double angle;

			double width; //If AP_CIRCLE is used, the diameter of the circle is specified by width
			double height;

			double period;
			double phase;

			double fallOffPower;

			enum {
				SINE_WAVE,
				SQUARE_WAVE,
				TRIANGLE_WAVE
			} maskType;

			enum {
				AP_CIRCLE,
				AP_RECTANGLE
			} apertureType;
		};

		struct CX_GaborProperties_t {
			CX_GaborProperties_t(void) :
			color(255, 255, 255, 255)
			{}

			ofColor color;
			CX_PatternProperties_t pattern;
		};

		ofPixels greyscalePattern(const CX_PatternProperties_t& patternProperties);

		ofPixels gaborToPixels (const CX_GaborProperties_t& properties);
		ofTexture gaborToTexture (const CX_GaborProperties_t& properties);
		void gabor (int x, int y, const CX_GaborProperties_t& properties);
	}
}