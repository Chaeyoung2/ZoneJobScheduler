#pragma once

class Actor
{
public:
    Actor() {}
    Actor(int _max_hp, int _hp)
    : max_hp(_max_hp), hp(_hp) {}

    void take_damage(unsigned int damage)
    {
        if(damage > hp)
            hp = 0;
        else
            hp -= damage;
    }

    int get_hp()
    {
        return hp;
    }
    
private:
    int max_hp;
    int hp;
};


