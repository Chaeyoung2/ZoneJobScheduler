#pragma once

class Actor
{
public:
    Actor();

    [[nodiscard]] bool takeDamage(const int damage);
    int getHp() const;

private:
    int m_hp = 100;
};


