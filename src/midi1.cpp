#include <retain.hpp>


struct midi_channel_info {
 int error_code;
 int channel_id;
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
 midi_channel_info mci;
 mci.error_code=0;
 mci.channel_id=0;
 auto retain<midi_channel_info> as(&mci);
 use_midi();
 if (mci.error_code == 0) { /* ... */ }
}


