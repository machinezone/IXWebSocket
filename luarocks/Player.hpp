#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <iostream>

class Player
{
public:
    Player(const char* name, unsigned int health) :
        m_Name(name),
        m_Health(health)
    {
    }

    void info()
    {
        std::cout << m_Name << " have " << m_Health << " HP" << std::endl;
    }

    void say(const char* text)
    {
        std::cout << m_Name << ": " << text << std::endl;
    }

    void heal(Player* target)
    {
        target->setHealth(100);
    }

    const char* getName()
    {
        return m_Name;
    }

    unsigned int getHealth()
    {
        return m_Health;
    }

    bool setHealth(unsigned int health)
    {
        if (health >= 0 && health <= 100)
        {
            m_Health = health;
            return true;
        }
        else
            return false;
    }

private:
    const char* m_Name;
    unsigned int m_Health;
};

#endif // PLAYER_HPP