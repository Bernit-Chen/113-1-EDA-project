#include <cassert>
#include "legalizer/legalizer.h"
#include "util/strOperation.h"
#include "physical/ntkObject.h"

bool Legalizer::parseInput(int argc, char **argv) {
    assert ((argc == 5) &&
            (std::string(argv[1]) == "-l" || std::string(argv[1]) == "--legalize"));

    IOPkg parseInputMsg;
    parseInputMsg << "Parsing input\n";
    parseInputMsg.setColor("red");

    std::string inputLibraryPath = argv[2];

    std::string inputMacroLef = inputLibraryPath + "_8T.macro.lef";
    if (!parseInputMacroLef(inputMacroLef)) {
        parseInputMsg << "Failed to parse " << inputMacroLef << "\n";
        return false;
    }
    inputMacroLef = inputLibraryPath + "_12T.macro.lef";
    if (!parseInputMacroLef(inputMacroLef)) {
        parseInputMsg << "Failed to parse " << inputMacroLef << "\n";
        return false;
    }
    // inputMacroLef = inputLibraryPath + "_VDD.macro.lef";
    // if (!parseInputMacroLef(inputMacroLef)) {
    //     parseInputMsg << "Failed to parse " << inputMacroLef << "\n";
    //     return false;
    // }
    // inputMacroLef = inputLibraryPath + "_VSS.macro.lef";
    // if (!parseInputMacroLef(inputMacroLef)) {
    //     parseInputMsg << "Failed to parse " << inputMacroLef << "\n";
    //     return false;
    // }

    // std::string inputLib = inputLibraryPath + "_typical_conditional_nldm_8T.lib";
    // if (!parseInputLib(inputLib)) {
    //     parseInputMsg << "Failed to parse " << inputLib << "\n";
    //     return false;
    // }
    // inputLib = inputLibraryPath + "_typical_conditional_nldm_12T.lib";
    // if (!parseInputLib(inputLib)) {
    //     parseInputMsg << "Failed to parse " << inputLib << "\n";
    //     return false;
    // }
    // inputLib = inputLibraryPath + "_typical_conditional_nldm_VDD.lib";
    // if (!parseInputLib(inputLib)) {
    //     parseInputMsg << "Failed to parse " << inputLib << "\n";
    //     return false;
    // }
    // inputLib = inputLibraryPath + "_typical_conditional_nldm_VSS.lib";
    // if (!parseInputLib(inputLib)) {
    //     parseInputMsg << "Failed to parse " << inputLib << "\n";
    //     return false;
    // }

    std::string inputDef = argv[3];
    if (!parseInputDef(inputDef)) {
        parseInputMsg << "Failed to parse " << inputDef << "\n";
        return false;
    }

    return true;
}

bool Legalizer::parseInputMacroLef(std::string inputName) {
    // Parse it 

    // std::cout << "parseInputMacroLef"<< "\n";
    std::cout << "Parsing " << inputName << "\n";
    IOPkg input(true, false, inputName, "");
    if (!input.inputExist()) {
        std::cout << "Failed to open " << inputName << "\n";
        return false;
    }

    // std::cout << "Start" << "\n";
    std::string data;
    std::string macroName;
    float width = 0, height = 0;
    LibGate* currentLibGate = nullptr;

    while (!input.inputFinish()) {
        input >> data;

        if (data == "MACRO") {
            input >> macroName;
            // std::cout << "Found MACRO: " << macroName << "\n";

            currentLibGate = new LibGate(macroName, 0, 0, 0, 0, macroName, LibGate::SR_SHORT);
            chip->addLibGate(currentLibGate);
            chip->addLibGateName2Idx(macroName, chip->libGateList().size() - 1);
        }
        else if (data == "SIZE") {
            std::string widthStr, heightStr;

            input >> widthStr >> data >> heightStr;
            width = std::stof(widthStr);
            height = std::stof(heightStr);

            if (currentLibGate) {
                *currentLibGate = LibGate(macroName, 0, 0, width, height, macroName, LibGate::SR_SHORT);
            } else {
                std::cerr << "Error: No valid LibGate to set SIZE." << std::endl;
                return false;
            }
        }
        else if (data == "PIN") {
            std::string pinName;
            input >> pinName;
            // std::cout << "Found PIN: " << pinName << "\n";

            if (!currentLibGate) {
                std::cerr << "Error: Found PIN but no current MACRO (LibGate)." << std::endl;
                return false;
            }

            bool validPin = true;
            Pin::direction pinDirection = Pin::IN;

            while (true) {
                input >> data;

                if (data == "DIRECTION") {
                    input >> data;

                    if (data == "INOUT") {
                        validPin = false;
                        break;
                    } else if (data == "INPUT") {
                        pinDirection = Pin::IN;
                    } else if (data == "OUTPUT") {
                        pinDirection = Pin::OUT;
                    }
                }

                if (data == "PORT") {
                    float x1 = -1, y1 = -1, x2 = -1, y2 = -1;
                    std::string x1Str, y1Str, x2Str, y2Str;

                    while (data != "END") {
                        input >> data;

                        if (data == "RECT") {
                            input >> x1Str >> y1Str >> x2Str >> y2Str;
                            x1 = std::stof(x1Str);
                            y1 = std::stof(y1Str);
                            x2 = std::stof(x2Str);
                            y2 = std::stof(y2Str);
                        }
                    }

                    if (validPin) {
                        Pin* pin = new Pin(pinName, Pin::LIBGATE, pinDirection);
                        Port* port = new Port(x1, y1, x2, y2);
                        pin->addPort(port);

                        chip->libGateList().back()->addPin(pin);
                        chip->libGateList().back()->addPinName2Idx(pinName, chip->libGateList().back()->pinList().size() - 1);
                    }
                }
                else if (data == "END") {
                    break;
                }
            }
        }
        else if (data == "OBS") {
            while (data != "END") {
                input >> data;
            }
        }
        else if (data == "END") {
            input >> data; 

            if (data == macroName) {
                // std::cout << "Ending MACRO: " << macroName << "\n";
                currentLibGate = nullptr;
            }
        }
    }
    // std::cout << "Finish" << "\n";
    // std::cout << "\nParsed LibGates:\n";
    // for (const auto& libGate : chip->libGateList()) {
    //     libGate->print();
    // }
    return true;
}

bool Legalizer::parseInputLib(std::string inputName) {
    // TODO
    return true;
}

bool Legalizer::parseInputDef(std::string inputName) {
    std::cout << "Parsing " << inputName << "\n";

    IOPkg input(true, false, inputName, "");
    if (!input.inputExist()) {
        std::cout << "Failed to open " << inputName << "\n";
        return false;
    }

    bool firstRow = true;
    std::string data;
    int dbuPerMicron = -1;

    while (!input.inputFinish()) {
        input >> data;
        
        if (data == "DESIGN") {
            input >> data;
            chip->setName(data);
        }
        else if (data == "UNITS") {
            input >> data >> data >> data;
            dbuPerMicron = std::stoi(data);
        }
        else if (data == "DIEAREA") {
            assert (dbuPerMicron != -1);
            float x1, y1, x2, y2;
            input >> data >> data;
            x1 = std::stof(data)/dbuPerMicron;
            input >> data;
            y1 = std::stof(data)/dbuPerMicron;
            input >> data >> data >> data;
            x2 = std::stof(data)/dbuPerMicron;
            input >> data;
            y2 = std::stof(data)/dbuPerMicron;
            input >> data;
            chip->setBoundary(x1, y1, x2, y2);
        }
        else if (data == "ROW" && firstRow) {
            float x1, y1, x2, y2;
            int numX;
            float stepX;
            input >> data >> data >> data;
            x1 = std::stof(data);
            input >> data;
            y1 = std::stof(data);
            input >> data >> data >> data;
            numX = std::stoi(data);
            chip->setNumSites(numX);
            input >> data >> data >> data >> data;
            stepX = std::stof(data);
            chip->setSiteWidth(stepX/dbuPerMicron);
            x2 = x1 + chip->siteWidth() * chip->numSites();
            y2 = y1 + chip->shortRowHeight();
            chip->addRow(new Row(x1, y1, x2, y2, Row::SHORT, Row::N));
            int numRows = floor(chip->boundary().height() / (y2-y1));
            for (int i = 1; i < numRows; i++) {
                if (i % 2 == 0) {
                    y1 += chip->shortRowHeight();
                    y2 += chip->shortRowHeight();
                    chip->addRow(new Row(x1, y1, x2, y2, Row::SHORT, Row::FS));
                }
                else {
                    y1 += chip->tallRowHeight();
                    y2 += chip->tallRowHeight();
                    chip->addRow(new Row(x1, y1, x2, y2, Row::TALL, Row::N));
                }
            }
            firstRow = false;
        }
        else if (data == "COMPONENTS") {
            int numComps;
            std::string compName, modelName;
            float x1, y1, x2, y2;
            Node::orient orient;
            LibGate* libGate;

            input >> data;
            numComps = std::stoi(data);
            input >> data;

            for (int i = 0; i < numComps; i++) {
                input >> data >> data;
                compName = data;
                input >> data;
                modelName = data;
                while (data != "(") {
                    input >> data;
                }
                input >> data;
                std::cout << data << "\n";
                x1 = std::stof(data)/dbuPerMicron;
                std::cout << "x1: " << x1 << "\n";
                input >> data;
                y1 = std::stof(data)/dbuPerMicron;
                input >> data >> data;

                Node::orient orient;
                if (data == "N") {
                    orient = Node::N;
                }
                else if (data == "FN") {
                    orient = Node::FN;
                }
                else if (data == "S") {
                    orient = Node::S;
                }
                else if (data == "FS") {
                    orient = Node::FS;
                }
                input >> data;
                
                auto libGateMap = chip->libGateName2Idx();
                
                if (libGateMap.find(modelName) != libGateMap.end()) {
                    int gateIndex = libGateMap.at(modelName);
                    libGate = chip->libGateList().at(gateIndex);
                    x2 = x1 + libGate->width();
                    y2 = y1 + libGate->height();

                    Node* node = new Node(compName, Node::PI, orient, libGate, x1, y1, x2, y2);
                    chip->addNode(node);
                    chip->addNodeName2Idx(compName, chip->nodeList().size() - 1);

                    for (const auto& libPin : libGate->pinList()) {
                        Pin* nodePin = new Pin(*libPin); 
                        node->addPin(nodePin);
                        node->addPinName2Idx(nodePin->name(), node->pinList().size() - 1);
                    }
                }
            }
        }
        else if (data == "PINS") {
            // int numPins;
            // std::string pinName, netName;
            // Pin::direction direction;
            // input >> data;
            // numPins = std::stoi(data);
            // input >> data;
            // for (int i = 0; i < numPins; i++) {

            // }
        }
        else if (data == "NETS") {
            int numNets;
            std::string netName;

            input >> data;
            numNets = std::stoi(data);
            input >> data;

            for (int i = 0; i < numNets; i++) {
                input >> data;
                input >> netName;
                Wire* wire = new Wire(netName);

                while (true) {
                    input >> data;
                    if (data == ";") break;
                    if (data == "(") {
                        std::string nodeName, pinName;
                        input >> nodeName >> pinName;

                        if (!pinName.empty() && pinName.back() == ')') {
                            pinName.pop_back();
                        }

                        if (nodeName == "PIN") {
                            // ex: ( PIN key1_20_ )
                        } else {
                            if (chip->nodeName2Idx().count(nodeName)) {
                                int nodeIdx = chip->nodeName2Idx().at(nodeName);
                                Node* node = chip->nodeList().at(nodeIdx);

                                if (node->pinName2Idx().count(pinName)) {
                                    int pinIdx = node->pinName2Idx().at(pinName);
                                    Pin* pin = node->pinList().at(pinIdx);

                                    wire->addPin(pin);
                                    pin->setWire(wire);
                                } else {
                                    std::cerr << "Error: Pin " << pinName << " not found in node " << nodeName << "\n";
                                }
                            } else {
                                std::cerr << "Error: Node " << nodeName << " not found in chip\n";
                            }
                        }
                    }
                }
                chip->addWire(wire);
                chip->addWireName2Idx(netName, chip->wireList().size() - 1);
            }
        }
        else if (data == "END") {
            input >> data;
        }
    }

    // chip->print();
    // std::cout << "Parsing completed \n";

    return true;
}