#include "score_system.h"

void addPoints(std::vector<Team>& teams, int teamIndex, int points)
{
    if (teamIndex >= 0 && teamIndex < teams.size())
    {
        teams[teamIndex].score += points;
    }
}
