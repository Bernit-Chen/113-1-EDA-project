#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include "legalizer/legalizer.h"

using namespace std;

bool Legalizer::_genGurobi() {
    _writeInfoMsg("Generating the Gurobi model ...\n");

    ofstream outFile("NIMCH_gurobi_c++.cpp");
    if (!outFile.is_open()) {
        cerr << "Error: Failed to open output file!" << endl;
        return false;
    }

    outFile << "#include <iostream>" << endl;
    outFile << "#include <cstring>" << endl;
    outFile << "#include <fstream>" << endl;
    outFile << "#include <unordered_map>" << endl;
    outFile << "#include <vector>" << endl << endl;

    outFile << "#include \"gurobi_c++.h\"" << endl;
    outFile << "using namespace std;" << endl << endl;

    outFile << "template <typename T>" << endl;
    outFile << "using str2 = std::unordered_map<std::string, T>;" << endl << endl;

    outFile << "int main() {" << endl;
    outFile << "    try {" << endl;

    outFile << "        ofstream outFile(\"NIMCH_gurobi_result.txt\");" << endl;

    outFile << "        GRBEnv env = GRBEnv(true);" << endl;
    outFile << "        env.set(\"LogFile\", \"NIMCH_gurobi.log\");" << endl;
    outFile << "        env.start();" << endl;
    outFile << "        GRBModel model = GRBModel(env);" << endl << endl;

    const auto& piList = _chip->netlist->getPIList();
    const auto& intNodeList = _chip->netlist->getIntNodeList();
    const auto& poList = _chip->netlist->getPOList();
    double maxDelay = _chip->netlist->getMaxDelay(); 
    int numRows = _chip->getNumRows(); 
    double chipWidth = _chip->getBoundary().width();
    double chipheight = _chip->getBoundary().height();

    // Output node and gate information
    outFile << "        // Node and Gate Information" << endl;
    outFile << "        vector<string> piList = {\"" << piList[0]->getName() << "\"";
    for (size_t i=1; i<piList.size(); ++i) {
        outFile << ", \"" << piList[i]->getName() << "\"";
    }
    outFile << "};" << endl;
    outFile << "        vector<string> intNodeList = {\"" << intNodeList[0]->getName() << "\"";
    for (size_t i=1; i<intNodeList.size(); ++i) {
        outFile << ", \"" << intNodeList[i]->getName() << "\"";
    }
    outFile << "};" << endl;
    outFile << "        vector<string> poList = {\"" << poList[0]->getName() << "\"";
    for (size_t i=1; i<poList.size(); ++i) {
        outFile << ", \"" << poList[i]->getName() << "\"";
    }
    outFile << "};" << endl;
    outFile << "        str2<vector<string>> gateList = {" << endl;
    for (const auto& intNode : intNodeList) {
        vector<sPtr<LibGate>> libGateList = _gateLibrary->getLibGateList(intNode->getLogic());
        outFile << "            {\"" << intNode->getName() << "\", {\"" << libGateList[0]->getName() << "\"";
        for (size_t i=1; i<libGateList.size(); ++i) {
            outFile << ", \"" << libGateList[i]->getName() << "\"";
        }
        outFile << "}}," << endl;
    }
    outFile << "        };" << endl << endl;

    outFile << "        str2<str2<GRBVar>> x;" << endl;
    outFile << "        for (const auto& intNode : intNodeList) {" << endl;
    outFile << "            for (const auto& gate : gateList[intNode]) {" << endl;
    outFile << "                string x_i_k = \"x[\" + intNode + \"][\" + gate + \"]\";" << endl;
    outFile << "                x[intNode][gate] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, x_i_k);" << endl;
    outFile << "            } // for each libGate whose logic matches intNode" << endl;
    outFile << "        } // for each intNode" << endl << endl;

    outFile << "        str2<GRBVar> iAT, oAT;" << endl;
    outFile << "        for (const auto& intNode : intNodeList) {" << endl;
    outFile << "            string iAT_i = \"iAT[\" + intNode + \"]\";" << endl;
    outFile << "            string oAT_i = \"oAT[\" + intNode + \"]\";" << endl;
    outFile << "            iAT[intNode] = model.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, iAT_i);" << endl;
    outFile << "            oAT[intNode] = model.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, oAT_i);" << endl;
    outFile << "        } // for each internal node" << endl;
    outFile << "        for (const auto& poNode : poList) {" << endl;
    outFile << "            string iAT_i = \"iAT[\" + poNode + \"]\";" << endl;
    outFile << "            iAT[poNode] = model.addVar(0.0, 0.0, 0.0, GRB_CONTINUOUS, iAT_i);" << endl;
    outFile << "        } // for each PO node" << endl << endl;

    outFile << "        std::string constraint;" << endl;
    outFile << "        int count;" << endl;
    outFile << "        char constrName[32];" << endl << endl;

    outFile << "        count = 0;" << endl;
    outFile << "        GRBLinExpr xSum;" << endl;
    outFile << "        for (const auto& intNode : intNodeList) {" << endl;
    outFile << "            xSum = 0;" << endl;
    outFile << "            for (const auto& gate : gateList[intNode]) {" << endl;
    outFile << "                xSum += x[intNode][gate];" << endl;
    outFile << "            } // for each libGate whose logic matches intNode" << endl;
    outFile << "            strcpy(constrName, (\"c1[\" + intNode + \"]\").c_str());" << endl;
    outFile << "            model.addConstr(xSum == 1, constrName);" << endl;
    outFile << "        } // for each intNode" << endl << endl;

    std::string constraint;
    
    for (sPtr<IntNode> intNode : intNodeList) {
        std::string iAT_i = "iAT[\"" + intNode->getName() + "\"]";
        for (std::string iWireName : intNode->getInWireList()) {
            sPtr<Node> iNode = _chip->netlist->getWire(iWireName)->getInNode();
            std::string oAT_j;
            if (iNode->isInternal()) {
                sPtr<IntNode> intNode = _chip->netlist->getIntNode(iNode->getName());
                oAT_j = "oAT[\"" + intNode->getName() + "\"]";
            } // if iNode is internal
            else if (iNode->isPI()) {
                sPtr<PINode> piNode = _chip->netlist->getPI(iNode->getName());
                oAT_j = to_string(piNode->getOAT());
            } // if iNode is PI
            else {
                _writeErrorMsg("Error: iNode is neither internal nor PI!\n");
                return false;
            } // if iNode is neither internal nor PI (impossible)
            std::string delay_j_i = to_string(_chip->netlist->getWire(iWireName)->getDelay());
            constraint = iAT_i + " >= " + oAT_j + " + " + delay_j_i;
            outFile << "        model.addConstr(" << constraint
                    << ", \"c2[" << intNode->getName() << "][" << iNode->getName() << "]\");" << endl;
        } // for each input wire of intNode
    } // for each intNode
    outFile << endl;

    outFile << "        GRBLinExpr delaySum;" << endl;
    for (sPtr<IntNode> intNode : intNodeList) {
        std::string iAT_i = "iAT[\"" + intNode->getName() + "\"]";
        std::string oAT_i = "oAT[\"" + intNode->getName() + "\"]";
        outFile << "        delaySum = 0;" << endl;
        for (sPtr<LibGate> libGate : _gateLibrary->getLibGateList(intNode->getLogic())) {
            std::string x_i_k = "x[\"" + intNode->getName() + "\"][\"" + libGate->getName() + "\"]";
            std::string delay_i_k = to_string(intNode->getDelay(libGate->getName()));
            outFile << "        delaySum += " << x_i_k << " * " << delay_i_k << ";" << endl;
        } // for each libGate whose logic matches intNode
        constraint = oAT_i + " == " + iAT_i + " + delaySum";
        outFile << "        model.addConstr(" << constraint << ", \"c3[" << intNode->getName() << "]\");" << endl;
    } // for each intNode
    outFile << endl;

    for (sPtr<PONode> poNode : poList) {
        std::string iAT_i = "iAT[\"" + poNode->getName() + "\"]";
        constraint = iAT_i + " <= " + to_string(maxDelay);
        outFile << "        model.addConstr(" << constraint << ", \"c4[" << poNode->getName() << "]\");" << endl;
    } // for each PO node
    outFile << endl;

    const std::string gamma = "0.9";
    outFile << "        GRBLinExpr widthSum;" << endl;
    for (int r = 0; r < numRows; ++r) {
        outFile << "        widthSum = 0;" << endl;
        LibGate::height height = _chip->isRowShort(r) ? LibGate::height::SHORT : LibGate::height::TALL;
        for (int nearestRow=(r-1); nearestRow<=(r+1); ++nearestRow) {
            if (nearestRow >= 0 && nearestRow < numRows) {
                for (sPtr<IntNode> intNode : _chip->getNodesOnRow(nearestRow)) {
                    for (sPtr<LibGate> libGate : _gateLibrary->getLibGateList(intNode->getLogic())) {
                        if (libGate->getHeight() == height) {
                            std::string x_i_k = "x[\"" + intNode->getName() + "\"][\"" + libGate->getName() + "\"]";
                            std::string width_k = to_string(libGate->getBoundary().width());
                            outFile << "        widthSum" << " += " << x_i_k << " * " << width_k << ";" << endl;
                        } // if libGate height matches row height
                    } // for each libGate whose logic matches intNode
                } // for each intNode on nearestRow
            } // if nearestRow is valid
        } // for each nearestRow = row - 1, row, row + 1
        constraint = "widthSum >= " + gamma + " * " + to_string(chipWidth);
        outFile << "        model.addConstr(" << constraint << ", \"c5[" << r << "][upper]\");" << endl;
        constraint = "widthSum <= " + to_string(chipWidth);
        outFile << "        model.addConstr(" << constraint << ", \"c[" << r << "][lower]\");" << endl;
    } // for each row
    outFile << endl;

// Objective
    // (o) minimize alpha*Cost_area + (1-alpha)*Cost_hdiff
    outFile << "        double alpha = 0.5;" << endl;

    outFile << "        // (o) alpha*Cost_area" << endl;
    outFile << "        GRBLinExpr areaSum = 0;" << endl;
    for (sPtr<IntNode> intNode : intNodeList) {
        const auto& gateList = _gateLibrary->getLibGateList(intNode->getLogic());
        for (sPtr<LibGate> gate : gateList) {
            std::string x_i_k = "x[\"" + intNode->getName() + "\"][\"" + gate->getName() + "\"]";
            std::string area_k = to_string(gate->getBoundary().area());
            outFile << "        areaSum += " << x_i_k << " * " << area_k << ";" << endl;
        }
    }
    outFile << "        Cost_area = " << "areaSum" << " / " << chipheight << " / " << chipWidth << ";" << endl;
    
    outFile << "        // (o) (1-alpha)*Cost_hdiff" << endl;
    outFile << "        GRBLinExpr heightMismatchSum = 0;" << endl;
    for (sPtr<IntNode> intNode : intNodeList) {
        bool origin_isShortRow = _chip->isRowShort(intNode->getRow()); // True-8T ; False-12T
        bool origin_isTallRow = !origin_isShortRow;
        const auto& gateList = _gateLibrary->getLibGateList(intNode->getLogic());
        for (sPtr<LibGate> gate : gateList) {
            if ((origin_isShortRow && gate->isTall()) || (origin_isTallRow && gate->isShort())) {
                std::string x_i_k = "x[\"" + intNode->getName() + "\"][\"" + gate->getName() + "\"]";
                outFile << "        heightMismatchSum += " << x_i_k << ";" << endl;
            }
        }
    }
    outFile << "        Cost_hdiff = " << "heightMismatchSum" << " / " << intNodeList.size() << ";" << endl;

    outFile << "        model.setObjective(alpha*Cost_area+(1-alpha)*Cost_hdiff, GRB_MINIMIZE);" << endl << endl;
    outFile << "        model.optimize();" << endl << endl;
    
    // TODO: write the optimization result to the output file "NIMCH_gurobi_result.txt"
    outFile << "        // Output Results" << endl;
    outFile << "        for (const auto& intNode : intNodeList) {" << endl;
    outFile << "            for (const auto& gate : gateList[intNode]) {" << endl;
    outFile << "                if (x[intNode][gate].get(GRB_DoubleAttr_X) > 0.5) {" << endl;
    outFile << "                    outFile << intNode << \" \" << gate << endl;" << endl;
    outFile << "                    break;" << endl;
    outFile << "                }" << endl;
    outFile << "            }" << endl;
    outFile << "        }" << endl << endl;
    
    outFile << "        outFile.close();" << endl << endl;

    outFile << "    } catch (GRBException e) {" << endl;
    outFile << "        cerr << \"Error code = \" << e.getErrorCode() << endl;" << endl;
    outFile << "        cerr << e.getMessage() << endl;" << endl;
    outFile << "    } catch (...) {" << endl;
    outFile << "        cerr << \"Exception during optimization\" << endl;" << endl;
    outFile << "    }" << endl << endl;

    outFile << "    return 0;" << endl;
    outFile << "}" << endl;

    outFile.close();
    
    _writeSuccessMsg("Gurobi model generated\n");
    return true;
}