#ifndef NODE_H
#define NODE_H

#include <vector>

class Node {
private:
    std::string name; // The name of this node 
    std::vector<int> state; // The internal state associated with this node 
    std::vector<int> modifiers; // The value modifiers that will be applied to the agent after going to this node
    
public:
    // Function: Constructor 
    // Description: Initializes the information associated with this node 
    Node(std::string name, std::vector<int> state, std::vector<int> modifiers);
};


#endif //NODE_H
