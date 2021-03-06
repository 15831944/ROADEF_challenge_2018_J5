#include "glassNode.h"

#include "../GlassData/glassConstants.h"
#include "../GlassCutter/glassCutter.h"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <ostream>
#include <iostream>

#define BUILD_LAZY true
#define MAX_BUILT_SIZE 5000

GlassNode::GlassNode(GlassInstance* instance, GlassCutter* cutter, unsigned int plateIndex, 
    unsigned int x, unsigned int y, unsigned int width, unsigned int height,
    unsigned int depth, int type)
    : instance(instance), cutter(cutter), 
    plateIndex(plateIndex), x(x), y(y), width(width), height(height), depth(depth), type(type), 
    sons(std::vector<GlassNode>()), cutsAvailable(std::vector<GlassCut>()), realCuts(std::vector<RealCut>()),
    beginSequencePosition(0), endSequencePosition(0), xMax(x) {
}

GlassNode::GlassNode()
    : instance(NULL), cutter(NULL), x(0), y(0), width(WIDTH_PLATES), height(HEIGHT_PLATES), depth(0), type(BRANCH),
    sons(std::vector<GlassNode>()), cutsAvailable(std::vector<GlassCut>()), realCuts(std::vector<RealCut>()),
    beginSequencePosition(0), endSequencePosition(0), xMax(0) {
}

void GlassNode::reset() {
    setX(0);
    setY(0);
    setWidth(WIDTH_PLATES);
    setHeight(HEIGHT_PLATES);
    setDepth(0);
    setType(BRANCH);
    sons.clear();
    cutsAvailable.clear();
    realCuts.clear();
}

void GlassNode::checkTooSmall() const {
    if (width < MIN_WASTE_AREA || height < MIN_WASTE_AREA) {
        //std::cout << (*this) << std::endl;
        throw std::runtime_error("Waste too small");
    }
}

void GlassNode::checkTooDepth() const {
    if (depth > MAX_DEPTH)
        throw std::runtime_error("Tree too depth");
}

void GlassNode::preCheckTrimming(const GlassLocationIt& first, const GlassLocationIt& last) const {
    if (depth < 3) return;
        if (std::distance(first, last) > 1) throw std::runtime_error("Trimming failed (prechecked)");
}

void GlassNode::checkDimensions(const GlassLocationIt& first, const GlassLocationIt& last) const {
    if (std::distance(first, last) == 0) return;

    if (depth == 1) {
        if (width > MAX_XX) 
            throw std::runtime_error("1-cut failed");
        if (width < MIN_XX)
            throw std::runtime_error("1-cut failed");
        return;
    }
    if (depth == 2) {
        if (height < MIN_YY)
            throw std::runtime_error("1-cut failed");
    }
    if (depth == 3) {
        for (GlassLocationIt locationIt = first; locationIt != last; locationIt++) {
            if (locationIt->getX() != x || locationIt->getWidth() != width)
                throw std::runtime_error("Other mistake");
            if (locationIt->getY() != y && locationIt->getYH() != y + height)
                throw std::runtime_error("Other mistake");
        }
        return;
    } 

    if (depth == 2) {
        for (GlassLocationIt locationIt = first; locationIt != last; locationIt++) {
            if (locationIt->getY() != y && locationIt->getYH() != y + height)
                throw std::runtime_error("Other mistake");
        }
    }
}

bool GlassNode::isNodeFitLocation(const GlassLocation& location) const {
    assert(location.getBinId() == plateIndex);
    return x == location.getX() && y == location.getY() 
        && width == location.getWidth() && height == location.getHeight();
}

void GlassNode::buildCutsAvailable(const GlassLocationIt& first, const GlassLocationIt& last) {
    cutsAvailable.clear();
    unsigned int index = std::distance(first, last) - 1;
    for (GlassLocationIt locationIt = first; locationIt != last; locationIt++) {
        if (isVerticalCut()) {
            cutsAvailable.emplace_back(GlassCut(locationIt->getX(), index, !BEGIN_ITEM, VERTICAL_CUT));
            cutsAvailable.emplace_back(GlassCut(locationIt->getXW(), index, BEGIN_ITEM, VERTICAL_CUT));
        } else {
            cutsAvailable.emplace_back(GlassCut(locationIt->getY(), index, !BEGIN_ITEM, !VERTICAL_CUT));
            cutsAvailable.emplace_back(GlassCut(locationIt->getYH(), index, BEGIN_ITEM, !VERTICAL_CUT));
        }
        index--;
    }
    std::sort(cutsAvailable.begin(), cutsAvailable.end());
    //std::cout << "-----------------" << std::endl;
    //displayCutsAvailable();
}

bool GlassNode::isFreeOfDefects(const GlassCut& cut) const {
   if (cut.isVerticalCut())
        return getPlate().cutIsFreeOutOfDefects(cut.getAbscissa(), y, height, cut.isVerticalCut());
    else
        return getPlate().cutIsFreeOutOfDefects(cut.getAbscissa(), x, width, cut.isVerticalCut());
    return true;
}

bool GlassNode::isCutPossibleForMinXY(unsigned int prevAbscissa, unsigned int abscissa) {
    int delta = prevAbscissa - abscissa;
    if (delta <= 0) std::cout<< prevAbscissa << " < " << abscissa << std::endl;
    assert(delta > 0);
    assert(depth < 3);
    switch (depth) {
        case 0:
            return delta > MIN_XX && delta < MAX_XX;
        case 1:
            return delta > MIN_YY;
        case 2:
            return true;
        assert(false);
        return false;
    }
    return false;
}

bool GlassNode::isCutPossibleForMinWaste(unsigned int abscissa) {
    for (const GlassCut& cut: cutsAvailable) {
        if (cut.getAbscissa() != abscissa && abs((int)cut.getAbscissa() - (int)abscissa) < MIN_WASTE_AREA) {
            return false;
        }
    }
    return true;
}

void GlassNode::buildRealCuts() {
    realCuts.clear();
    unsigned int openedCuts = 0;
    int maxItemSeen = -1;
    
    unsigned int nbItemsSeen = 0;
    unsigned int firstAbscissa = isVerticalCut() ? x : y;
    unsigned int lastAbscissa = isVerticalCut() ? x + width : y + height;
    unsigned int prevAbscissa = lastAbscissa;
    unsigned int nbPrevItems = 0;

    realCuts.emplace_back(RealCut(prevAbscissa, nbItemsSeen));
    
    for (const GlassCut& cut: cutsAvailable) {
        //std::cout << prevAbscissa << " new cut " << cut.getAbscissa() << std::endl;

        if (!cut.isBegin()) { assert(openedCuts > 0); openedCuts--; }

        if (maxItemSeen + 1 == nbItemsSeen && openedCuts == 0) {
            if (cut.getAbscissa() != firstAbscissa && cut.getAbscissa() != lastAbscissa
                && prevAbscissa - cut.getAbscissa() >= MIN_WASTE_AREA 
                && isCutPossibleForMinWaste(cut.getAbscissa()) && isFreeOfDefects(cut)) {

                bool isNotWasteCut = nbItemsSeen > nbPrevItems;

                if (depth < 3) {
                    if (isNotWasteCut) {
                        if (isCutPossibleForMinXY(prevAbscissa, cut.getAbscissa())) {
                            realCuts.emplace_back(RealCut(cut.getAbscissa(), nbItemsSeen));
                            prevAbscissa = cut.getAbscissa();
                            nbPrevItems = nbItemsSeen;
                        }
                    } else {
                        realCuts.emplace_back(RealCut(cut.getAbscissa(), nbItemsSeen));
                        prevAbscissa = cut.getAbscissa();
                    }
                } else {
                    if (nbItemsSeen - nbPrevItems > 1) 
                        throw std::runtime_error("Trimming failed (more than 1 item)");
                    if (realCuts.size() >= 2) {
                        throw std::runtime_error("Trimming failed (more than 1 cut)");
                    }
                    realCuts.emplace_back(RealCut(cut.getAbscissa(), nbItemsSeen));
                    prevAbscissa = cut.getAbscissa();
                    nbPrevItems = nbItemsSeen;
                }            
            } else if (depth == 0 && realCuts.size() == 1 && cut.getAbscissa() < lastAbscissa 
                && cut.getAbscissa() + MIN_WASTE_AREA < lastAbscissa) {
                unsigned int cutAbscissa = cut.getAbscissa() + MIN_WASTE_AREA;
                if (cutAbscissa > lastAbscissa) throw std::runtime_error("cut impossible");
                if (cutAbscissa != firstAbscissa && cutAbscissa != lastAbscissa
                    && prevAbscissa - cutAbscissa >= MIN_WASTE_AREA 
                    && isCutPossibleForMinWaste(cutAbscissa) && isFreeOfDefects(cut)) {

                    bool isNotWasteCut = nbItemsSeen > nbPrevItems;
                    if (isNotWasteCut) {
                        if (isCutPossibleForMinXY(prevAbscissa, cutAbscissa)) {
                            realCuts.emplace_back(RealCut(cutAbscissa, nbItemsSeen));
                            prevAbscissa = cutAbscissa;
                            nbPrevItems = nbItemsSeen;
                        }
                    } else {
                        realCuts.emplace_back(RealCut(cutAbscissa, nbItemsSeen));
                        prevAbscissa = cutAbscissa;
                    }
                    
                }         
            }
        }

        if (cut.isBegin()) {
            openedCuts++;
            maxItemSeen = std::max(maxItemSeen, (int)cut.getItemSequencePosition());
            nbItemsSeen++;
            // TODO warning ici...
        }
    }
    realCuts.emplace_back(RealCut(firstAbscissa, nbItemsSeen));
    /*std::cout << "====|===" << std::endl;
    displayRealCuts();*/
}

unsigned int GlassNode::cutRealCuts(const GlassLocationIt& first, unsigned int nbItemsToCuts, bool lazy) {
    unsigned int nbItemsCuts = 0;
    unsigned int prevAbscissa = isVerticalCut() ? x + width : y + height;
    unsigned int nbPrevItems = 0;
    
    for (const RealCut& cut: realCuts) {
        if (cut.x > (isVerticalCut() ? x + width : y + height)) { assert(false); continue; }
        
        if (cut.x != prevAbscissa) {
            assert(nbItemsToCuts >= cut.nbItems);
            if (prevAbscissa <= cut.x) {
                std::cout << (*this) << std::endl;
                std::cout << prevAbscissa << " " << cut.x << std::endl;
                displayRealCuts();
            }
            assert(prevAbscissa > cut.x);
            nbItemsCuts += addSonAndBuildNode(cut.x, prevAbscissa - cut.x, 
                beginSequencePosition + nbItemsToCuts - cut.nbItems, beginSequencePosition + nbItemsToCuts - nbPrevItems, lazy);
            prevAbscissa = cut.x;
            nbPrevItems = cut.nbItems;
        }
    }
    
    unsigned int endNodeAbscissa = isVerticalCut() ? x : y;
    assert(prevAbscissa == endNodeAbscissa);
    return nbItemsCuts;
}

unsigned int GlassNode::addSonAndBuildNode(unsigned int beginAbscissa, unsigned int size,
    unsigned int beginItem, unsigned int endItem, bool lazy) {

    if (size > (isVerticalCut() ? width : height)) {
        return 0;
    }

    if (beginItem != endItem)
        xMax = std::max(xMax, beginAbscissa + size);
    
    std::vector<GlassLocation> items;
    if (lazy && depth == 0) {
        const GlassLocationIt beginIt = cutter->getLocationIterator(plateIndex, beginItem);
        const GlassLocationIt endIt = cutter->getLocationIterator(plateIndex, endItem);
        items.insert(items.end(), beginIt, endIt);
    }
    BuiltNode builtNode(beginAbscissa, size, items);
    
    if (lazy && depth == 0 && builtNodes.find(builtNode) != builtNodes.end()) {
        assert(builtNodes.size() != 0);
        return builtNode.items.size();
    }

    assert(size <= (isVerticalCut() ? width : height));

    if (isVerticalCut())
        sons.emplace_back(GlassNode(instance, cutter, plateIndex, beginAbscissa, y, size, height, depth + 1, BRANCH));
    else
        sons.emplace_back(GlassNode(instance, cutter, plateIndex, x, beginAbscissa, width, size, depth + 1, BRANCH));
    
    unsigned nbItemsCuts = sons.back().buildNodeAndReturnNbItemsCuts(beginItem, endItem, lazy);
    if (nbItemsCuts == endItem - beginItem && lazy && depth == 0){
        if (builtNodes.size() > MAX_BUILT_SIZE) builtNodes.clear();
        builtNodes.insert(builtNode);
    }
    return nbItemsCuts;
}

unsigned int GlassNode::checkNodeAndReturnNbItemsCuts(unsigned int begin, unsigned int end) {
    return buildNodeAndReturnNbItemsCuts(begin, end, BUILD_LAZY);
}

unsigned int GlassNode::buildNodeAndReturnNbItemsCuts(unsigned int begin, unsigned int end, bool lazy) {
    xMax = 0;
    //std::cout << "buildNode for " << *this << " : " << begin << " " << std::endl;
    beginSequencePosition = begin;
    endSequencePosition = end;
    
    sons.clear();
    checkTooSmall();

    if (beginSequencePosition == endSequencePosition) {
        type = WASTE;
        return 0;
    }

    GlassLocationIt first = getFirst();
    GlassLocationIt last = getLast();

    const GlassLocation& location = *(first + end - begin - 1);
    /*if (location.getX() == 1828 && location.getY() == 2256 && location.getXW() == 4126 && location.getYH() == 2684)
        std::cout << "Okue" << std::endl;*/
    /*if (location.getX() == 1848 && location.getY() == 2256 && location.getXW() == 4146 && location.getYH() == 2684)
        std::cout << "OUI" << std::endl;*/

    if (first + 1 == last && isNodeFitLocation(*first)) {
        setType(first->getItemIndex());
        return 1;
    }

    checkTooDepth();
    preCheckTrimming(first, last);
    checkDimensions(first, last);
    buildCutsAvailable(first, last);
    //displayCutsAvailable();
    buildRealCuts();
    std::sort(realCuts.begin(), realCuts.end());
    //displayRealCuts();
    //std::cout << "===========" << std::endl;
    unsigned int nbItemsCuts = cutRealCuts(first, endSequencePosition - beginSequencePosition, lazy);
    if (endSequencePosition - beginSequencePosition != nbItemsCuts){
        throw std::runtime_error("Infeasible cut (not enough items cut)");
    }
    return nbItemsCuts;
}

void GlassNode::displayCutsAvailable() const {
    for (const GlassCut& cut: cutsAvailable) {
        std::cout << "cut. abscissa " << cut.getAbscissa();
        std::cout << " & begin " << cut.isBegin() << " & item " << cut.getItemSequencePosition() << std::endl;
    }
}

void GlassNode::displayRealCuts() const {
    for (const RealCut& cut: realCuts) {
        std::cout << "cut. abscissa " << cut.x << " & nbItems " << cut.nbItems << std::endl; 
    }
}

void GlassNode::displayNode() const {
    displayNode(" ");
}

void GlassNode::displayNode(std::string prefix) const {
    std::cout << prefix << plateIndex << " " << x << " " << y << " " << x + width << " " << y + height << std::endl;
    for (const GlassNode& son: sons) {
        son.displayNode(prefix + " ");
    }
}

unsigned int GlassNode::saveNode(std::ofstream& outputFile, unsigned int nodeId, int parentId, bool last) {
    outputFile << plateIndex << ";" << nodeId << ";";
    outputFile << x << ";" << y << ";" << width << ";" << height << ";";
    outputFile << type << ";" << depth << ";";
    
    if (parentId < 0)
        outputFile << "" << std::endl;
    else
        outputFile << parentId << std::endl;

    if (last && !sons.front().hasSons())
        sons.front().setType(RESIDUAL);

    unsigned int maxSonId = nodeId;
    std::reverse(sons.begin(), sons.end());
    for (GlassNode& son: sons) {
        maxSonId = son.saveNode(outputFile, maxSonId + 1, nodeId, !LAST_BIN);
    }
    return maxSonId;
}

std::ostream& operator<<(std::ostream& os, const GlassNode& node) {
    os << node.getPlateIndex() << " " <<  node.getDepth() << " " << node.getX() << " " << node.getY() << " ";
    os << node.getX() + node.getWidth() << " " << node.getY() + node.getHeight();
    return os;
}

double GlassNode::getSurfaceOccupation() {
    assert(instance != NULL);
    if (type == RESIDUAL) return 1;
    double itemsArea = 0.;
    for (GlassLocationIt it = getFirst() ; it < getLast() ; it++) {
        assert(it->getItemIndex() < instance->getNbItems());
        itemsArea += instance->getItem(it->getItemIndex()).getArea();
    }
    return itemsArea / (width * height);
}

// WARNING: nodes are in reversed order
unsigned int GlassNode::getXMax() const {
    return xMax;
}

GlassLocationIt GlassNode::getFirst() {
    return cutter->getLocationIterator(plateIndex, beginSequencePosition);
}

GlassLocationIt GlassNode::getLast() {
    return cutter->getLocationIterator(plateIndex, endSequencePosition);
}

GlassNode::BuiltNode::BuiltNode(): beginAbscissa(0), size(0), items(std::vector<GlassLocation>()){

}

GlassNode::BuiltNode::BuiltNode(unsigned int beginAbscissa, unsigned int size, 
    std::vector<GlassLocation> items) 
    : beginAbscissa(beginAbscissa), size(size), items(items) {

}

bool GlassNode::BuiltNode::operator<(const BuiltNode& other) const {
    if (beginAbscissa == other.beginAbscissa) {
        if (size == other.size) {
            if (items.size() == other.items.size()) {
                for (unsigned int index = 0; index < items.size(); index++) {
                    if (items[index] == other.items[index]) continue;
                    return items[index] < other.items[index];
                }
            }
            return items.size() < other.items.size();
        }
        return size < other.size;
    }
    return beginAbscissa < other.beginAbscissa;
}

std::ostream& operator<<(std::ostream& os, const GlassNode::BuiltNode& node) {
    os << node.beginAbscissa << " " << node.size << "[";
    for (const GlassLocation& location: node.items)
        os << " " << location.getItemIndex();
    os << "] ";
}

bool RealCut::operator<(const RealCut& other) const {
    return x > other.x;
}







