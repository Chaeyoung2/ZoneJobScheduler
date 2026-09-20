#pragma once

class Actor
{
public:
    Actor();
    Actor(int initialMaxHp, int initialHp);

    void takeDamage(int damage);
    
private:
    const int m_maxHp = 100;
    int m_hp = 100;
};


