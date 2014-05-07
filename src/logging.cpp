#include <iostream>
#include <fstream>
#include <sstream>
#include "retain.hpp"

//
// setup
//

class StreamRetain : public retain<StreamRetain> { // ***
public:
  StreamRetain(bool use=true)  : retain<StreamRetain>(this,use) {} // ***
  virtual void stream(const std::string &message) {}
  virtual ~StreamRetain() {}
};
typedef StreamRetain NullStreamRetain;

class AddStreamRetain : public StreamRetain {
public:
  std::ostream &out;
  AddStreamRetain(std::ostream &_out, bool use=true) : StreamRetain(use), out(_out) {}
  virtual void stream(const std::string &message) 
  {
    out << message << std::endl;

    // stream to deeper stream retains as well
    iterator p(this); ++p; // ***
    if (p != end()) p->stream(message); // ***
  }
};

#define STREAM(MSG) if (retained<StreamRetain>()) { \
                      std::ostringstream __osstmp__; \
                      __osstmp__ << MSG; \
		      recall<StreamRetain>()->stream(__osstmp__.str()); \
                    }

#define LOG(MSG) STREAM(__FILE__ << " " << __LINE__ << ": " << MSG)


//
// use
//

void sing()
{
  STREAM("doe ray me");
}


int main()
{ AddStreamRetain as(std::cerr); // ***
  LOG("start");

  std::string logFileName="tmp/messages.log";
  std::ofstream logFile(logFileName.c_str());
  bool logFileOk = !!logFile;

  { AddStreamRetain as_if(logFile,logFileOk); // ***
    LOG("open log file '" << logFileName << "': " << (logFileOk ? "ok" : "failed"));
      

    std::string songFileName="tmp/song.out";
    std::ofstream songFile(songFileName.c_str());
    bool songFileOk = !!songFile;
    LOG("open song file '" << songFileName << "': " << (songFileOk ? "ok" : "failed"));

    std::stringstream sout;

    { NullStreamRetain as; // stop precursion to ancestor retains
      { AddStreamRetain as_if(songFile,songFileOk); // ***
        { AddStreamRetain as(sout); // ***
           sing();
        }
      }
    }
    LOG("sung: " << sout.str());
  }

  LOG("finish");
  return 0;
}
