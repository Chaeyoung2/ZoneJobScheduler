#pragma once

class Actor
{
public:
    Actor();

    [[nodiscard]] bool takeDamage(int damage);
    int getHp() const;

private:
    int m_hp = 100;
};


