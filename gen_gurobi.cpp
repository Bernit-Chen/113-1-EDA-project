#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include "legalizer/legalizer.h"

using namespace std;

bool Legalizer::gen_gurobi() {
    ofstream outFile("NIMCH_gurobi_c++.cpp");
    if (!outFile.is_open()) {
        cerr << "Error: Failed to open output file!" << endl;
        return false;
    }

    outFile << "#include <iostream>" << endl;
    outFile << "#include <fstream>" << endl;
    outFile << "#include \"gurobi_c++.h\"" << endl;
    outFile << "using namespace std;" << endl << endl;

    outFile << "int main() {" << endl;
    outFile << "    ofstream outFile(\"NIMCH_gurobi_result.txt\");" << endl;
    outFile << "    try {" << endl;

    outFile << "        GRBEnv env = GRBEnv(true);" << endl;
    outFile << "        env.set(\"LogFile\", \"NIMCH_gurobi.log\");" << endl;
    outFile << "        env.start();" << endl;
    outFile << "        GRBModel model = GRBModel(env);" << endl << endl;

    const auto& piList = _chip.netlist.getPIList();
    const auto& intNodeList = _chip.netlist.getIntNodeList();
    const auto& poList = _chip.netlist.getPOList();
    double maxDelay = _chip.netlist.getMaxDelay(); 
    int numRows = _chip.getNumRows(); 
    double chipWidth = _chip.getBoundary().width();

    // Variables: X__i__k, indicating whether a libGate k is selected for an intNode i
    for (IntNode* intNode : intNodeList) {
        const auto& gateList = _gateLibrary.getLibGateList(intNode->getLogic());
        for (LibGate* libGate : _gateLibrary.getLibGateList(intNode->getLogic())) {
            std::string X__i__k = "X__" + intNode->getName() + "__" + libGate->getName();
            outFile << "        GRBVar " << X__i__k
                    << " = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, \"" << X__i__k << "\");" << endl;
        } // for each libGate whose logic matches intNode
    } // for each intNode

    // Variables: oAT__i, indicating the output arrival time of a node i
    for (PINode* piNode : piList) {
        std::string oAT__i = "oAT__" + piNode->getName();
        outFile << "        const " << oAT__i << " = 0.0;" << endl;
    } // for each PI node
    for (IntNode* intNode : intNodeList) {
        std::string oAT__i = "oAT__" + intNode->getName();
        outFile << "        GRBVar " << oAT__i
                << " = model.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, \"" << oAT__i << "\");" << endl;
    } // for each internal node
    for (PONode* poNode : poList) {
        std::string oAT__i = "oAT__" + poNode->getName();
        outFile << "        GRBVar " << oAT__i
                << " = model.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, \"" << oAT__i << "\");" << endl;
    } // for each PO node

    // Constraints: X__i = sum_k{X__i__k} = 1. Each intNode i selects exactly one libGate k
    for (IntNode* intNode : intNodeList) {
        std::string X__i = "X__" + intNode->getName();
        outFile << "        GRBLinExpr " << X__i << " = 0;" << endl;
        for (LibGate* libGate : _gateLibrary.getLibGateList(intNode->getLogic())) {
            std::string X__i__k = "X__" + intNode->getName() + "__" + libGate->getName();
            outFile << "        " << X__i << " += " << X__i__k << ";" << endl;
        } // for each libGate whose logic matches intNode
        std::string constraint__i = X__i + " == 1";
        outFile << "        model.addConstr(" << constraint__i << ", \"" << constraint__i << "\");" << endl;
    } // for each intNode
    
    for (IntNode* intNode : intNodeList) {
        std::string oAT__i = "oAT__" + intNode->getName();
        std::string oAT_sum__i = "oAT_sum__" + intNode->getName();
        outFile << "        GRBLinExpr " << oAT_sum__i << " = 0;" << endl;
        for (LibGate* libGate : _gateLibrary.getLibGateList(intNode->getLogic())) {
            // Constraints: oAT__i__k = max_j(oAT__j + delay(k, j), for all j in inputNode(Node)
            // Output arrival time of intNode i mapped to libGate k is the max arrival times of all inputNode j
            std::string X__i__k = "X__" + intNode->getName() + "__" + libGate->getName();
            std::string oAT__i__k = "oAT__" + intNode->getName() + "__" + libGate->getName();
            for (std::string iWireName : intNode->getInWireList()) {
                Node* iNode = _chip.netlist.getWire(iWireName)->getInNode();
                std::string oAT__j = "oAT__" + iNode->getName();
                std::string constraint__i__k__j = oAT__i__k + " >= " + oAT__j + " + "
                    + to_string(intNode->getDelay(libGate->getName(), iNode->getName()));
                outFile << "        model.addConstr(" << constraint__i__k__j << ", \"" << constraint__i__k__j << "\");" << endl;
            } // for each inputNode j
            outFile << "        " << oAT_sum__i << " += " << X__i__k << " * " << oAT__i__k << ";" << endl;
        } // for each libGate whose logic matches intNode
        // Constraints: oAT__i = sum_k{X__i__k * oAT__i__k}
        // Output arrival time of intNode i based on libGate k selection
        std::string constraint__i = oAT__i + " == " + oAT_sum__i;
        outFile << "        model.addConstr(" << constraint__i << ", \"" << constraint__i << "\");" << endl;
    }

    // Constraint: Fanout delay <= max delay (only for external nodes)
    for (PONode* poNode : poList) {
        std::string oAT__i = "oAT__" + poNode->getName();
        std::string constraint__i = oAT__i + " <= " + to_string(maxDelay);
        outFile << "        model.addConstr(" << constraint__i << ", \"" << constraint__i << "\");" << endl;
    } // for each PO node

    const double gamma = 0.9;
    for (int r = 0; r < numRows; ++r) {
        outFile << "        GRBLinExpr rowWidth = 0;" << endl;
        for (int nearestRow = r - 1; nearestRow <= r + 1; ++nearestRow) {
            if (nearestRow >= 0 && nearestRow < numRows) {
                for (IntNode* intNode : _chip.getNodesOnRow(nearestRow)) {
                    LibGate::height height = _chip.isRowShort(nearestRow) ? LibGate::height::SHORT : LibGate::height::TALL;
                    for (LibGate* libGate : _gateLibrary.getLibGateList(intNode->getLogic())) {
                        if (libGate->getHeight() == height) {
                            std::string X__i__k = "X__" + intNode->getName() + "__" + libGate->getName();
                            double gateWidth = libGate->getBoundary().width();
                            outFile << "        rowWidth += " << X__i__k << " * " << gateWidth << ";" << endl;
                        } // if libGate height matches row height
                    } // for each libGate whose logic matches intNode
                } // for each intNode on nearestRow
            } // if nearestRow is valid
        } // for each nearestRow = row - 1, row, row + 1
        // Constraints: Sum of all intNodes widths on three nearest rows falls within [r * chip width, chip width]
        std::string constraint_lower__r = "rowWidth >= " + to_string(gamma) + " * " + to_string(chipWidth);
        std::string constraint_upper__r = "rowWidth <= " + to_string(chipWidth);
        outFile << "        model.addConstr(" << constraint_lower__r << ", \"" << constraint_lower__r << "\");" << endl;
        outFile << "        model.addConstr(" << constraint_upper__r << ", \"" << constraint_upper__r << "\");" << endl;
    } // for each row

    // Objective: Minimize total gate area
    outFile << "        GRBLinExpr totalArea = 0;" << endl;
    for (IntNode* intNode : intNodeList) {
        const auto& gateList = _gateLibrary.getLibGateList(intNode->getLogic());
        for (LibGate* gate : gateList) {
            std::string X__i__k = "X__" + intNode->getName() + "__" + gate->getName();
            outFile << "        totalArea += " << X__i__k
                    << " * " << gate->getBoundary().area() << ";" << endl;
        }
    }
    outFile << "        model.setObjective(totalArea, GRB_MINIMIZE);" << endl << endl;

    outFile << "        model.optimize();" << endl;

    outFile << "    } catch (GRBException e) {" << endl;
    outFile << "        cerr << \"Error code = \" << e.getErrorCode() << endl;" << endl;
    outFile << "        cerr << e.getMessage() << endl;" << endl;
    outFile << "    } catch (...) {" << endl;
    outFile << "        cerr << \"Exception during optimization\" << endl;" << endl;
    outFile << "    }" << endl;

    outFile << "    return 0;" << endl;
    outFile << "}" << endl;

    outFile.close();
    return true;
}