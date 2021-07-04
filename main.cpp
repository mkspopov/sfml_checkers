#include "game_object.h"

#include <SFML/Graphics.hpp>

#include <iostream>

void DrawCircle() {
    sf::RenderWindow window(sf::VideoMode(200, 200), "SFML works!");
    sf::CircleShape shape(100.f);
    shape.setFillColor(sf::Color::Green);

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        window.clear();
        window.draw(shape);
        window.display();
        sf::sleep(sf::seconds(0.1));
    }
}

static const auto LIGHT_GREY = sf::Color(0xD3D3D3FF);
static const auto PEACH_PUFF = sf::Color(0xFFDAB9FF);
static const auto WHITE_SMOKE = sf::Color(0xF5F5F5FF);
static const auto LIGHT_DIM_GREY = sf::Color(0xC0C0C0FF);
static const auto DIM_GREY = sf::Color(0x696969FF);
static const auto GREY = sf::Color(0x808080FF);

constexpr float PIECE_RADIUS = 30;

int ToCellId(int x, int y) {
    return (y / 80) * 8 + x / 80;
}

struct Piece {
    sf::CircleShape shape;
    bool isWhite = true;
};

class GameManager {
public:
    GameManager()
        : board_(64, -1)
    {
        boardSquares_.reserve(64);
        blackPieces_.reserve(12);
        whitePieces_.reserve(12);
    }

    void InitBoard() {
        auto createPiece = [&](auto& arr, auto pos, auto color, auto outlineColor) {
            auto& piece = arr.emplace_back();
            piece.shape = sf::CircleShape(PIECE_RADIUS);
            piece.shape.setPosition(pos);
            drawable_.emplace_back(piece.shape);
            piece.shape.setFillColor(color);
            piece.shape.setOutlineColor(outlineColor);
            piece.shape.setOutlineThickness(2.f);
            board_[ToCellId(pos.x, pos.y)] = drawable_.size() - 1;
        };

        size_t skipRowsFrom = 3;
        size_t skipRowsTo = 5;
        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                sf::Vector2f position = {j * 80.f, i * 80.f};
                boardSquares_.emplace_back(sf::Vector2f{80.0f, 80.0f});
                boardSquares_.back().setPosition(position);
                position += sf::Vector2f{10, 10};
                drawable_.emplace_back(boardSquares_.back());
                if ((i + j) & 1) {
                    boardSquares_.back().setFillColor(LIGHT_GREY);
                    if (i < skipRowsFrom) {
                        createPiece(blackPieces_, position, DIM_GREY, GREY);
                        blackPieces_.back().isWhite = false;
                    } else if (i >= skipRowsTo) {
                        createPiece(whitePieces_, position, WHITE_SMOKE, LIGHT_DIM_GREY);
                    }
                } else {
                    boardSquares_.back().setFillColor(PEACH_PUFF);
                }
            }
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

private:
    void CalculateMoves() {
        auto piecesPtr = &whitePieces_;
        if (!whitesTurn_) {
            piecesPtr = &blackPieces_;
        }
        auto& pieces = *piecesPtr;

        for (auto& piece : pieces) {
            if (MustJumpFrom(piece)) {

            }
        }
    }

    void CalcJumps() {

    }

    void ClickPossiblePiece() {
        RemoveHighlightFromMoves();
        ClickHighlightedPiece();
    }

    void ClickHighlightedCell() {
        RemoveHighlightFromMoves();
        MakeMove();
        if (NeedJump()) {
            ClickPossiblePiece();
        } else {
            ChangePlayer();
            Turn();
        }
    }

    void ClickHighlightedPiece() {
        RemoveHighlightFromPieces();
        ShowMoves();
        AddMovesEventTransitions();
    }


    void ProcessClick(int cellId) {
        if (transitions_.contains(cellId)) {
            auto handler = transitions_[cellId];
            transitions_.clear();
        }
//        if (board_.at(cellId) != -1) {
//            auto circ = dynamic_cast<sf::CircleShape*>(&drawable_[board_[cellId]].get());
//            if (circ) {
//                circ->setOutlineColor(sf::Color::Green);
//            }
//        }
    }

    void Turn() {
        CalculateMoves();
        HighlightPieces();
        AddEventTransitions();
    }

    std::vector<int> board_;

    std::vector<sf::RectangleShape> boardSquares_;
    std::vector<Piece> whitePieces_;
    std::vector<Piece> blackPieces_;

    std::vector<std::reference_wrapper<sf::Drawable>> drawable_;

    bool whitesTurn_ = true;
};

int main() {
    sf::ContextSettings settings;
    settings.antialiasingLevel = 16;
    sf::RenderWindow window(sf::VideoMode(640, 640), "SFML works!", sf::Style::Default, settings);

    GameManager game;
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
