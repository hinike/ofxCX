#include "CX_Logger.h"

//using namespace std;


/*! This is an instance of CX::CX_Logger that is hooked into the CX backend.
All log messages generated by CX and openFrameworks go through this instance.
\ingroup entryPoint
*/
CX::CX_Logger CX::Instances::Log;

namespace CX {
namespace Private {

	enum class LogTarget {
		CONSOLE,
		FILE,
		CONSOLE_AND_FILE
	};

	struct CX_LoggerTargetInfo {
		CX_LoggerTargetInfo(void) :
			file(nullptr)
		{}

		LogTarget targetType;
		CX_LogLevel level;

		std::string filename;
		ofFile *file;
	};

	struct CX_LogMessage {
		CX_LogMessage(CX_LogLevel level_, string module_) :
			level(level_),
			module(module_)
		{}

		std::shared_ptr<std::stringstream> message;
		CX_LogLevel level;
		std::string module;
		std::string timestamp;
	};

	struct CX_ofLogMessageEventData_t {
		ofLogLevel level;
		std::string module;
		std::string message;
	};

	class CX_LoggerChannel : public ofBaseLoggerChannel {
	public:
		~CX_LoggerChannel(void) {};

		void log(ofLogLevel level, const std::string & module, const std::string & message);
		void log(ofLogLevel level, const std::string & module, const char* format, ...);
		void log(ofLogLevel level, const std::string & module, const char* format, va_list args);

		ofEvent<CX_ofLogMessageEventData_t> messageLoggedEvent;
	};

	void CX_LoggerChannel::log(ofLogLevel level, const std::string & module, const std::string & message) {
		CX_ofLogMessageEventData_t md;
		md.level = level;
		md.module = module;
		md.message = message;
		ofNotifyEvent(this->messageLoggedEvent, md);
	}

	void CX_LoggerChannel::log(ofLogLevel level, const std::string & module, const char* format, ...) {
		va_list args;
		va_start(args, format);
		this->log(level, module, format, args);
		va_end(args);
	}

	void CX_LoggerChannel::log(ofLogLevel level, const std::string & module, const char* format, va_list args) {
		int bufferSize = 256;

		char *buffer = new char[bufferSize];

		while (true) {

			int result = vsnprintf(buffer, bufferSize - 1, format, args);
			if ((result > 0) && (result < bufferSize)) {
				this->log(level, module, std::string(buffer));
				break;
			}

			bufferSize *= 4;
			if (bufferSize > 17000) { //Largest possible is 16384 chars.
				this->log(ofLogLevel::OF_LOG_ERROR, "CX_LoggerChannel", "Could not convert formatted arguments: "
						  "Resulting message would have been too long.");
				break;
			}
		}

		delete[] buffer;
	}

} //namespace Private



CX_Logger::CX_Logger(void) :
	_flushCallback(nullptr),
	_logTimestamps(false),
	_timestampFormat("%H:%M:%S"),
	_defaultLogLevel(CX_LogLevel::LOG_NOTICE)
{
	levelForConsole(CX_LogLevel::LOG_ALL);

	_ofLoggerChannel = ofPtr<Private::CX_LoggerChannel>(new Private::CX_LoggerChannel);

	ofAddListener(_ofLoggerChannel->messageLoggedEvent, this, &CX_Logger::_loggerChannelEventHandler);

	levelForAllModules(CX_LogLevel::LOG_ERROR);
}

CX_Logger::~CX_Logger(void) {
	ofRemoveListener(_ofLoggerChannel->messageLoggedEvent, this, &CX_Logger::_loggerChannelEventHandler);

	flush();
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == Private::LogTarget::FILE) {
			_targetInfo[i].file->close();
			delete _targetInfo[i].file;
		}
	}
}

/*! Log all of the messages stored since the last call to flush() to the
selected logging targets. This is a blocking operation, because it may take
quite a while to output all log messages to various targets (see \ref blockingCode).
\note This function is not 100% thread-safe: Only call it from the main thread. */
void CX_Logger::flush(void) {

	unsigned int messageCount = _messageQueue.size();
	if (messageCount == 0) {
		return;
	}

	//Open output files
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == Private::LogTarget::FILE) {
			_targetInfo[i].file->open(_targetInfo[i].filename, ofFile::Append, false);
			if (!_targetInfo[i].file->is_open()) {
				std::cerr << "File " << _targetInfo[i].filename << " not opened for logging." << std::endl;
			}
		}
	}


	for (unsigned int i = 0; i < messageCount; i++) {
		const Private::CX_LogMessage& m = _messageQueue[i];

		if (_flushCallback) {
			CX_MessageFlushData dat(m.message->str(), m.level, m.module);
			_flushCallback(dat);
		}

		std::string logName = _getLogLevelName(m.level);
		logName.append(max<int>((int)(7 - logName.size()), 0), ' '); //Pad out names to 7 chars
		std::string formattedMessage;
		if (_logTimestamps) {
			formattedMessage += m.timestamp + " ";
		}

		formattedMessage += "[ " + logName + " ] ";

		if (m.module != "") {
			formattedMessage += "<" + m.module + "> ";
		}

		*m.message << std::endl;

		formattedMessage += m.message->str();

		if (m.level >= _moduleLogLevels[m.module]) {
			for (unsigned int i = 0; i < _targetInfo.size(); i++) {
				if (m.level >= _targetInfo[i].level) {
					if (_targetInfo[i].targetType == Private::LogTarget::CONSOLE) {
						cout << formattedMessage;
					} else if (_targetInfo[i].targetType == Private::LogTarget::FILE) {
						*_targetInfo[i].file << formattedMessage;
					}
				}
			}
		}
	}

	//Close output files
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == Private::LogTarget::FILE) {
			_targetInfo[i].file->close();
		}
	}

	//Delete printed messages
	_messageQueueMutex.lock();
	_messageQueue.erase(_messageQueue.begin(), _messageQueue.begin() + messageCount);
	_messageQueueMutex.unlock();
}

/*! \brief Clear all stored log messages. */
void CX_Logger::clear(void) {
	_messageQueueMutex.lock();
	_messageQueue.clear();
	_messageQueueMutex.unlock();
}

/*! \brief Set the log level for messages to be printed to the console. */
void CX_Logger::levelForConsole(CX_LogLevel level) {
	bool consoleFound = false;
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == Private::LogTarget::CONSOLE) {
			consoleFound = true;
			_targetInfo[i].level = level;
		}
	}

	if (!consoleFound) {
		Private::CX_LoggerTargetInfo consoleTarget;
		consoleTarget.targetType = Private::LogTarget::CONSOLE;
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
	if (filename == "CX_LOGGER_DEFAULT") {
		filename = "Log file " + CX::Instances::Clock.getExperimentStartDateTimeString("%Y-%b-%e %h-%M-%S %a") + ".txt";
	}
	filename = "logfiles/" + filename;
	filename = ofToDataPath(filename);

	bool fileFound = false;
	unsigned int fileIndex = -1;
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if ((_targetInfo[i].targetType == Private::LogTarget::FILE) && (_targetInfo[i].filename == filename)) {
			fileFound = true;
			fileIndex = i;
			_targetInfo[i].level = level;
		}
	}

	//If nothing is to be logged, either delete or never create the target
	if (level == CX_LogLevel::LOG_NONE) {
		if (fileFound) {
			_targetInfo.erase(_targetInfo.begin() + fileIndex);
		}
		return;
	}

	if (!fileFound) {
		Private::CX_LoggerTargetInfo fileTarget;
		fileTarget.targetType = Private::LogTarget::FILE;
		fileTarget.level = level;
		fileTarget.filename = filename;
		fileTarget.file = new ofFile(); //This is deallocated in the dtor

		fileTarget.file->open(filename, ofFile::Reference, false);
		if (fileTarget.file->exists()) {
			std::cerr << "Log file already exists with name: " << filename << ". It will be overwritten." << std::endl;
		}

		fileTarget.file->open(filename, ofFile::WriteOnly, false);
		if (fileTarget.file->is_open()) {
			std::cout << "Log file \"" + filename + "\" opened" << std::endl;
		}
		*fileTarget.file << "CX log file. Created " << CX::Instances::Clock.getDateTimeString() << std::endl;
		fileTarget.file->close();

		_targetInfo.push_back(fileTarget);
	}
}

/*! Sets the log level for the given module. Messages from that module that are at a lower level than
\ref level will be ignored.
\param level See the CX::CX_LogLevel enum for valid values.
\param module A string representing one of the modules from which log messages are generated.
*/
void CX_Logger::level(CX_LogLevel level, std::string module) {
	_moduleLogLevelsMutex.lock();
	_moduleLogLevels[module] = level;
	_moduleLogLevelsMutex.unlock();
}

/*! Gets the log level in use by the given module.
\param module The name of the module.
\return The CX_LogLevel for `module`. */
CX_LogLevel CX_Logger::getModuleLevel(std::string module) {
	CX_LogLevel level = _defaultLogLevel;
	_moduleLogLevelsMutex.lock();
	if (_moduleLogLevels.find(module) != _moduleLogLevels.end()) {
		level = _moduleLogLevels[module];
	}
	_moduleLogLevelsMutex.unlock();
	return level;
}

/*!
Set the log level for all modules. This works both retroactively and proactively: All currently known modules
are given the log level and the default log level for new modules as set to the level.
*/
void CX_Logger::levelForAllModules(CX_LogLevel level) {
	_moduleLogLevelsMutex.lock();
	_defaultLogLevel = level;
	for (map<string, CX_LogLevel>::iterator it = _moduleLogLevels.begin(); it != _moduleLogLevels.end(); it++) {
		_moduleLogLevels[it->first] = level;
	}
	_moduleLogLevelsMutex.unlock();
}

/*!
Sets the user function that will be called on each message flush event. For every message that has been
logged, the user function will be called. No filtering is performed: All messages regardless of the module
log level will be sent to the user function.
\param f A pointer to a user function that takes a reference to a CX_MessageFlushData struct and returns nothing.
*/
void CX_Logger::setMessageFlushCallback(std::function<void(CX_MessageFlushData&)> f) {
	_flushCallback = f;
}

/*! Set whether or not to log timestamps and the format for the timestamps.
\param logTimestamps Does what it says.
\param format Timestamp format string. See http://pocoproject.org/docs/Poco.DateTimeFormatter.html#4684 for
documentation of the format. Defaults to %H:%M:%S.%i (24-hour clock with milliseconds at the end).
*/
void CX_Logger::timestamps(bool logTimestamps, std::string format) {
	_logTimestamps = logTimestamps;
	_timestampFormat = format;
}

/*! This is the fundamental logging function for this class. Example use:
\code{.cpp}
Log.log(CX_LogLevel::LOG_WARNING, "moduleName") << "Speical message number: " << 20;
\endcode

Possible output: "[warning] <moduleName> Speical message number: 20"

A newline is inserted automatically at the end of each message.

\param level Log level for this message. This has implications for message filtering. See CX::CX_Logger::level().
This should not be LOG_ALL or LOG_NONE, because that would be weird, wouldn't it?
\param module Name of the module that this log message is related to. This has implications for message filtering.
See CX::CX_Logger::level().
\return A reference to a `std::stringstream` that the log message data should be streamed into.
\note This function and all of the trivial wrappers of this function (verbose(), notice(), warning(),
error(), fatalError()) are thread-safe.
*/
std::stringstream& CX_Logger::log(CX_LogLevel level, std::string module) {
	return _log(level, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_VERBOSE, module)`. */
std::stringstream& CX_Logger::verbose(std::string module) {
	return _log(CX_LogLevel::LOG_VERBOSE, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_NOTICE, module)`. */
std::stringstream& CX_Logger::notice(std::string module) {
	return _log(CX_LogLevel::LOG_NOTICE, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_WARNING, module)`. */
std::stringstream& CX_Logger::warning(std::string module) {
	return _log(CX_LogLevel::LOG_WARNING, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_ERROR, module)`. */
std::stringstream& CX_Logger::error(std::string module) {
	return _log(CX_LogLevel::LOG_ERROR, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_FATAL_ERROR, module)`. */
std::stringstream& CX_Logger::fatalError(std::string module) {
	return _log(CX_LogLevel::LOG_FATAL_ERROR, module);
}

/*! Set this instance of CX_Logger to be the target of any messages created by openFrameworks logging functions.
This function is called during CX setup for CX::Instances::Log. You do not need to call it yourself. */
void CX_Logger::captureOFLogMessages(void) {
	ofSetLoggerChannel(ofPtr<ofBaseLoggerChannel>(this->_ofLoggerChannel));
	ofSetLogLevel(ofLogLevel::OF_LOG_VERBOSE);
}

string CX_Logger::_getLogLevelName(CX_LogLevel level) {
	switch (level) {
	case CX_LogLevel::LOG_VERBOSE: return "verbose";
	case CX_LogLevel::LOG_NOTICE: return "notice";
	case CX_LogLevel::LOG_WARNING: return "warning";
	case CX_LogLevel::LOG_ERROR: return "error";
	case CX_LogLevel::LOG_FATAL_ERROR: return "fatal";
	case CX_LogLevel::LOG_ALL: return "all";
	case CX_LogLevel::LOG_NONE: return "none";
	};
	return "";
}

void CX_Logger::_loggerChannelEventHandler(CX::Private::CX_ofLogMessageEventData_t& md) {
	CX_LogLevel convertedLevel;

	switch (md.level) {
	case ofLogLevel::OF_LOG_VERBOSE: convertedLevel = CX_LogLevel::LOG_VERBOSE; break;
	case ofLogLevel::OF_LOG_NOTICE: convertedLevel = CX_LogLevel::LOG_NOTICE; break;
	case ofLogLevel::OF_LOG_WARNING: convertedLevel = CX_LogLevel::LOG_WARNING; break;
	case ofLogLevel::OF_LOG_ERROR: convertedLevel = CX_LogLevel::LOG_ERROR; break;
	case ofLogLevel::OF_LOG_FATAL_ERROR: convertedLevel = CX_LogLevel::LOG_FATAL_ERROR; break;
	case ofLogLevel::OF_LOG_SILENT: convertedLevel = CX_LogLevel::LOG_NONE; break;
	}

	this->_log(convertedLevel, md.module) << md.message;
}

stringstream& CX_Logger::_log(CX_LogLevel level, string module) {

	_moduleLogLevelsMutex.lock();
	if (_moduleLogLevels.find(module) == _moduleLogLevels.end()) {
		_moduleLogLevels[module] = _defaultLogLevel;
	}
	_moduleLogLevelsMutex.unlock();

	Private::CX_LogMessage temp(level, module);
	temp.message = std::shared_ptr<std::stringstream>(new std::stringstream);

	_messageQueueMutex.lock();
	_messageQueue.push_back(temp);

	if (_logTimestamps) {
		_messageQueue.back().timestamp = CX::Instances::Clock.getDateTimeString(_timestampFormat);
	}
	_messageQueueMutex.unlock();

	return *temp.message;
}

}
