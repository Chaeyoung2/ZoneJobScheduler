#pragma once

class Actor
{
public:
    Actor();
    Actor(int initialMaxHp, int initialHp);

    void takeDamage(unsigned int damage);
    int getHp() const;
    
private:
    const int m_maxHp = 100;
    int m_hp = 100;
};


