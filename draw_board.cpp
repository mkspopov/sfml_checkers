#include "utils.h"

#include <SFML/Graphics.hpp>

#include <cassert>
#include <iostream>

void DrawBoard(sf::RenderWindow& window, int rows, int cols, const char* filename) {
    int size = rows * cols;
    std::vector<sf::RectangleShape> boardSquares;
    boardSquares.reserve(size);
    std::vector<std::reference_wrapper<sf::Drawable>> drawable;
    drawable.reserve(size);

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            sf::Vector2f position = {j * CELL_SIZE, i * CELL_SIZE};
            boardSquares.emplace_back(sf::Vector2f{CELL_SIZE, CELL_SIZE});
            boardSquares.back().setPosition(position);
            drawable.emplace_back(boardSquares.back());
            if ((i + j) & 1) {
                boardSquares.back().setFillColor(color::LIGHT_GREY);
            } else {
                boardSquares.back().setFillColor(color::PEACH_PUFF);
            }
        }
    }

    window.clear();
    for (auto& obj : drawable) {
        window.draw(obj);
    }
    sf::Texture texture;
    texture.create(window.getSize().x, window.getSize().y);
    texture.update(window);
    if (texture.copyToImage().saveToFile(filename)) {
        std::cout << "screenshot saved to " << filename << std::endl;
    }
}

int main(int argc, char** argv) {
    assert(argc == 4);
    sf::ContextSettings settings;
    settings.antialiasingLevel = 16;
    sf::RenderWindow window(sf::VideoMode(640, 640), "SFML works!", sf::Style::Default, settings);
    DrawBoard(window, std::stoi(argv[1]), std::stoi(argv[2]), argv[3]);
}
