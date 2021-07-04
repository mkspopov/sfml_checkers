#include <iostream>

class A {
public:
    A() {
        std::cout << "A constructed\n";
    }

    ~A() {
        std::cout << "A destructed\n";
    }

private:
    int a;
};

class GameObject {
public:
    virtual ~GameObject() = default;

private:
    A a, b, c, d, e, f, g;
};

class Character : public virtual GameObject {
private:
    A a;
};

class Vehicle : public virtual GameObject {

};

class Strange : public Character, public Vehicle {
private:
    A a, b, c;
};
