#include "utils.h"

#include <mynn/mynn.h>

#include <SFML/Graphics.hpp>

#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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

//    void Render(auto& window) {
//    }

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

    void CalcJumps(std::unique_ptr<PathNode>& node, std::unordered_set<int>& eaten, int maxSteps,
                   int forbiddenDir) {
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
        if ((whitesTurn_ && to / numCols_ == 0) ||
            (!whitesTurn_ && to / numCols_ == numRows_ - 1)) {
            if (!piece.isQueen) {
                piece.isQueen = true;
                if (whitesTurn_) {
                    renderer_.SetWhitesQueen(pieceId);
                } else {
                    renderer_.SetBlacksQueen(pieceId);
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
    explicit AiBot(std::shared_ptr<Module> nn) : nn_(std::move(nn)) {
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
        auto max = std::numeric_limits<float>::lowest();
        for (const auto& from : state->GetPaths()) {
            auto pieceId = board.at(from->cellId);
            if (pieceId >= 0 && !state->IsEnemy(from->cellId)) {
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
            Log() << "(whites," << cellId << ")";
        } else {
            cellId = blackPlayer_->Turn(game_.GetState());
            Log() << "(blacks," << cellId << ")";
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

template <class SecondPlayer>
std::unique_ptr<Controller> PlayWith(GameManager& game, Events& events) {
    return std::make_unique<Controller>(
        game,
        std::make_unique<Human>(events),
        std::make_unique<SecondPlayer>(events));
}

std::unique_ptr<Controller>
PlayWith(GameManager& game, Events& events, std::unique_ptr<Player> secondPlayer) {
    return std::make_unique<Controller>(
        game,
        std::make_unique<Human>(events),
        std::move(secondPlayer));
}

class School {
public:
    explicit School(int numBots)
        : numBots_(numBots)
        , bots_(numBots_)
        , score_(numBots_)
        , mutexes_(numBots_)
    {
        for (auto& bot : bots_) {
            bot = std::make_shared<Sequential>();
            (*bot)
                .AddModule(Linear(64, 32))
                .AddModule(ReLU())
                .AddModule(Linear(32, 16))
                .AddModule(ReLU())
                .AddModule(Linear(16, 1));
        }
    }

    void Teach() {
        ThreadPool pool(12);
        std::atomic<int> gameInd = 0;
        for (size_t diff = 1; diff < numBots_; ++diff) {
            for (size_t first = 0; first + diff < numBots_; ++first) {
                pool.AddTask([=, this, &gameInd]() {
//                    sf::RenderWindow window(sf::VideoMode(640, 640), std::to_string(first) + " " + std::to_string(first + diff));
//                    BoardRenderer renderer(window);

                    Log() = Logger(gameInd.fetch_add(1));

                    EmptyRenderer renderer;
                    GameManager game(8, 8, renderer);
                    game.InitBoard("board_8x8.png");
                    game.Start();

                    auto second = first + diff;

                    std::unique_lock firstLock(mutexes_[first], std::defer_lock);
                    std::unique_lock secondLock(mutexes_[second], std::defer_lock);
                    std::lock(firstLock, secondLock);

                    auto win = Play(game, Controller(
                        game,
                        std::make_shared<AiBot>(bots_[first]),
                        std::make_shared<AiBot>(bots_[second])));

                    if (win == 3) {
                        throw std::runtime_error("WFT");
                    }
                    if (win == 0) {
                        ++score_[first];
                    } else {
                        assert(win == 1);
                        ++score_[second];
                    }
                    Log() << "won " << win;
                });
            }
        }
    }

    auto GetBest() const {
        std::vector<int> indices(numBots_);
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&](auto lhs, auto rhs) {
            return score_[lhs] < score_[rhs];
        });
        Log() << "Score of " << indices.back() << " is " << score_[indices.back()];
        return bots_[indices.back()];
    }

private:
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
    std::vector<int> score_;
    std::vector<std::mutex> mutexes_;
    std::vector<std::shared_ptr<Sequential>> bots_;
};

int main(int argc, char** argv) {
    std::string bot;
    if (argc > 1) {
        bot = argv[1];
    }

    sf::ContextSettings settings;
    settings.antialiasingLevel = 16;
    sf::RenderWindow window(sf::VideoMode(640, 640), "SFML works!", sf::Style::Default, settings);

    BoardRenderer renderer(window);
    GameManager game(8, 8, renderer);
    game.InitBoard("board_8x8.png");
    game.Start();
    Events events(window);

    std::unique_ptr<Controller> controller;

    auto Run = [&]() {
        try {
            while (window.isOpen()) {
                renderer.Render();

                if (events.Poll()) {
                    controller->NextMove();
                }
            }
        } catch (const std::runtime_error&) {
            if (game.IsWhitesTurn()) {
                return 1;
            }
            return 0;
        }
        return 2;
    };

    if (bot == "simple") {
        controller = PlayWith<SimpleBot>(game, events);
    } else if (bot == "ai") {
        auto nn = std::make_unique<Sequential>();
        (*nn)
            .AddModule(Linear(64, 32))
            .AddModule(ReLU())
            .AddModule(Linear(32, 16))
            .AddModule(ReLU())
            .AddModule(Linear(16, 1));
        controller = PlayWith(game, events, std::make_unique<AiBot>(std::move(nn)));
    } else if (bot == "learn") {
        const int numBots = 4;
        School school(numBots);
        school.Teach();
        auto nn = school.GetBest();
        controller = PlayWith(game, events, std::make_unique<AiBot>(std::move(nn)));
    } else {
        controller = PlayWith<Human>(game, events);
    }

    Run();
}

#pragma clang diagnostic pop