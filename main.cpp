#include "utils.h"

#include <mynn/mynn.h>

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>

static constexpr int INPUT_DIM = 5;
static constexpr int INPUT_ROWS = 32;

int ToCellId(int x, int y, int numCols) {
    return (y / 80) * numCols + x / 80;
}

sf::Vector2f ToVector(int cellId, int numCols) {
    auto row = static_cast<float>(cellId / numCols);
    auto col = static_cast<float>(cellId % numCols);
    return {col * 80.0F + 10.0F, row * 80.0F + 10.0F};
}

struct Piece {
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

class Renderer {
public:
    virtual void RemoveHighlightFromPieces(const std::unordered_set<int>& availablePieces) {
    }

    virtual void RemoveHighlightFromMoves(const std::unique_ptr<PathNode>& moves) {
    }

    virtual void ShowMoves(const std::unique_ptr<PathNode>& moves) {
    }

    virtual void InitBoard(const std::string& boardFilename, const std::vector<Piece>& whitePieces,
                           const std::vector<Piece>& blackPieces,
                           int numRows, int numCols) {
    }

    virtual void Render() = 0;

    virtual void SetWhitesQueen(int pieceId) {
    }

    virtual void SetBlacksQueen(int pieceId) {
    }

    virtual void HighlightPieces(const std::unordered_set<int>& availablePieces) {
    }

    virtual void SetPiecePosition(int pieceId, int cellId) {
    }

    virtual void ErasePiece(int pieceId) {
    }
};

class EmptyRenderer : public Renderer {
public:
    EmptyRenderer() = default;

    void Render() override {
    }
};

class BoardRenderer : public Renderer {
public:
    explicit BoardRenderer(sf::RenderWindow& window)
        : window_(window) {
    }

    void RemoveHighlightFromPieces(const std::unordered_set<int>& availablePieces) override {
        for (auto pieceId : availablePieces) {
            pieces_.at(pieceId).setOutlineColor(color::LIGHT_DIM_GREY);
        }
    }

    void RemoveHighlightFromMoves(const std::unique_ptr<PathNode>& moves) override {
        for (auto& move : moves->children) {
            if (!move->isEmptyCell) {
                for (auto& jump : move->children) {
                    squaresToDraw_.erase(jump->cellId);
                }
            } else {
                squaresToDraw_.erase(move->cellId);
            }
        }
    }

    void ShowMoves(const std::unique_ptr<PathNode>& moves) override {
        for (auto& move : moves->children) {
            if (!move->isEmptyCell) {
                for (auto& jump : move->children) {
                    squaresToDraw_.insert(jump->cellId);
                }
            } else {
                squaresToDraw_.insert(move->cellId);
            }
        }
    }

    void InitBoard(const std::string& boardFilename, const std::vector<Piece>& whitePieces,
                   const std::vector<Piece>& blackPieces,
                   int numRows, int numCols) override {
        numCols_ = numCols;
        auto loaded = texture_.loadFromFile(boardFilename);
        if (!loaded) {
            throw std::runtime_error("cannot load from " + boardFilename);
        }
        Log() << "Loaded board from " << boardFilename;
        sprite_.setTexture(texture_);

        for (int i = 0; i < numRows; ++i) {
            for (int j = 0; j < numCols; ++j) {
                sf::Vector2f position = {j * CELL_SIZE, i * CELL_SIZE};
                boardSquares_.emplace_back(sf::Vector2f{CELL_SIZE, CELL_SIZE});
                boardSquares_.back().setPosition(position);
                boardSquares_.back().setFillColor(color::AVAILABLE_MOVE);
            }
        }

        pieces_.resize(whitePieces.size() + blackPieces.size());

        auto setPiece = [&](auto& piece, auto cellId, auto color, auto outlineColor) {
            piece = sf::CircleShape(PIECE_RADIUS);
            piece.setPosition(ToVector(cellId, numCols_));
            piece.setFillColor(color);
            piece.setOutlineColor(outlineColor);
            piece.setOutlineThickness(2.f);
        };

        for (const auto&[id, piece] : Enumerate(blackPieces)) {
            setPiece(pieces_.at(id), piece.cellId, color::DIM_GREY, color::GREY);
        }

        for (const auto&[id, piece] : Enumerate(whitePieces)) {
            setPiece(pieces_.at(id + blackPieces.size()), piece.cellId, color::WHITE_SMOKE,
                     color::LIGHT_DIM_GREY);
        }
    }

    void Render() override {
        window_.draw(sprite_);
        for (auto cellId : squaresToDraw_) {
            window_.draw(boardSquares_[cellId]);
        }
        for (const auto& piece : pieces_) {
            window_.draw(piece);
        }
        window_.display();
    }

    void SetWhitesQueen(int pieceId) override {
        pieces_.at(pieceId).setFillColor(color::SOFT_YELLOW);
    }

    void SetBlacksQueen(int pieceId) override {
        pieces_.at(pieceId).setFillColor(color::RAINBOW_INDIGO);
    }

    void HighlightPieces(const std::unordered_set<int>& availablePieces) override {
        for (auto pieceId : availablePieces) {
            pieces_.at(pieceId).setOutlineColor(sf::Color::Green);
        }
    }

    void SetPiecePosition(int pieceId, int cellId) override {
        pieces_.at(pieceId).setPosition(ToVector(cellId, numCols_));
    }

    void ErasePiece(int pieceId) override {
        pieces_.at(pieceId).setPosition(UNDEFINED_POSITION);
    }

private:
    sf::RenderWindow& window_;

    std::vector<sf::RectangleShape> boardSquares_;

    sf::Sprite sprite_;
    sf::Texture texture_;
    std::unordered_set<int> squaresToDraw_;

    std::vector<sf::CircleShape> pieces_;

    int numCols_ = -1;
};

class OutOfMovesError : public std::runtime_error {
public:
    OutOfMovesError() : std::runtime_error("Lost!") {}
};

class DrawError : public std::runtime_error {
public:
    DrawError() : std::runtime_error("Draw!") {}
};

class GameManager {
public:
    GameManager(size_t numRows, size_t numCols, Renderer& renderer)
        : size_(numRows * numCols), numRows_(numRows), numCols_(numCols), numBlackPieces_(12),
          paths_(size_), board_(size_, -1), renderer_(renderer) {
        blackPieces_.reserve(12);
        whitePieces_.reserve(12);
        allPieces_.reserve(24);
    }

    void InitBoard(
        const std::string& boardFilename = "",
        std::vector<Piece> whitePieces = {},
        std::vector<Piece> blackPieces = {},
        int skipRowsFrom = 3,
        int skipRowsTo = 5) {
        bool creatingDefaultBoard = false;
        if (whitePieces.empty() && blackPieces.empty()) {
            numBlackPieces_ = 12;
            creatingDefaultBoard = true;
        } else {
            numBlackPieces_ = blackPieces.size();
        }
        for (int i = 0; i < numRows_; ++i) {
            for (int j = 0; j < numCols_; ++j) {
                int cellId = i * numCols_ + j;
                paths_.at(cellId) = std::make_unique<PathNode>(cellId);
                if ((i + j) & 1) {
                    if (creatingDefaultBoard) {
                        if (i < skipRowsFrom) {
                            blackPieces.push_back({cellId, false});
                        } else if (i >= skipRowsTo) {
                            whitePieces.push_back({cellId, false});
                        }
                    }
                } else {
                    board_.at(cellId) = -2;
                }
            }
        }
        for (const auto&[id, piece] : Enumerate(blackPieces)) {
            allPieces_.push_back(piece);
            int pieceId = id;
            blackPieces_.insert(pieceId);
            board_.at(piece.cellId) = pieceId;
        }
        for (const auto&[id, piece] : Enumerate(whitePieces)) {
            allPieces_.push_back(piece);
            int pieceId = id + numBlackPieces_;
            whitePieces_.insert(pieceId);
            board_.at(piece.cellId) = pieceId;
        }
        renderer_.InitBoard(boardFilename, whitePieces, blackPieces, numRows_, numCols_);
    }

    void ProcessClick(int cellId) {
        if (transitions_.contains(cellId)) {
            auto handler = transitions_.at(cellId);
            handler(cellId);
        }
    }

    void Start() {
        Turn();
    }

    bool IsWhitesTurn() const {
        return whitesTurn_;
    }

    bool IsLastLine(int cellId) const {
        return (whitesTurn_ && cellId / numCols_ == 0) ||
            (!whitesTurn_ && cellId / numCols_ == numRows_ - 1);
    }

    class State {
    public:
        explicit State(const GameManager& game) : game_(game) {
        }

        const auto& GetPaths() const {
            return game_.paths_;
        }

        const auto& GetBoard() const {
            return game_.board_;
        }

        bool IsWhite(int pieceId) const {
            return game_.IsWhite(pieceId);
        }

        bool IsEnemy(int cellId) const {
            return game_.IsEnemy(cellId);
        }

        bool IsQueen(int pieceId) const {
            return game_.allPieces_.at(pieceId).isQueen;
        }

        bool IsLastLine(int cellId) const {
            return game_.IsLastLine(cellId);
        }

        int GetNumCols() const {
            return game_.numCols_;
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
            throw OutOfMovesError();
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

    void CalcJumps(const std::unordered_set<int>& pieces) {
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
        renderer_.RemoveHighlightFromMoves(paths_.at(selectedPiece_.cellId));
        selectedPiece_.cellId = cellId;
        renderer_.ShowMoves(paths_.at(cellId));
        AddMovesEventTransitions(cellId);
    }

    void ClickHighlightedCell(int cellId) {
        transitions_.clear();
        renderer_.RemoveHighlightFromPieces(availablePieces_);
        renderer_.RemoveHighlightFromMoves(paths_.at(selectedPiece_.cellId));
        MakeMove(cellId);
        if (mustJumpFrom_ != -1) {
            renderer_.ShowMoves(paths_.at(mustJumpFrom_));
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
                renderer_.ShowMoves(paths_.at(cellId));
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

        if (eaten_.empty() && piece.isQueen) {
            --turnsUntilDraw_;
            if (turnsUntilDraw_ == 0) {
                throw DrawError();
            }
        } else {
            turnsUntilDraw_ = TURNS_UNTIL_DRAW;
        }

        if (IsLastLine(to)) {
            if (!piece.isQueen) {
                piece.isQueen = true;
                if (whitesTurn_) {
                    renderer_.SetWhitesQueen(pieceId);
                } else {
                    renderer_.SetBlacksQueen(pieceId);
                }

                if (!eaten_.empty()) {
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
        renderer_.SetPiecePosition(pieceId, to);
        if (IsWhite(pieceId)) {
            whitePieces_.insert(pieceId);
        } else {
            blackPieces_.insert(pieceId);
        }
        assert(pieceId >= 0);
        board_.at(to) = pieceId;

        allPieces_.at(pieceId).cellId = to;
    }

    int RemovePiece(int cellId) {
        auto pieceId = board_.at(cellId);
        assert(pieceId >= 0);
        board_.at(cellId) = -1;

        allPieces_.at(pieceId).cellId = -1;
        renderer_.ErasePiece(pieceId);
        if (IsWhite(pieceId)) {
            whitePieces_.erase(pieceId);
        } else {
            blackPieces_.erase(pieceId);
        }

        return pieceId;
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

    std::unordered_set<int>& GetPlayerPieces() {
        if (whitesTurn_) {
            return whitePieces_;
        }
        return blackPieces_;
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
        renderer_.HighlightPieces(availablePieces_);
        for (auto pieceId : availablePieces_) {
            transitions_[allPieces_.at(pieceId).cellId] = [this](int cellId) {
                ClickHighlightedPiece(cellId);
            };
        }
    }

    static inline constexpr auto FORWARD = {-9, -7};
    static inline constexpr auto BACKWARD = {7, 9};
    static inline constexpr auto BOTH_DIRS = {-9, -7, 7, 9};

    const int size_;
    const int numRows_;
    const int numCols_;
    int numBlackPieces_;
    std::vector<Piece> allPieces_;
    std::unordered_set<int> whitePieces_;
    std::unordered_set<int> blackPieces_;

    Renderer& renderer_;

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

    static constexpr int NUM_PLAYERS = 2;
    static constexpr int TURNS_UNTIL_DRAW = 15 * NUM_PLAYERS;
    int turnsUntilDraw_ = TURNS_UNTIL_DRAW;
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
                return ToCellId(event.mouseButton.x, event.mouseButton.y, state->GetNumCols());
            }
        }
        return -1;
    }

private:
    Events& events_;
};

class SimpleBot : public Player {
public:
    explicit SimpleBot() = default;

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

class Simulator : public Player {
public:
    explicit Simulator(std::vector<int> turns) : turns_(std::move(turns)) {
    }

    int Turn(std::unique_ptr<GameManager::State>) override {
        sf::sleep(sf::milliseconds(30));
        return turns_[ind_++];
    }

private:
    std::vector<int> turns_;
    size_t ind_ = 0;
};

class AiBot : public Player {
public:
    explicit AiBot(std::shared_ptr<Module> nn) : nn_(std::move(nn)) {
    }

    int Turn(std::unique_ptr<GameManager::State> state) override {
        if (turns_.empty()) {
            CalcTurns(state);
        }
        if (turns_.empty()) {
            throw OutOfMovesError();
        }
        auto turn = turns_.front();
        turns_.erase(turns_.begin());
        return turn;
    }

private:
    void CalcTurns(const std::unique_ptr<GameManager::State>& state) {
        static const std::vector<float> FREE = {1, 0, 0, 0, 0};
        static const std::vector<float> WHITE = {0, 1, 0, 0, 0};
        static const std::vector<float> WHITE_QUEEN = {0, 0, 1, 0, 0};
        static const std::vector<float> BLACK = {0, 0, 0, 1, 0};
        static const std::vector<float> BLACK_QUEEN = {0, 0, 0, 0, 1};

        const auto& board = state->GetBoard();
        std::vector<std::vector<float>> input;
        input.reserve(board.size() / 2);
        for (auto [i, pieceId] : Enumerate(board)) {
            if (pieceId != -2) {
                if (pieceId == -1) {
                    input.push_back(FREE);
                } else if (state->IsWhite(pieceId)) {
                    if (state->IsQueen(pieceId)) {
                        input.push_back(WHITE_QUEEN);
                    } else {
                        input.push_back(WHITE);
                    }
                } else {
                    if (state->IsQueen(pieceId)) {
                        input.push_back(BLACK_QUEEN);
                    } else {
                        input.push_back(BLACK);
                    }
                }
            }
        }

        std::vector<int> path;
        auto max = std::numeric_limits<float>::lowest();
        for (const auto& from : state->GetPaths()) {
            auto pieceId = board.at(from->cellId);
            if (pieceId >= 0 && !state->IsEnemy(from->cellId)) {
                LeavesTraverse(from, path, [&]() {
                    if (path.size() > 1) {
                        auto after = input;
                        after[path.front() / 2] = FREE;
                        for (size_t i = 1; i + 1 < path.size(); i += 2) {
                            after[path[i] / 2] = FREE;
                        }
                        bool isQueen = state->IsQueen(pieceId);
                        if (!isQueen) {
                            for (int cellId : path) {
                                if (state->IsLastLine(cellId)) {
                                    isQueen = true;
                                    break;
                                }
                            }
                        }
                        if (state->IsWhite(pieceId)) {
                            if (isQueen) {
                                after[path.back() / 2] = WHITE_QUEEN;
                            } else {
                                after[path.back() / 2] = WHITE;
                            }
                        } else {
                            if (isQueen) {
                                after[path.back() / 2] = BLACK_QUEEN;
                            } else {
                                after[path.back() / 2] = BLACK;
                            }
                        }

                        auto matrix = CreateMatrixFromData(after);
                        nn_->AdjustShape(matrix);
                        float prob = nn_->Forward(matrix)[0];
                        if (prob > max) {
                            max = prob;
                            turns_ = path;
                        }
                    }
                });
            }
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
            path.pop_back();
            return;
        }
        for (const auto& child : cur->children) {
            LeavesTraverse(child, path, cb);
        }
        path.pop_back();
    }

    std::vector<int> turns_;
    std::shared_ptr<Module> nn_;
};

class Controller {
public:
    Controller(GameManager& game, std::shared_ptr<Player> white, std::shared_ptr<Player> black)
        : game_(game), whitePlayer_(std::move(white)), blackPlayer_(std::move(black)) {
    }

    void NextMove() {
        int cellId;
        if (game_.IsWhitesTurn()) {
            cellId = whitePlayer_->Turn(game_.GetState());
            if (cellId != -1) Log() << "(whites," << cellId << ")";
        } else {
            cellId = blackPlayer_->Turn(game_.GetState());
            if (cellId != -1) Log() << "(blacks," << cellId << ")";
        }
        if (cellId == -1) {
            return;
        }
        game_.ProcessClick(cellId);
    }

private:
    GameManager& game_;
    std::shared_ptr<Player> whitePlayer_;
    std::shared_ptr<Player> blackPlayer_;
};

// template <class SecondPlayer>
// std::unique_ptr<Controller> PlayWith(GameManager& game, Events& events) {
//     return std::make_unique<Controller>(
//         game,
//         std::make_unique<Human>(events),
//         std::make_unique<SecondPlayer>(events));
// }

std::unique_ptr<Controller>
PlayWith(GameManager& game, Events& events, std::unique_ptr<Player> secondPlayer) {
    return std::make_unique<Controller>(
        game,
        std::make_unique<Human>(events),
        std::move(secondPlayer));
}

auto BuildNeuralNetwork() {
    auto nn = std::make_shared<Sequential>();
    (*nn)
        .AddModule(Flatten())
        .AddModule(Linear(INPUT_DIM * INPUT_ROWS, 32))
        .AddModule(ReLU())
        .AddModule(Linear(32, 16))
        .AddModule(ReLU())
        .AddModule(Linear(16, 1));
    static int i = 0;
    std::ofstream file("nn" + std::to_string(i));
    nn->Dump(file);
    ++i;
    return nn;
}

class School {
    struct Student {
        explicit Student(std::shared_ptr<Sequential> bot)
            : bot(std::move(bot))
            , mutex(std::make_unique<std::mutex>())
        {}

        Student(Student&&) = default;
        Student& operator=(Student&&) = default;

        bool operator<(const Student& rhs) const {
            return score < rhs.score;
        }

        int score = 0;
        std::unique_ptr<std::mutex> mutex;
        std::shared_ptr<Sequential> bot;
    };

public:
    explicit School(int numBots)
        : numBots_(numBots)
        , bestBlack_(BuildNeuralNetwork())
    {
        for (size_t i = 0; i < numBots_; ++i) {
            whiteBots_.emplace_back(BuildNeuralNetwork());
            whiteBots_.back().bot->ApplyNoise();
            blackBots_.emplace_back(BuildNeuralNetwork());
            blackBots_.back().bot->ApplyNoise();
        }
    }

    void Teach() {
        ThreadPool pool(12);
        std::atomic<int> gameInd = 0;
        for (size_t diff = 0; diff < numBots_; ++diff) {
            for (size_t firstInd = 0; firstInd < numBots_; ++firstInd) {
                pool.AddTask([=, this, &gameInd]() {
                    auto secondInd = (firstInd + diff) % numBots_;
                    auto& first = whiteBots_[firstInd];
                    auto& second = blackBots_[secondInd];

                    std::stringstream ss;
                    ss << "Game" << gameInd.fetch_add(1);
                    auto filename = ss.str();
                    thread_local std::ofstream file(filename);
                    Log() = Logger(std::move(filename), file);

                    EmptyRenderer renderer;
                    GameManager game(8, 8, renderer);
                    game.InitBoard("board_8x8.png");
                    game.Start();

                    std::unique_lock firstLock(*first.mutex, std::defer_lock);
                    std::unique_lock secondLock(*second.mutex, std::defer_lock);
                    std::lock(firstLock, secondLock);

                    auto win = Play(game, Controller(
                        game,
                        std::make_shared<AiBot>(first.bot),
                        std::make_shared<AiBot>(second.bot)));

                    if (win == 4) {
                        throw std::runtime_error("WFT");
                    }
                    if (win == 0) {
                        first.score += 2;
                    } else if (win == 1) {
                        second.score += 2;
                    } else {
                        assert(win == 2);
                        ++first.score;
                        ++second.score;
                    }

                    Log() << "won " << win;
                });
            }
        }
        pool.WaitAll();
    }

    void Update() {
        Update(whiteBots_);
        Log() << "Best whites score:" << whiteBots_.front().score;
        Update(blackBots_);
        Log() << "Best blacks score:" << blackBots_.front().score;
        if (blackBots_.front().score > bestBlack_.score) {
            bestBlack_.score = blackBots_.front().score;
            bestBlack_.bot = std::make_shared<Sequential>(*blackBots_.front().bot);
        }
        ZeroScore(whiteBots_);
        ZeroScore(blackBots_);
    }

    auto GetBest() const {
        Log() << "Best black score: " << bestBlack_.score;
        return bestBlack_.bot;
    }

private:
    void ZeroScore(std::vector<Student>& models) {
        for (auto& model : models) {
            model.score = 0;
        }
    }

    void Update(std::vector<Student>& models) {
        std::sort(models.begin(), models.end());

        const size_t numBest = 2;
        std::vector<Sequential> bestModels(numBest);
        for (size_t i = 0; i < numBest; ++i) {
            bestModels[i] = *models[i].bot;
        }

        // Noise population
        for (size_t i = bestModels.size(); i < models.size(); ++i) {
            // Choose one of two best models
            size_t index = GenerateNormalNumber() < 0.0;
            models[i].bot = std::make_shared<Sequential>(bestModels[index]);
            models[i].bot->ApplyNoise();
        }
    }

    int Play(GameManager& game, Controller controller) {
        try {
            while (true) {
                controller.NextMove();
            }
        } catch (const std::runtime_error& e) {
            if (e.what() != std::string("Lost!")) {
                throw;
            }
            if (game.IsWhitesTurn()) {
                return 1;
            }
            return 0;
        } catch (...) {
            return 3;
        }
        return 2;
    }

    const int numBots_;
    std::vector<Student> whiteBots_;
    std::vector<Student> blackBots_;
    Student bestBlack_;
};

class Game {
public:
    inline static const sf::ContextSettings SETTINGS = sf::ContextSettings(0, 0, 16);

    Game() :
        window_(sf::VideoMode(640, 640), "SFML works!", sf::Style::Default, SETTINGS),
        events_(window_),
        renderer_(window_),
        game_(8, 8, renderer_)
    {
        game_.InitBoard("board_8x8.png");
        game_.Start();
    }

    void PlayWithHuman() {
        auto controller = std::make_unique<Controller>(
            game_,
            std::make_unique<Human>(events_),
            std::make_unique<Human>(events_));
        Run(*controller);
    }

    void PlayWith(std::unique_ptr<Player> secondPlayer) {
        auto controller = std::make_unique<Controller>(
            game_,
            std::make_unique<Human>(events_),
            std::move(secondPlayer));
        Run(*controller);
    }

    void Simulate(std::string path) {
        std::ifstream file(path);
        std::vector<int> wt, bt;
//        std::vector<std::vector<int>> wt, bt;
        std::string line;
        bool whitesTurn = true;
        while (std::getline(file, line)) {
            auto ind = line.find('(');
            auto substr = line.substr(ind + 1, 6);
            auto turn = std::stoi(line.substr(ind + 8));
            if (substr == "whites") {
                wt.push_back(turn);
//                if (whitesTurn) {
//                wt.back().push_back(turn);
//                } else {
//                    whitesTurn = true;
//                    wt.emplace_back().push_back(turn);
//                }
            } else if (substr == "blacks") {
                bt.push_back(turn);

//                if (whitesTurn) {
//                    whitesTurn = false;
//                    bt.emplace_back().push_back(turn);
//                } else {
//                    bt.back().push_back(turn);
//                }
            }
        }
        auto controller = std::make_unique<Controller>(
            game_,
            std::make_unique<Simulator>(wt),
            std::make_unique<Simulator>(bt));
        Run(*controller);
    }

private:
    int Run(Controller& controller) {
        try {
            while (window_.isOpen()) {
                renderer_.Render();

                if (events_.Poll()) {
                    controller.NextMove();
                }
            }
        } catch (const OutOfMovesError& e) {
            Log() << e.what();
            if (game_.IsWhitesTurn()) {
                return 1;
            }
            return 0;
        } catch (const DrawError& e) {
            Log() << e.what();
            return 2;
        }
        return 3;
    }

    sf::RenderWindow window_;
    Events events_;
    BoardRenderer renderer_;
    GameManager game_;
};

void Simulate(std::string path) {
    Game().Simulate(std::move(path));
}

int main(int argc, char** argv) {
    std::string bot;
    if (argc > 1) {
        bot = argv[1];
    }

    int ret = -1;
    if (bot == "simple") {
        Game().PlayWith(std::make_unique<SimpleBot>());
    } else if (bot == "ai") {
        Game().PlayWith(std::make_unique<AiBot>(BuildNeuralNetwork()));
    } else if (bot == "learn") {
        const int numBots = 4;
        School school(numBots);
        const int numEpochs = 20;
        for (int i = 0; i < numEpochs; ++i) {
            school.Teach();
            school.Update();
        }
        auto black = school.GetBest();
        Game().PlayWith(std::make_unique<AiBot>(std::move(black)));
    } else if (bot == "simulate") {
        Simulate(argv[2]);
    } else {
        Game().PlayWithHuman();
    }
}
