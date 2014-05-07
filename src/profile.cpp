#include <iostream>
#include <retain.hpp>

//
// setup
//
class Profile
{
public:
  int adds;
  int calls;
  Profile() { 
    adds=0;
    calls=0;
  }

  // add count calls to all retained profiles
  static void call(int count=1)
  {
    for (retain<Profile>::iterator profile=retain<Profile>::begin(); // ***
	 profile != retain<Profile>::end(); ++profile) 
      {
	profile->calls += count;
      }
  }

  // add count adds to all retained profiles
  static void add(int count=1)
  {
    for (retain<Profile>::iterator profile=retain<Profile>::begin(); // ***
	 profile != retain<Profile>::end(); ++profile) 
      {
	profile->adds += count;
      }
  }
};

std::ostream& operator<< (std::ostream& out, const Profile &profile)
{
  out << "took " << profile.calls << " calls and " << profile.adds << " adds";
  return out;
}


//
// use
//

int fibonacci(int i) {
  Profile::call();
  if (i < 2) return i;
  Profile::add();
  return fibonacci(i-1)+fibonacci(i-2);
}


int main()
{ 
  Profile summary;
  retain<Profile> as(&summary); // ***

  {
    Profile part;
    retain<Profile> as(&part); // ***
    int n=5;
    int ans=fibonacci(n);
    std::cout << "fibonacci(" << n << ")=" << ans << " " << part << std::endl;
  }

  
  {
    Profile part;
    retain<Profile> as(&part); // ***
    int n=10;
    int ans=fibonacci(n);
    std::cout << "fibonacci(" << n << ")=" << ans << " " << part << std::endl;
  }

  std::cout << "summary: " << summary << std::endl;

  return 0;
}
