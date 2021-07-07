#include <iostream>

states:
    game starts
    wait player click piece
    wait player click cell
    needJump:
        wait player click cell
    next player turn

    for state : stateQueue {
        state.ProcessEvent(event);
    }

    Finish produces states, calls prepare on them
    can make preparations in constructor and just wait




























