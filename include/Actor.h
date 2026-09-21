#pragma once

class Actor
{
public:
    Actor();

    void takeDamage(int damage);
    int getHp() const;

private:
    int m_hp = 100;
};


