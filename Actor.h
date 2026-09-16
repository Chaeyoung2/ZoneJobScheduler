#pragma once

class Actor
{
public:
    Actor() {}
    Actor(int initialMaxHp, int initialHp)
    : maxHp(initialMaxHp), hp(initialHp) {}

    void takeDamage(unsigned int damage)
    {
        if(damage > hp)
            hp = 0;
        else
            hp -= damage;
    }

    int getHp()
    {
        return hp;
    }
    
private:
    int maxHp = 100;
    int hp = 100;
};


