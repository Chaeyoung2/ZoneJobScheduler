#pragma once

class Actor
{
public:
    Actor();
    Actor(int initialMaxHp, int initialHp);

    void takeDamage(unsigned int damage);
    int getHp();
    
private:
    int maxHp = 100;
    int hp = 100;
};


