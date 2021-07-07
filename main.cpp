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

    void ProcessEvent(sf::Event& event) {
        if (event.type == sf::Event::MouseButtonPressed) {
            auto cellId = ToCellId(event.mouseButton.x, event.mouseButton.y);
            ProcessClick(cellId);
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
        if (pieceId < numBlackPieces_) {
            blackPieces_.push_back(pieceId);
        } else {
            whitePieces_.push_back(pieceId);
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
        if (pieceId < numBlackPieces_) {
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
        return whitesTurn_ ^ (board_.at(cellId) >= numBlackPieces_);
    }

    void ProcessClick(int cellId) {
        if (transitions_.contains(cellId)) {
            auto handler = transitions_.at(cellId);
            handler(cellId);
        }
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

int main() {
    sf::ContextSettings settings;
    settings.antialiasingLevel = 16;
    sf::RenderWindow window(sf::VideoMode(640, 640), "SFML works!", sf::Style::Default, settings);

    GameManager game(8, 8);
    game.InitBoard();
    game.Start();

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else {
                game.ProcessEvent(event);
            }
        }

        window.clear();
        game.Render(window);
        window.display();
        sf::sleep(sf::milliseconds(30));
    }
}
