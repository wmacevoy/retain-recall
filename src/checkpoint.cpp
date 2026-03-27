#include <map>
#include <list>
#include <iostream>

#include "retain.hpp"

//
// Facts are a map of string key/value pairs that
// support checkpoints.  A checkpoint is an enclosing
// frame that will allow the facts to revert to their
// previous state unless a checkpoint is accepted.
//
// Checkpoints can be nested.
//
class Facts {
private:
    typedef std::pair < std::string , std::string > Pair;
    typedef std::map < std::string , std::string > Data;
    Data data;
    static const std::string EMPTY;

public:
    const std::string& get(const std::string &key) const
    {
        auto i = data.find(key);
        if (i != data.end()) {
            return i->second;
        } else {
            return EMPTY;
        }
    }

private:
    void set_unchecked(const std::string &key, const std::string &value)
    {
        if (value != "") {
            data[key] = value;
        } else {
            data.erase(key);
        }
    }

public:
    class Checkpoint : public retain<Facts::Checkpoint> // ***
    {
    private:
        Facts *facts;
        Data undos;
        bool accepted;
    public:
        Checkpoint(Facts *_facts, bool use = true)
            : retain<Facts::Checkpoint>(this, use),
              facts(use ? _facts : nullptr), accepted(false) // ***
        {
        }

    private:
        void undo(const std::string &key, const std::string &value)
        {
            if (undos.find(key) == undos.end()) undos[key] = value;
        }

    public:
        ~Checkpoint()
        {
            if (facts == nullptr) return; // unused
            if (accepted) {
                // hand undo info to enclosing checkpoint (if any)
                iterator parent = iterator(this); ++parent; // ***
                while (parent != end() && parent->facts != facts) // ***
                    ++parent;
                if (parent != end()) { // ***
                    for (auto &kv : undos) { // ***
                        parent->undo(kv.first, kv.second);
                    }
                }
            } else {
                // apply undo
                for (auto &kv : undos) {
                    facts->set_unchecked(kv.first, kv.second);
                }
            }
        }
        void accept() { accepted = true; }
        void reject() { accepted = false; }
        friend class Facts;
    };

public:
    void set(const std::string &key, const std::string &value)
    {
        std::string old_value = get(key);
        if (old_value != value) {
            Checkpoint::iterator checkpoint = Checkpoint::begin();

            // skip checkpoints unrelated to these facts
            while (checkpoint != Checkpoint::end() && checkpoint->facts != this) // ***
                ++checkpoint;
            if (checkpoint != Checkpoint::end()) { // ***
                checkpoint->undo(key, old_value);
            }
            set_unchecked(key, value);
        }
    }

    void print(std::ostream &out) const
    {
        bool first = true;
        for (const auto &kv : data) {
            if (first) first = false;
            else out << " ";
            out << kv.first << "=>" << kv.second;
        }
    }
};
const std::string Facts::EMPTY;

std::ostream& operator<< (std::ostream &out, const Facts &facts)
{
    facts.print(out);
    return out;
}

//
// use (checkpoints reject by default)
//
int main()
{
    Facts facts;

    facts.set("sky", "pink");
    std::cout << facts << std::endl;
    { Facts::Checkpoint checkpoint(&facts);
        facts.set("sky", "yellow");
        std::cout << facts << std::endl;
    }
    std::cout << facts << std::endl;
    { Facts::Checkpoint checkpoint(&facts);
        facts.set("sky", "blue");
        checkpoint.accept();
    }
    std::cout << facts << std::endl;

    { Facts::Checkpoint checkpoint(&facts);
        facts.set("sky", "white");
        { Facts::Checkpoint checkpoint(&facts);
            facts.set("water", "dry");
            std::cout << facts << std::endl;
        }
        std::cout << facts << std::endl;

        { Facts::Checkpoint checkpoint(&facts);
            facts.set("water", "wet");
            std::cout << facts << std::endl;
            checkpoint.accept();
        }
        std::cout << facts << std::endl;
    }

    std::cout << facts << std::endl;
    return 0;
}
