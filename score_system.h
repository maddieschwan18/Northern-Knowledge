#pragma once
#include <vector>
#include <string>

struct Team {
    std::string name;
    int score;
};

// Called when host approves
void addPoints(std::vector<Team>& teams, int teamIndex, int points);
