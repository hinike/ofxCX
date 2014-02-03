#include "CX_Logger.h"

using namespace std;
using namespace CX;

CX_Logger CX::Instances::Log;
//CX_LoggerChannel CX::Private::ofLoggerChannel;

enum class LogTarget {
	CONSOLE,
	FILE,
	CONSOLE_AND_FILE
};

struct CX::CX_LoggerTargetInfo {
	CX_LoggerTargetInfo (void) :
		file(nullptr)
	{}

	LogTarget targetType;
	CX_LogLevel level;

	string filename;
	ofFile *file;
};


struct CX::CX_LogMessage {
	CX_LogMessage (CX_LogLevel level_, string module_) :
		level(level_),
		module(module_)
	{}

	stringstream* message;
	CX_LogLevel level;
	string module;
	string timestamp;
};


CX_Logger::CX_Logger (void) :
	_logTimestamps(false),
	_flushCallback(nullptr),
	_timestampFormat("%H:%M:%S"),
	_defaultLogLevel(CX_LogLevel::LOG_NOTICE)
{
	levelForConsole(CX_LogLevel::LOG_ALL);
	
	ofAddListener(_ofLoggerChannel.messageLoggedEvent, this, &CX_Logger::_loggerChannelEventHandler);

	levelForAllModules(CX_LogLevel::LOG_ERROR);
}

CX_Logger::~CX_Logger (void) {
	ofRemoveListener(_ofLoggerChannel.messageLoggedEvent, this, &CX_Logger::_loggerChannelEventHandler);

	flush();
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == LogTarget::FILE) {
			_targetInfo[i].file->close();
			delete _targetInfo[i].file;
		}
	}
}

/*! Log all of the messages stored since the last call to flush() to the selected logging targets. This is a BLOCKING operation. */
void CX_Logger::flush (void) {

	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == LogTarget::FILE) {
			_targetInfo[i].file->open(_targetInfo[i].filename, ofFile::Append, false);
			if (!_targetInfo[i].file->is_open()) {
				cerr << "File " << _targetInfo[i].filename << " not opened for logging." << endl;
			}
		}
	}

	for (auto it = _messageQueue.begin(); it != _messageQueue.end(); it++) {
		CX_MessageFlushData dat( it->message->str(), it->level, it->module );
		//ofNotifyEvent(this->messageFlushEvent, dat );
		if (_flushCallback) {
			_flushCallback( dat );
		}

		string logName = _getLogLevelName(it->level);
		logName.append( max<int>((int)(7 - logName.size()), 0), ' ' ); //Pad out names to 7 chars
		string formattedMessage;
		if (_logTimestamps) {
			formattedMessage += it->timestamp + " ";
		}

		formattedMessage += "[ " + logName + " ] ";

		if (it->module != "") {
			formattedMessage += "<" + it->module + "> ";
		}

		*it->message << endl;

		formattedMessage += it->message->str();

		if (it->level >= _moduleLogLevels[it->module]) {
			for (unsigned int i = 0; i < _targetInfo.size(); i++) {
				if (it->level >= _targetInfo[i].level) {
					if (_targetInfo[i].targetType == LogTarget::CONSOLE) {
						cout << formattedMessage;
					} else if (_targetInfo[i].targetType == LogTarget::FILE) {
						*_targetInfo[i].file << formattedMessage;
					}
				}
			}
		}

		delete it->message; //Deallocate message pointer
	}

	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == LogTarget::FILE) {
			_targetInfo[i].file->close();
		}
	}

	_messageQueue.clear();
}

/*! Set the log level for messages to be printed to the console. */
void CX_Logger::levelForConsole(CX_LogLevel level) {
	bool consoleFound = false;
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == LogTarget::CONSOLE) {
			consoleFound = true;
			_targetInfo[i].level = level;
		}
	}

	if (!consoleFound) {
		CX_LoggerTargetInfo consoleTarget;
		consoleTarget.targetType = LogTarget::CONSOLE;
		consoleTarget.level = level;
		_targetInfo.push_back(consoleTarget);
	}
}

/*! Sets the log level for the file with given file name. If the file does not exist, it will be created. 
If the file does exist, it will be overwritten with a warning logged to cerr. 
\param level See the CX_LogLevel enum for valid values.
\param filename Optional. If no file name is given, a file with name generated from a date/time from the start time of the experiment will be used.
*/
void CX_Logger::levelForFile(CX_LogLevel level, std::string filename) {
	if (filename == "CX_DEFERRED_LOGGER_DEFAULT") {
		filename = "Log file " + CX::Instances::Clock.getExperimentStartDateTimeString("%Y-%b-%e %h-%M-%S %a") + ".txt";
	}
	filename = "logfiles/" + filename;
	filename = ofToDataPath(filename); //Testing
	
	bool fileFound = false;
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if ((_targetInfo[i].targetType == LogTarget::FILE) && (_targetInfo[i].filename == filename)) {
			fileFound = true;
			_targetInfo[i].level = level;
		}
	}

	if (!fileFound) {
		CX_LoggerTargetInfo fileTarget;
		fileTarget.targetType = LogTarget::FILE;
		fileTarget.level = level;
		fileTarget.filename = filename;
		fileTarget.file = new ofFile(); //This is deallocated in the dtor

		fileTarget.file->open(filename, ofFile::Reference, false);
		if (fileTarget.file->exists()) {
			cerr << "Log file already exists with name: " << filename << ". It will be overwritten." << endl;
		}
		
		fileTarget.file->open(filename, ofFile::WriteOnly, false);
		if (fileTarget.file->is_open()) {
			cout << "Log file opened" << endl;
		}
		*fileTarget.file << "CX log file. Created " << CX::Instances::Clock.getDateTimeString() << endl;
		fileTarget.file->close();
		
		_targetInfo.push_back(fileTarget);
	}
}

/*! Sets the log level for the given module.
\param level See the CX_LogLevel enum for valid values.
\param module A string representing one of the modules from which log messages are generated.
*/
void CX_Logger::level(CX_LogLevel level, std::string module) {
	_moduleLogLevels[module] = level;
}

/*!
Set the log level for all modules. This works both retroactively and proactively: All currently known modules
are given the log level and the default log level for new modules as set to the level.
*/
void CX_Logger::levelForAllModules(CX_LogLevel level) {
	_defaultLogLevel = level;
	for (map<string, CX_LogLevel>::iterator it = _moduleLogLevels.begin(); it != _moduleLogLevels.end(); it++) {
		_moduleLogLevels[it->first] = level;
	}
}

/*!
Sets the user function that will be called on each message flush event. For every message that has been
logged, the user function will be called. No filtering is performed: All messages regardless of the module
log level will be sent to the user function.
\param f A pointer to a user function that takes a reference to a CX_MessageFlushData struct and returns nothing.
*/
void CX_Logger::setMessageFlushCallback (std::function<void(CX_MessageFlushData&)> f) {
	_flushCallback = f;
}

/*! Set whether or not to log timestamps and the format for the timestamps.
\param logTimestamps Does what it says.
\param format Timestamp format string. See http://pocoproject.org/docs/Poco.DateTimeFormatter.html#4684 for 
documentation of the format. Defaults to %H:%M:%S.%i (24-hour clock with milliseconds at the end).
*/
void CX_Logger::timestamps (bool logTimestamps, std::string format) {
	_logTimestamps = logTimestamps;
	_timestampFormat = format;
}

/*! This is the basic logging function for this class. Example use:
Log.log(CX_LogLevel::LOG_WARNING, "myModule") << "My message number " << 20;
\param level Log level for this message.
\param module Name of the module that this log message is related to.
\return A reference to a std::stringstream that the log message data should be streamed into.
*/
std::stringstream& CX_Logger::log(CX_LogLevel level, std::string module) {
	return _log(level, module);
}

/*! This function is equivalent to a call to log(CX_LogLevel::LOG_VERBOSE, module). */
std::stringstream& CX_Logger::verbose(std::string module) {
	return _log(CX_LogLevel::LOG_VERBOSE, module);
}

/*! This function is equivalent to a call to log(CX_LogLevel::LOG_NOTICE, module). */
std::stringstream& CX_Logger::notice(std::string module) {
	return _log(CX_LogLevel::LOG_NOTICE, module);
}

/*! This function is equivalent to a call to log(CX_LogLevel::LOG_WARNING, module). */
std::stringstream& CX_Logger::warning(std::string module) {
	return _log(CX_LogLevel::LOG_WARNING, module);
}

/*! This function is equivalent to a call to log(CX_LogLevel::LOG_ERROR, module). */
std::stringstream& CX_Logger::error(std::string module) {
	return _log(CX_LogLevel::LOG_ERROR, module);
}

/*! This function is equivalent to a call to log(CX_LogLevel::LOG_FATAL_ERROR, module). */
std::stringstream& CX_Logger::fatalError(std::string module) {
	return _log(CX_LogLevel::LOG_FATAL_ERROR, module);
}

/*! Set this instance of CX_Logger to be the target of any messages created by oF logging functions. */
void CX_Logger::captureOFLogMessages(void) {
	ofSetLoggerChannel(ofPtr<ofBaseLoggerChannel>(dynamic_cast<ofBaseLoggerChannel*>(&this->_ofLoggerChannel)));
}

string CX_Logger::_getLogLevelName (CX_LogLevel level) {
	switch (level) {
	case CX_LogLevel::LOG_VERBOSE: return "verbose";
	case CX_LogLevel::LOG_NOTICE: return "notice";
	case CX_LogLevel::LOG_WARNING: return "warning";
	case CX_LogLevel::LOG_ERROR: return "error";
	case CX_LogLevel::LOG_FATAL_ERROR: return "fatal";
	};
	return "";
}

void CX_Logger::_loggerChannelEventHandler(CX::CX_ofLogMessageEventData_t& md) {
	CX_LogLevel convertedLevel;

	switch (md.level) {
	case ofLogLevel::OF_LOG_VERBOSE: convertedLevel = CX_LogLevel::LOG_VERBOSE; break;
	case ofLogLevel::OF_LOG_NOTICE: convertedLevel = CX_LogLevel::LOG_NOTICE; break;
	case ofLogLevel::OF_LOG_WARNING: convertedLevel = CX_LogLevel::LOG_WARNING; break;
	case ofLogLevel::OF_LOG_ERROR: convertedLevel = CX_LogLevel::LOG_ERROR; break;
	case ofLogLevel::OF_LOG_FATAL_ERROR: convertedLevel = CX_LogLevel::LOG_FATAL_ERROR; break;
		//case ofLogLevel::OF_LOG_SILENT: convertedLevel = CX_LogLevel::LOG_NOTICE; break;
	}

	this->_log(convertedLevel, md.module) << md.message;
}

stringstream& CX_Logger::_log(CX_LogLevel level, string module) {

	if (_moduleLogLevels.find(module) == _moduleLogLevels.end()) {
		_moduleLogLevels[module] = _defaultLogLevel;
	}

	_messageQueue.push_back(CX_LogMessage(level, module));
	_messageQueue.back().message = new stringstream; //Manually allocated: Must deallocate later.

	if (_logTimestamps) {
		_messageQueue.back().timestamp = CX::Instances::Clock.getDateTimeString(_timestampFormat);
	}

	return *(_messageQueue.back().message);
}