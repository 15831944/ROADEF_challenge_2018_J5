#include "heuristic.h"

#include "../GlassCutter/glassCutter.h"
#include "../GlassKernel/glassMove.h"
#include "../GlassData/glassConstants.h"
#include "../GlassKernel/GlassMoves/swap.h"
#include "../GlassKernel/GlassMoves/kConsecutivePermutation.h"

#include <cstdlib>
#include <sstream>
#include <fstream>
#include <iostream>

unsigned int Heuristic::glassRandint(unsigned int begin, unsigned int last) {
    return rand() % (last - begin) + begin;
}

Heuristic::Heuristic(GlassInstance instance): instance(instance), cutter(&instance, sequence){
    initRandomlySequence();
    buildMoves();
    localSearch(4);
    displayMoveStatistics();
    computeScore(4);
}

void Heuristic::buildMoves() {
    poolMoves.clear();
    poolMoves.push_back(new Swap(this));
    for (unsigned int k = 2; k <= 4; k++) 
        poolMoves.push_back(new KConsecutivePermutation(this, k));
}

void Heuristic::initRandomlySequence() {
    sequence.clear();
    unsigned int nbItems = instance.getNbItems();

    std::vector<bool> itemsSelected;
    itemsSelected.resize(nbItems);
    for (unsigned int i = 0; i < itemsSelected.size(); i++) 
        itemsSelected[i] = false;

    unsigned int nbItemsSelected = 0;
    while (nbItemsSelected < nbItems) {
        unsigned int itemIndex = glassRandint(0, nbItems);
        if (itemsSelected[itemIndex]) continue;
        sequence.push_back(instance.getItem(itemIndex).getStackId());
        itemsSelected[itemIndex] = true;
        nbItemsSelected++;
    }
}

void Heuristic::displaySequence() {
    if (sequence.size() == 0) {
        std::cout << "Sequence courante vide" << std::endl;
        return;
    }

    std::cout << "Sequence courante : [";
    for (unsigned int index = 0; index < sequence.size() - 1; index++)
        std::cout << sequence[index] << ", ";
    std::cout << sequence[sequence.size() - 1] << "]" << std::endl;
}

unsigned int Heuristic::computeScore(unsigned int depth) {
    return computeScore(depth, 0);
}

unsigned int Heuristic::computeScore(unsigned int depth, unsigned int beginSequenceIndex) {
    std::cout <<  beginSequenceIndex << std::endl;
    cutter.setSequence(sequence); // TODO a priori inutile
    cutter.revertPlatesUntilSequenceIndex(beginSequenceIndex);
    cutter.cut(depth);
    return cutter.getCurrentScore();
}

void Heuristic::localSearch(unsigned int depth) {
    bestScore = computeScore(depth);
    int beginSequenceIndex = 0;
    for (unsigned int k = 0; k < 500; k++) {
        unsigned int moveIndex = glassRandint(0, poolMoves.size());
        GlassMove* move = poolMoves[moveIndex];
        int startingFrom = move->attempt();
        if (startingFrom == NOTHING) continue;
        unsigned int score = computeScore(depth, (unsigned int) std::min(beginSequenceIndex, startingFrom));
        if (score <= bestScore) {
            move->commit();
            move->addStat(score < bestScore ? IMPROVE : ACCEPTED);
            std::cout << " Nouveau score " << score << " vs " << bestScore << std::endl;
            bestScore = score;
            beginSequenceIndex = sequence.size();
        } else {
            move->revert();
            move->addStat(REFUSED);
            beginSequenceIndex = std::min(beginSequenceIndex, startingFrom);
        }
    }
    std::cout << " Best score " << bestScore << std::endl;
    std::cout << " Instance area : " << instance.getItemsArea() << std::endl;
}

void Heuristic::displayMoveStatistics() {
    for (GlassMove* move: poolMoves) {
        move->displayStatistics();
    }
}

void Heuristic::saveBest(std::string name) {
    cutter.saveBest(name);
}