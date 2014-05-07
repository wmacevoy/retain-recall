#include <set>
#include <string>
#include <utility>
#include <iostream>

//
// setup
//
#include "retain.hpp"

// a graph as a set of (from,to) edge pairs, with each node as a string
struct Graph
{
  typedef std::string Node;
  typedef std::pair < Node , Node > Edge;
  typedef std::set < Edge > Edges;
  Edges edges;

  // add a directed edge to the graph
  void edge(const Node &from, const Node &to) 
  { 
    edges.insert(Edge(from,to)); 
  }

  // print out the current edges
  void print(std::ostream &out) const
  {
    if (!edges.empty()) {
      bool first=true;
      out << "(";
      for (Edges::const_iterator i=edges.begin(); i!=edges.end(); ++i) {
	if (first) first=false;
	else out << ",";
	out << i->first << "-" << i->second;
      }
      out << ")";
    }
  }

};

// print out all edge pairs in a graph
std::ostream& operator<< (std::ostream &out, const Graph &graph)
{
  graph.print(out);
  return out;
}

// add directed edge between nodes for the current retained graph
const Graph::Node& operator-(const Graph::Node &from,const Graph::Node &to) 
{
  recall<Graph>()->edge(from,to); // ***
  return to;
}

//
// use
//

int main()
{
  Graph g;
  Graph h;
  Graph::Node a("a"),b("b"),c("c"),d("d");

  { 
    retain<Graph> as(&g); // ***

    // without retain...recall, the following would be written as:
    // g.edge(a,b);
    // g.edge(b,c);
    // g.edge(a,d);
    // g.edge(d,a);
    
    (a-b-c,a-d,d-a); // directed chains in g
  }

  {
    retain<Graph> as(&h); // ***

    (a-b,c-a); // directed chains in h
  }

  std::cout << "g=" << g << std::endl;
  std::cout << "h=" << h << std::endl;

  return 0;
}
