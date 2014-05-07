#include <retain.hpp>


class midi_channel_info : public retain<midi_channel_info> {
public:
  int error_code;
  int channel_id;

  midi_channel_info(int _channel_id=0, bool _use=true) 
    : retain<midi_channel_info>(this,_use), channel_id(_channel_id), error_code(0) {}

  ~midi_channel_info() {
    if (error_code != 0)
      throw "midi channel error";
  }

};

void use_midi()
{
  if (!retained<midi_channel_info>()) {
    throw "use midi cannot recall midi_channel_info; retain one beforehand.";
  }
   midi_channel_info *mci=recall<midi_channel_info>();
   if (mci->channel_id <= 0) 
     mci->error_code = 1;
   else 
     mci->error_code=0;

   /* ... */
}

int main()
{
 midi_channel_info is(0);
 use_midi();
}


