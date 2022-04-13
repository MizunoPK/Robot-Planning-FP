#include "includes.h"
#include "Simulation.h"
#include "util.h"
#include <fstream>
#include <vector>

Simulation::Simulation(std::string graphFile, std::string agentFile) {
    initializeStateSpace(graphFile);
    initializeAgent(agentFile);

    // Debugging Prints
    this->ss->printNodes();
    this->agent->printAgentInfo();
}

void Simulation::initializeStateSpace(std::string graphFile) {
    // Process the Graph File
    std::fstream graphFileStream(graphFile);
    if (graphFileStream.is_open()) {
        std::string fileLine;

        // Get the bounds
        std::getline(graphFileStream, fileLine);
        std::vector<int> x_bounds = splitIntList(fileLine);
        std::getline(graphFileStream, fileLine);
        std::vector<int> y_bounds = splitIntList(fileLine);
        this->ss = std::make_shared<StateSpace>(x_bounds, y_bounds);

        while (graphFileStream) {
            std::getline(graphFileStream, fileLine);
            std::vector<std::string> lineVector = split(fileLine, ' ');

            // Get node coords
            std::string coordsString = lineVector.at(0);
            std::vector<int> coords = splitIntList(coordsString);
            
            // Get node internal state
            std::vector<double> state = splitDoubleList(lineVector.at(1));

            // Get node agent-modifiers
            std::vector<double> modifiers = splitDoubleList(lineVector.at(2));

            // Make a node and add it to the state space graph
            ss->setNode(coordsString, std::make_shared<Node>(coords, state, modifiers));
        }
        graphFileStream.close();
    }
    else {
        ERROR << "Could not open Node File \"" << graphFile << "\"" << ENDL;
        exit(-1);
    }
}

void Simulation::initializeAgent(std::string agentFile) {
    // Process the Agent File
    std::fstream agentFileStream(agentFile);
    if (agentFileStream.is_open()) {
        std::string fileLine;
        // The agent file should always have 4 lines of relevent data
        // First line is actor state:
        std::getline(agentFileStream, fileLine);
        std::vector<double> actorState = splitDoubleList(fileLine);

        // Second Line is Starting Node:
        std::getline(agentFileStream, fileLine);
        std::shared_ptr<Node> startingNode = this->ss->getNode(splitIntList(fileLine));

        // Third Line is Primary Goal:
        std::getline(agentFileStream, fileLine);
        std::shared_ptr<Node> primaryGoal = this->ss->getNode(splitIntList(fileLine));

        // Fourth Line is Secondary Goals:
        std::getline(agentFileStream, fileLine);
        std::vector<std::shared_ptr<Node>> secondaryGoals;
        if (fileLine != "NULL") {
            std::vector<std::string> nodeStrs = split(fileLine, '-');
            for ( int i=0; i < nodeStrs.size(); i++ ) {
                std::shared_ptr<Node> goal = this->ss->getNode(splitIntList(nodeStrs.at(i)));
                secondaryGoals.push_back(goal);
            }
        }
        
        // Make the agent
        agent = std::make_shared<Agent>(actorState, startingNode, primaryGoal, secondaryGoals);
        agentFileStream.close();
    }
    else {
        ERROR << "Could not open Agent File \"" << agentFile << "\"" << ENDL;
        exit(-1);
    }
}

void Simulation::runSearch() {

    // define OPEN - priority queue of nodes ready to be evaluated
    std::vector<std::shared_ptr<SearchNode>> openQueue;

    // define CLOSED - a set of nodes already evaluated
    //               - defined as a map associating a Node -> SearchNode ... 
    //                  Makes it easier to find a SearchNode and ensure only one SearchNode exists per Node
    std::unordered_map<std::shared_ptr<Node>, std::shared_ptr<SearchNode>> closedMap;

    // add the start node to OPEN
    std::weak_ptr<SearchNode> w;
    openQueue.push_back(std::make_shared<SearchNode>(agent->getStartingNode(), 0, 0, w));

    // loop through the search
    while (true) {
        if ( LOGGING_LEVEL > 4 ) {
            std::string openString = "";
            for ( int i = openQueue.size() - 1; i >= 0; i-- ) {
                openString = openString + "(" + getCoordString(openQueue.at(i)->node->getCoord()) + ") ";
            }
            TRACE << "Current OPEN queue: " << openString << ENDL;
        }

        std::shared_ptr<SearchNode> current = openQueue.back(); // current = node in OPEN with the lowest f_cost
        openQueue.pop_back(); // remove current from OPEN
        closedMap[current->node] = current; // add current to CLOSED
        bool sortOPEN = false; // if we make any changes that need us to re-sort open, update this flag

        TRACE << "Popped node " << getCoordString(current->node->getCoord()) << " from OPEN" << ENDL;

        // if current is the target node - the path has been found
        if ( current->node == agent->getPrimaryGoal() ) {
            TRACE << "PRIMARY GOAL FOUND... ENDING SEARCH" << ENDL;
            break;
        }
        
        // foreach neighbor of the current node
        std::vector<std::shared_ptr<Node>> neighbors = this->ss->getAdjacencyList(current->node);
        for ( int i=0; i < neighbors.size(); i++ ) {
            std::shared_ptr<Node> neighbor = neighbors.at(i);
            // if neighbor is in CLOSED
            if ( closedMap.find(neighbor) != closedMap.end() ) {
                // skip to the next neighbor
                TRACE << "This neighbor is already in CLOSED... Skipping" << ENDL;
                continue;
            }
            TRACE << "Checking Neighbor: " << getCoordString(neighbor->getCoord()) << ENDL;

            // Calculate the cost of the path to this neighbor
            // g_cost = distance from starting node
            double g_cost = current->g_cost + neighbor->getWeight();
            TRACE << "G Cost: " << g_cost << ENDL;
            // h_cost = heuristic calculated distance from end node - manhatten distance
            std::vector<int>* neighborCoord = neighbor->getCoord();
            std::vector<int>* goalCoord = this->agent->getPrimaryGoal()->getCoord();
            double h_cost = abs(neighborCoord->at(0) - goalCoord->at(0)) + abs(neighborCoord->at(1) - goalCoord->at(1));
            TRACE << "H Cost: " << h_cost << ENDL;
            // f_cost = g_cost + h_cost ... The total cost of the node we use to determine if we want to move there
            double f_cost = h_cost + g_cost;
            TRACE << "F Cost: " << f_cost << ENDL;

            // Determine the location of the neighbor in OPEN
            int neighbor_i = -1; // -1 index indiciates the neighbor is not in OPEN
            for ( int j=0; j < openQueue.size(); j++ ) {
                if ( openQueue.at(j)->node == neighbor ) {
                    neighbor_i = j;
                    break;
                }
            }

            // if neighbor is not in OPEN - add it
            if ( neighbor_i == -1 ) {
                openQueue.push_back(std::make_shared<SearchNode>(neighbor, g_cost, f_cost, current));
                sortOPEN = true;
                TRACE << "New valid neighbor found... Adding to OPEN" << ENDL;
            }
            // it is in OPEN and the new path to neighbor is shorter - update its f_cost and parent node
            else if (neighbor_i != -1 && f_cost < openQueue.at(neighbor_i)->f_cost) {
                openQueue.at(i)->f_cost = f_cost;
                std::weak_ptr<SearchNode> prevNode = current;
                openQueue.at(i)->prevNode = prevNode;
                sortOPEN = true;
                TRACE << "Neighbor already is in OPEN, but we have found a better path to it. Updating the entry in OPEN." << ENDL;
            }
        }
        
        // if internal state changed, or anything in OPEN changed, re-sort OPEN
        if ( sortOPEN ) {
            // Use QUICK sort
            this->quickSort(&openQueue, 0, openQueue.size() - 1);
        }
    }

    // Output the results of the search
    this->outputPath(&closedMap);
}

void Simulation::quickSort(std::vector<std::shared_ptr<SearchNode>>* vector, int low, int high) {
    if ( low < high ) {
      /* p is partitioning index, vector[p] is now
        at right place */
      int p = partition(vector, low, high);

      // Separately sort elements before
      // partition and after partition
      quickSort(vector, low, p - 1);
      quickSort(vector, p + 1, high);
   }
}

int Simulation::partition(std::vector<std::shared_ptr<SearchNode>>* vector, int low, int high) {
    double pivot = vector->at(high)->f_cost; // pivot
    int i = (low - 1); // Index of larger element and indicates the right position of pivot found so far
 
    for (int j = low; j < high; j++)
    {
        // If current element is larger than the pivot
        if (vector->at(j)->f_cost > pivot)
        {
            i++; // increment index of larger element
            vector->at(i).swap(vector->at(j));
        }
    }
    vector->at(i + 1).swap(vector->at(high));
    return (i + 1);
}



void Simulation::outputPath(std::unordered_map<std::shared_ptr<Node>, std::shared_ptr<SearchNode>>* closedMap) {
    std::vector<std::shared_ptr<Node>> backwardsPath;
    backwardsPath.push_back(agent->getPrimaryGoal());
    findPath(&backwardsPath, (*closedMap)[agent->getPrimaryGoal()]->prevNode);

    INFO << "RESULTING PATH (Length: " << backwardsPath.size() << "):" << ENDL;
    for ( int i = backwardsPath.size() - 1; i >= 0; i-- ) {
        INFO << getCoordString(backwardsPath.at(i)->getCoord()) << ENDL;
    }
}

void Simulation::findPath(std::vector<std::shared_ptr<Node>>* path, std::weak_ptr<SearchNode> pathNode) {
    // Make sure the SearchNode the weak pointer to is pointing to still exists
    if ( auto tempSharedPointer = pathNode.lock() ) {
        path->push_back(tempSharedPointer->node);
        // Recursive Case: There is more to the path to add to the stack
        if ( tempSharedPointer->node != agent->getStartingNode() ) {
            findPath(path, tempSharedPointer->prevNode);
        }
    }
    else {
        FATAL << "Encountered a weak pointer whose object does not exist!" << ENDL;
        FATAL << "Here's the path that was able to be compiled (Length: " << path->size() << "):" << ENDL;
        for ( int i = path->size() - 1; i >= 0; i-- ) {
            INFO << getCoordString(path->at(i)->getCoord()) << ENDL;
        }
    }
}