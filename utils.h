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







template <class C>
class Enumerate {
public:
    explicit Enumerate(C& c, size_t index = 0) : c_(c), index_(index) {
    }

    Enumerate& operator++() {
        ++index_;
        return *this;
    }

    auto operator*() {
        return std::tie(index_, c_.at(index_));
    }

    bool operator!=(const Enumerate& rhs) const {
        return index_ != rhs.index_;
    }

    auto begin() { return Enumerate(c_); }

    auto end() { return Enumerate(c_, c_.size()); }

private:
    C& c_;
    size_t index_ = 0;
};
















class State {
public:
    explicit State(GameManager& game) : game_(game) {
    }

    virtual void Process(const sf::Event& event) = 0;

private:
    GameManager& game_;
};


class WaitPieceClick : public State {
public:
    explicit WaitPieceClick(GameManager& game) : State(game) {
    }

    virtual void Process() const {

    }

private:

};

class WaitCellClick : public State {
public:
    explicit WaitCellClick(GameManager& game) : State(game) {
    }

    virtual void Process() const {

    }
};

class NeedJump : public State {
public:
    explicit NeedJump(GameManager& game) : State(game) {
    }

    virtual void Process() const {

    }
};

class EndTurn : public State {
public:
    explicit EndTurn(GameManager& game) : State(game) {
    }

    virtual void Process() const {

    }
};






