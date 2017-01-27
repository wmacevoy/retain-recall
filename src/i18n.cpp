#include <iostream>
#include <string>
#include <map>
#include <string.h>

#include "retain.hpp"

//
// setup
//

class Language {
public:
  typedef std::map < std::string , std::string > Translations;
  Translations translations;
  public: const std::string& translate(const std::string &word) const { 
    Translations::const_iterator i=translations.find(word);
    if (i != translations.end()) return i->second;
    else return word;
  };
};

Language NO_LANGUAGE;

class EnglishLanguage : public Language
{
public: EnglishLanguage()
  {
    translations["hello"]="good day";
    translations["world"]="earth";
  }
};
EnglishLanguage ENGLISH_LANGUAGE;

class SpanishLanguage : public Language
{
public: SpanishLanguage()
  {
    translations["hello"]="hola";
    translations["world"]="mundo";
  }
};
SpanishLanguage SPANISH_LANGUAGE;

//
// use
// 
const std::string& t(const std::string &word) 
{
  return recall<Language>()->translate(word); // ***
}

// set current language retain-as to english
void english()
{
  recall<Language>()=&ENGLISH_LANGUAGE; // ***
}
  

// set current language retain-as to spanish
void spanish()
{
  recall<Language>()=&SPANISH_LANGUAGE; // ***
}

void greet()
{
  std::cout << t("hello") << " " << t("world") << std::endl;
}

void hello()
{
  retain<Language> as(&ENGLISH_LANGUAGE); // ***
  greet();
}

void hola()
{
  retain<Language> as(&SPANISH_LANGUAGE); // ***
  greet();
}

int main(int argc, char *argv[])
{
  retain<Language> as(&NO_LANGUAGE); // ***
  for (int argi=1; argi<argc; ++argi) {
    if (strcmp(argv[argi],"english")==0) { english(); continue; }
    if (strcmp(argv[argi],"spanish")==0) { spanish(); continue; }
    if (strcmp(argv[argi],"greet")==0) { greet(); continue; }
    if (strcmp(argv[argi],"hello")==0) { hello(); continue; }
    if (strcmp(argv[argi],"hola")==0) { hola(); continue; }
    std::cout << 
      "unkown arg[" << argi << "]='" << argv[argi] << "'" << std::endl;
  }

  return 0;
}
