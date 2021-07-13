#include <mynn/matrix/matrix_utils.h>
#include <nn/src/nn_modules/nn.h>

#include <SFML/Graphics.hpp>

#include <cassert>
#include <functional>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace color {

static const auto LIGHT_GREY = sf::Color(0xD3D3D3FF);
static const auto PEACH_PUFF = sf::Color(0xFFDAB9FF);
static const auto WHITE_SMOKE = sf::Color(0xF5F5F5FF);
static const auto LIGHT_DIM_GREY = sf::Color(0xC0C0C0FF);
static const auto DIM_GREY = sf::Color(0x696969FF);
static const auto GREY = sf::Color(0x808080FF);
static const auto SOFT_CYAN = sf::Color(0xB2F3F3FF);
static const auto ULTRA_RED = sf::Color(0xFC6C84FF);
static const auto BABY_BLUE = sf::Color(0x82D1F1FF);
static const auto RAINBOW_INDIGO = sf::Color(0x1e3f66FF);
static const auto SOFT_SEA_FOAM = sf::Color(0xDDFFEFFF);
static const auto SOFT_YELLOW = sf::Color(0xFFFFBFFF);

static const auto AVAILABLE_MOVE = SOFT_SEA_FOAM;

}  // namespace color

constexpr float PIECE_RADIUS = 30;

int ToCellId(int x, int y) {
    return (y / 80) * 8 + x / 80;
}

struct Piece {
    sf::CircleShape shape;
    int cellId = -1;
    bool isQueen = false;
};

struct PathNode {
    PathNode() = default;

    explicit PathNode(int cellId) : cellId(cellId) {
    }

    std::vector<std::unique_ptr<PathNode>> children;
    int cellId = -1;
    bool isEmptyCell = true;
};

class GameManager {
public:
    GameManager(size_t numRows, size_t numCols)
        : size_(numRows * numCols)
        , numRows_(numRows)
        , numCols_(numCols)
        , numBlackPieces_(12)
        , paths_(size_)
        , board_(size_, -1)
    {
        boardSquares_.reserve(size_);
        blackPieces_.reserve(12);
        whitePieces_.reserve(12);
        allPieces_.reserve(whitePieces_.capacity() + blackPieces_.capacity());
    }

    void InitBoard() {
        auto createPiece = [&](auto& arr, auto pos, auto color, auto outlineColor) {
            auto pieceId = allPieces_.size();
            auto& piece = allPieces_.emplace_back();
            arr.emplace_back(pieceId);

            piece.shape = sf::CircleShape(PIECE_RADIUS);
            piece.shape.setPosition(pos);
            piece.shape.setFillColor(color);
            piece.shape.setOutlineColor(outlineColor);
            piece.shape.setOutlineThickness(2.f);

            auto cellId = ToCellId(pos.x, pos.y);
            piece.cellId = cellId;
            board_.at(cellId) = pieceId;
        };

        size_t skipRowsFrom = 3;
        size_t skipRowsTo = 5;
        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                auto cellId = i * 8 + j;
                paths_.at(cellId) = std::make_unique<PathNode>(cellId);

                sf::Vector2f position = {j * 80.f, i * 80.f};
                boardSquares_.emplace_back(sf::Vector2f{80.0f, 80.0f});
                boardSquares_.back().setPosition(position);
                position += sf::Vector2f{10, 10};
                drawable_.emplace_back(boardSquares_.back());
                if ((i + j) & 1) {
                    boardSquares_.back().setFillColor(color::LIGHT_GREY);
                    if (i < skipRowsFrom) {
                        createPiece(blackPieces_, position, color::DIM_GREY, color::GREY);
                    } else if (i >= skipRowsTo) {
                        createPiece(whitePieces_, position, color::WHITE_SMOKE, color::LIGHT_DIM_GREY);
                    }
                } else {
                    boardSquares_.back().setFillColor(color::PEACH_PUFF);
                    board_.at(cellId) = -2;
                }
            }
        }

        for (auto& piece : allPieces_) {
            drawable_.emplace_back(piece.shape);
        }
    }

    void ProcessClick(int cellId) {
        if (transitions_.contains(cellId)) {
            auto handler = transitions_.at(cellId);
            handler(cellId);
        }
    }

    void Render(auto& window) {
        for (auto& obj : drawable_) {
            window.draw(obj);
        }
    }

    void Start() {
        Turn();
    }

    bool IsWhitesTurn() const {
        return whitesTurn_;
    }

    class State {
    public:
        explicit State(const GameManager& game) : game_(game) {
        }

//        const auto& GetAvailablePieces() const {
//            return game_.availablePieces_;
//        }

        const auto& GetPaths() const {
            return game_.paths_;
        }

        const auto& GetBoard() const {
            return game_.board_;
        }

        bool IsWhite(int pieceId) const {
            return game_.IsWhite(pieceId);
        }

    private:
        const GameManager& game_;
    };

    std::unique_ptr<State> GetState() const {
        return std::make_unique<State>(*this);
    }

protected:
    // Builds path trees from all available pieces and sets availablePieces_.
    void CalculateMoves() {
        auto& pieces = GetPlayerPieces();

        CalcJumps(pieces);
        if (availablePieces_.empty()) {
            for (int pieceId : pieces) {
                CalcAvailableSpaces(pieceId);
            }
        }
        if (availablePieces_.empty()) {
            throw std::runtime_error("Lost!");
        }
    }

    void CalcAvailableSpaces(int pieceId) {
        auto dirs = FORWARD;
        if (!whitesTurn_) {
            dirs = BACKWARD;
        }
        auto& piece = allPieces_.at(pieceId);
        if (piece.isQueen) {
            dirs = BOTH_DIRS;
        }
        int maxSteps = 1;
        if (piece.isQueen) {
            maxSteps = std::max(numRows_, numCols_);
        }
        for (int dir : dirs) {
            for (int step = 1; step <= maxSteps; ++step) {
                int to = piece.cellId + dir * step;
                if (CanMoveTo(to)) {
                    paths_.at(piece.cellId)->children.emplace_back(std::make_unique<PathNode>(to));
                    availablePieces_.insert(pieceId);
                } else {
                    break;
                }
            }
        }
    }

    bool CanMoveTo(int cellId) const {
        return IsValidCell(cellId) && IsEmpty(cellId);
    }

    void CalcJumps(const std::vector<int>& pieces) {
        std::unordered_set<int> eaten;
        for (auto pieceId : pieces) {
            int maxSteps = 2;
            if (allPieces_[pieceId].isQueen) {
                maxSteps = std::max(numRows_, numCols_);
            }
            CalcJumps(paths_.at(allPieces_.at(pieceId).cellId), eaten, maxSteps, 0);
            if (!paths_.at(allPieces_.at(pieceId).cellId)->children.empty()) {
                availablePieces_.insert(pieceId);
            }
        }
    }

    void CalcJumps(std::unique_ptr<PathNode>& node, std::unordered_set<int>& eaten, int maxSteps, int forbiddenDir) {
        for (int dir : BOTH_DIRS) {
            if (dir == forbiddenDir) {
                continue;
            }

            std::unique_ptr<PathNode> enemy;
            for (int step = 1; step <= maxSteps; ++step) {
                int cell = node->cellId + dir * step;
                if (!IsValidCell(cell)) {
                    break;
                } else if (!IsEmpty(cell)) {
                    if (IsEnemy(cell)) {
                        if (enemy || eaten.contains(cell)) {
                            break;
                        }
                        enemy = std::make_unique<PathNode>(cell);
                        enemy->isEmptyCell = false;
                        eaten.insert(enemy->cellId);
                    } else {
                        break;
                    }
                } else if (enemy) {
                    auto& next = enemy->children.emplace_back(std::make_unique<PathNode>(cell));
                    CalcJumps(next, eaten, maxSteps, -dir);
                }
            }
            if (enemy) {
                eaten.erase(enemy->cellId);
                bool hasJumpsAfter = false;
                for (auto& child : enemy->children) {
                    if (!child->children.empty()) {
                        hasJumpsAfter = true;
                        break;
                    }
                }
                if (hasJumpsAfter) {
                    std::erase_if(enemy->children, [&](auto& ptr) {
                        return ptr->children.empty();
                    });
                }
                if (!enemy->children.empty()) {
                    node->children.emplace_back(std::move(enemy));
                }
            }
        }
    }

    void ClickPossiblePiece(int cellId) {
        for (auto& move : paths_[selectedPiece_.cellId]->children) {
            transitions_.erase(move->cellId);
        }
        RemoveHighlightFromMoves();
        selectedPiece_.cellId = cellId;
        ShowMoves(cellId);
        AddMovesEventTransitions(cellId);
    }

    void ClickHighlightedCell(int cellId) {
        transitions_.clear();
        RemoveHighlightFromPieces();
        RemoveHighlightFromMoves();
        MakeMove(cellId);
        if (mustJumpFrom_ != -1) {
            ShowMoves(mustJumpFrom_);
            AddMovesEventTransitions(mustJumpFrom_);
        } else {
            ChangePlayer();
            Turn();
        }
    }

    void ChangePlayer() {
        whitesTurn_ = !whitesTurn_;
    }

    void ClickHighlightedPiece(int cellId) {
        if (selectedPiece_.cellId != cellId) {
            if (selectedPiece_.cellId != -1) {
                ClickPossiblePiece(cellId);
            } else {
                selectedPiece_.cellId = cellId;
                ShowMoves(cellId);
                AddMovesEventTransitions(cellId);
            }
        }
    }

    void MakeMove(int to) {
        const auto from = selectedPiece_.cellId;

        const auto pieceId = RemovePiece(from);

        std::unique_ptr<PathNode> node;
        availablePieces_.erase(pieceId);
        for (auto id : availablePieces_) {
            paths_.at(allPieces_.at(id).cellId)->children.clear();
        }
        availablePieces_.clear();

        for (auto& move : paths_.at(from)->children) {
            if (!move->isEmptyCell) {
                for (auto& jump : move->children) {
                    if (jump->cellId == to) {
                        eaten_.insert(move->cellId);
                        RemovePiece(move->cellId);
                        node = std::move(jump);
                        break;
                    }
                }
            } else {
                boardSquares_.at(move->cellId).setFillColor(color::LIGHT_GREY);
                if (move->cellId == to) {
                    node = std::move(move);
                    break;
                }
            }
        }

        assert(paths_.at(to)->children.empty());
        paths_.at(to)->children = std::move(node->children);
        paths_.at(from)->children.clear();

        auto& piece = allPieces_[pieceId];
        if ((whitesTurn_ && to / numCols_ == 0) || (!whitesTurn_ && to / numCols_ == numRows_ - 1)) {
            if (!piece.isQueen) {
                piece.isQueen = true;
                if (whitesTurn_) {
                    piece.shape.setFillColor(color::SOFT_YELLOW);
                } else {
                    piece.shape.setFillColor(color::RAINBOW_INDIGO);
                }

                if (mustJumpFrom_ != -1) {
                    paths_.at(to)->children.clear();
                    CalcJumps(paths_.at(to), eaten_, std::max(numRows_, numCols_), 0);
                }
            }
        }

        if (paths_.at(to)->children.empty()) {
            mustJumpFrom_ = -1;
            selectedPiece_.cellId = -1;
            eaten_.clear();
        } else {
            mustJumpFrom_ = to;
            selectedPiece_.cellId = to;
        }

        AddPiece(to, pieceId);
    }

    void AddPiece(int to, int pieceId) {
        auto v = GetPositionVector2(to);
        allPieces_.at(pieceId).shape.setPosition(v);
        if (IsWhite(pieceId)) {
            whitePieces_.push_back(pieceId);
        } else {
            blackPieces_.push_back(pieceId);
        }
        assert(pieceId >= 0);
        board_.at(to) = pieceId;

        allPieces_.at(pieceId).cellId = to;
    }

    sf::Vector2f GetPositionVector2(int cellId) const {
        float row = cellId / numCols_;
        float col = cellId % numCols_;
        return {col * 80 + 10, row * 80 + 10};
    }

    int RemovePiece(int cellId) {
        auto pieceId = board_.at(cellId);
        assert(pieceId >= 0);
        board_.at(cellId) = -1;

        allPieces_.at(pieceId).cellId = -1;
        allPieces_.at(pieceId).shape.setPosition(-100, -100);
        if (IsWhite(pieceId)) {
            std::erase(blackPieces_, pieceId);
        } else {
            std::erase(whitePieces_, pieceId);
        }

        return pieceId;
    }

    void RemoveHighlightFromMoves() {
        for (auto& move : paths_.at(selectedPiece_.cellId)->children) {
            if (!move->isEmptyCell) {
                for (auto& jump : move->children) {
                    boardSquares_.at(jump->cellId).setFillColor(color::LIGHT_GREY);
                }
            } else {
                boardSquares_.at(move->cellId).setFillColor(color::LIGHT_GREY);
            }
        }
    }

    void RemoveHighlightFromPieces() {
        for (auto pieceId : availablePieces_) {
            allPieces_.at(pieceId).shape.setOutlineColor(color::LIGHT_DIM_GREY);
        }
    }

    void ShowMoves(int cellId) {
        for (auto& move : paths_.at(cellId)->children) {
            if (!move->isEmptyCell) {
                for (auto& jump : move->children) {
                    boardSquares_.at(jump->cellId).setFillColor(color::AVAILABLE_MOVE);
                }
            } else {
                boardSquares_.at(move->cellId).setFillColor(color::AVAILABLE_MOVE);
            }
        }
    }

    void AddMovesEventTransitions(int cellId) {
        for (auto& move : paths_.at(cellId)->children) {
            if (!move->isEmptyCell) {
                for (auto& jump : move->children) {
                    transitions_[jump->cellId] = [this](int cellId) {
                        ClickHighlightedCell(cellId);
                    };
                }
            } else {
                transitions_[move->cellId] = [this](int cellId) {
                    ClickHighlightedCell(cellId);
                };
            }
        }
    }

    std::vector<int>& GetPlayerPieces() {
        if (whitesTurn_) {
            return whitePieces_;
        }
        return blackPieces_;
    }

    void HighlightPieces() {
        for (auto pieceId : availablePieces_) {
            allPieces_.at(pieceId).shape.setOutlineColor(sf::Color::Green);
            transitions_[allPieces_.at(pieceId).cellId] = [this](int cellId) {
                ClickHighlightedPiece(cellId);
            };
        }
    }

    bool IsEmpty(int cellId) const {
        return board_.at(cellId) == -1;
    }

    bool IsValidCell(int cellId) const {
        return cellId >= 0 && cellId < size_ && board_.at(cellId) != -2;
    }

    bool IsEnemy(int cellId) const {
        return whitesTurn_ ^ IsWhite(board_.at(cellId));
    }

    bool IsWhite(int pieceId) const {
        return pieceId >= numBlackPieces_;
    }

    void Turn() {
        CalculateMoves();
        HighlightPieces();
    }

    static inline constexpr auto FORWARD = {-9, -7};
    static inline constexpr auto BACKWARD = {7, 9};
    static inline constexpr auto BOTH_DIRS = {-9, -7, 7, 9};

    const size_t size_;
    const size_t numRows_;
    const size_t numCols_;
    const int numBlackPieces_;
    std::vector<sf::RectangleShape> boardSquares_;
    std::vector<Piece> allPieces_;
    std::vector<int> whitePieces_;
    std::vector<int> blackPieces_;
    std::vector<std::reference_wrapper<sf::Drawable>> drawable_;

    // State
    std::vector<int> board_;
    bool whitesTurn_ = true;
    int mustJumpFrom_ = -1;
    std::unordered_set<int> eaten_;
    std::vector<std::unique_ptr<PathNode>> paths_;
    std::unordered_set<int> availablePieces_;
    Piece selectedPiece_;
    using ClickHandler = std::function<void(int cellId)>;
    std::unordered_map<int, ClickHandler> transitions_;
};

class Events {
public:
    Events(sf::Window& window) : window_(window) {
    }

    bool Poll() {
        if (window_.pollEvent(event_)) {
            polled_ = true;
            if (event_.type == sf::Event::Closed) {
                window_.close();
                return false;
            }
        }
        return true;
    }

    bool WaitEvent(sf::Event& event) {
        while (!polled_) {
            if (!Poll()) {
                return false;
            }
            sf::sleep(sf::milliseconds(30));
        }
        event = event_;
        polled_ = false;
        return true;
    }

private:
    sf::Window& window_;
    sf::Event event_;
    bool polled_ = false;
};

class Player {
public:
    virtual ~Player() = default;

    virtual int Turn(std::unique_ptr<GameManager::State> state) = 0;
};

class Human : public Player {
public:
    explicit Human(Events& events) : events_(events) {
    }

    int Turn(std::unique_ptr<GameManager::State> state) override {
        sf::Event event{};
        if (events_.WaitEvent(event)) {
            if (event.type == sf::Event::MouseButtonPressed) {
                return ToCellId(event.mouseButton.x, event.mouseButton.y);
            }
        }
        return -1;
    }

private:
    Events& events_;
};

class SimpleBot : public Player {
public:
    explicit SimpleBot(Events&) {
    }

    int Turn(std::unique_ptr<GameManager::State> state) override {
        sf::sleep(sf::milliseconds(300));
        static int turnFrom = -1;
        const auto& paths = state->GetPaths();
        for (const auto& from : paths) {
            if (!from->children.empty()) {
                if (turnFrom == -1) {
                    turnFrom = from->cellId;
                    return turnFrom;
                }
                turnFrom = -1;
                const auto& child = from->children.at(0);
                if (child->isEmptyCell) {
                    return child->cellId;
                }
                return child->children.at(0)->cellId;
            }
        }
        return -1;
    }
};

class AiBot : public Player {
public:
    explicit AiBot(Module& nn) : nn_(nn) {
    }

    int Turn(std::unique_ptr<GameManager::State> state) override {
        if (turns_.empty()) {
            CalcTruns(state);
        }
        if (turns_.empty()) {
            return -1;
        }
        auto turn = turns_.front();
        turns_.erase(turns_.begin());
        return turn;
    }

private:
    void CalcTruns(const std::unique_ptr<GameManager::State>& state) {
        const auto& board = state->GetBoard();
        std::vector<std::vector<float>> input;
        input.reserve(board.size());
        for (int pieceId : board) {
            if (pieceId != -2) {
                if (pieceId == -1) {
                    input.push_back({1, 0, 0});
                } else if (state->IsWhite(pieceId)) {
                    input.push_back({0, 1, 0});
                } else {
                    input.push_back({0, 0, 1});
                }
            } else {
                input.push_back({0, 0, 0});
            }
        }

        std::vector<int> path;
        double max = 0.0;
        for (const auto& from : state->GetPaths()) {
            LeavesTraverse(from, path, [&]() {
                if (path.size() > 1) {
                    auto ind = state->IsWhite(board[path.front()]) ? 1 : 2;
                    auto after = input;
                    after[path.front()][ind] = 0;
                    after[path.front()][0] = 1;
                    after[path.back()][ind] = 1;
                    after[path.back()][0] = 0;

                    for (size_t i = 1; i + 1 < path.size(); i += 2) {
                        after[path[i]][3 - ind] = 0;
                        after[path[i]][0] = 1;
                    }
                    auto matrix = CreateMatrixFromData(after);
                    double prob = nn_.Forward(matrix)[0];
                    if (prob > max) {
                        max = prob;
                        turns_ = path;
                    }
                }
            });
        }

        for (size_t i = 1; i + 1 < turns_.size(); ++i) {
            turns_.erase(turns_.begin() + i);
        }
    }

    template <class Callback>
    void LeavesTraverse(const std::unique_ptr<PathNode>& cur, std::vector<int>& path, Callback cb) {
        path.push_back(cur->cellId);
        if (cur->children.empty()) {
            cb();
            return;
        }
        for (const auto& child : cur->children) {
            LeavesTraverse(child, path, cb);
        }
        path.pop_back();
    }

    std::vector<int> turns_;
    Module& nn_;
};

class Controller {
public:
    Controller(GameManager& game, std::unique_ptr<Player> white, std::unique_ptr<Player> black)
        : game_(game)
        , whitePlayer_(std::move(white))
        , blackPlayer_(std::move(black))
    {
    }

    void NextMove() {
        int cellId;
        if (game_.IsWhitesTurn()) {
            cellId = whitePlayer_->Turn(game_.GetState());
        } else {
            cellId = blackPlayer_->Turn(game_.GetState());
        }
        if (cellId == -1) {
            return;
        }
        game_.ProcessClick(cellId);
    }

private:
    GameManager& game_;
    std::unique_ptr<Player> whitePlayer_;
    std::unique_ptr<Player> blackPlayer_;
};

template <class SecondPlayer>
std::unique_ptr<Controller> PlayWith(GameManager& game, Events& events) {
    return std::make_unique<Controller>(
        game,
        std::make_unique<Human>(events),
        std::make_unique<SecondPlayer>(events));
}

std::unique_ptr<Controller> PlayWith(GameManager& game, Events& events, std::unique_ptr<Player> secondPlayer) {
    return std::make_unique<Controller>(
        game,
        std::make_unique<Human>(events),
        std::move(secondPlayer));
}

int main(int argc, char** argv) {
    std::string bot;
    if (argc > 1) {
        bot = argv[1];
    }

    sf::ContextSettings settings;
    settings.antialiasingLevel = 16;
    sf::RenderWindow window(sf::VideoMode(640, 640), "SFML works!", sf::Style::Default, settings);

    GameManager game(8, 8);
    game.InitBoard();
    game.Start();
    Events events(window);

    std::unique_ptr<Controller> controller;
    if (bot == "simple") {
        controller = PlayWith<SimpleBot>(game, events);
    } else if (bot == "ai") {
        Sequential nn;
        nn
            .AddModule(Linear(2, 2))
            .AddModule(ReLU())
            .AddModule(ToProbabilities());
        controller = PlayWith(game, events, std::make_unique<AiBot>(nn));
    } else {
        controller = PlayWith<Human>(game, events);
    }

    while (window.isOpen()) {
        window.clear();
        game.Render(window);
        window.display();

        if (events.Poll()) {
            controller->NextMove();
        }
    }
}
