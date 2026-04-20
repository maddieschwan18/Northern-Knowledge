#include <SDL2/SDL.h>          // SDL2 main library
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "score_system.h"     // Score function
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace std;

// ------------------ Structures ------------------
// Individual question format loaded from CSV file
struct Question {
    string type;
    string q;
    string a;
    vector<string> options;
    string image;
    string answerImage;
};

// ------------------ Game State ------------------
enum GameMode { EASY, MEDIUM, HARD, FREE_PLAY };
// Game Screens 
enum GameState { WELCOME_SCREEN, INSTRUCTIONS_SCREEN, MODE_SELECT_SCREEN, GAME_RUNNING, SHOW_ANSWER_SCREEN, LEADERBOARD_SCREEN, FINAL_SCREEN};

GameState gameState = WELCOME_SCREEN; // Begins on Welcome Screen
GameMode currentMode = EASY;
int buzzedTeam = -1;    // Tracks which team buzzed (-1  = none)
// ------------------ Globals ------------------
vector<Team> teams = {
    {"Team 1", 0},
    {"Team 2", 0},
    {"Team 3", 0},
    {"Team 4", 0}
};

// Team colors for UI
vector<SDL_Color> teamColors = {
    {0, 120, 255, 255},   // Blue
    {255, 105, 180, 255}, // Pink
    {255, 215, 0, 255},   // Yellow
    {0, 200, 0, 255}      // Green
};

// State flags and screen transitions
bool stateChanged = false;
Uint32 questionStartTime = 0;
int timeLimit = 300000;
bool timerRunning = true;
int lastCorrectTeam = -1;
bool canAdvanceLeaderboard = false;
bool canAdvanceAnswerScreen = false;

SDL_Texture* cachedQuestionTex = nullptr;
int cachedQuestionIndex = -1;

// ------------------ Load Questions ------------------
vector<Question> loadQuestions(const string& filename) {
    vector<Question> questions;
    ifstream fin(filename);
    string line, word;

    while (getline(fin, line)) {
        if (line.empty()) continue;

        stringstream s(line);
        vector<string> row;

        while (getline(s, word, ',')) {
            row.push_back(word);
        }

        // Skip header row
        if (!row.empty() && row[0] == "type") continue;

        // Ensure at least 9 columns
        while (row.size() < 9) {
            row.push_back("");
        }

        Question q;
        q.type = row[0];
        q.q = row[1];
        q.a = row[2];

        // Only MC questions should load options
        if (q.type == "MC") {
            for (int i = 3; i <= 6; i++) {
                if (!row[i].empty()) {
                    q.options.push_back(row[i]);
                }
            }
        }

        q.image = row[7];
        q.answerImage = row[8];

        questions.push_back(q);
    }

    return questions;
}

// ------------------ Helpers ------------------
SDL_Texture* renderText(string msg, TTF_Font* font, SDL_Color color, SDL_Renderer* renderer) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, msg.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

SDL_Texture* renderTextWrapped(string msg, TTF_Font* font, SDL_Color color, SDL_Renderer* renderer, int maxWidth) {
    SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, msg.c_str(), color, maxWidth);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void drawTeamRibbon(SDL_Renderer* renderer, TTF_Font* font, SDL_Color orange, int winW) {
    int spacing = winW * 0.02;
    int x = spacing;

    for (int i = 0; i < teams.size(); i++) {
        string text = teams[i].name + ": " + to_string(teams[i].score);

        SDL_Texture* t = renderText(text, font, teamColors[i], renderer);

        int w, h;
        SDL_QueryTexture(t, nullptr, nullptr, &w, &h);

        // 🎯 If this team buzzed → draw orange rectangle
        if (i == buzzedTeam) {
            Uint32 ticks = SDL_GetTicks();

            // Pulse between about 8 and 14 pixels of padding
            int pulse = 8 + (int)(3 * sin(ticks * 0.01));

            SDL_SetRenderDrawColor(renderer, 255, 140, 0, 255);

            SDL_Rect highlightRect = {
                x - pulse,
                10 - pulse / 2,
                w + pulse * 2,
                h + pulse
            };

            for (int offset = 0; offset < 3; offset++) {
                SDL_Rect r = {
                    highlightRect.x - offset,
                    highlightRect.y - offset,
                    highlightRect.w + offset * 2,
                    highlightRect.h + offset * 2
                };
                SDL_RenderDrawRect(renderer, &r);
            }
        }

        // Draw the team text on top
        SDL_Rect dst = { x, 10, w, h };
        SDL_RenderCopy(renderer, t, nullptr, &dst);

        SDL_DestroyTexture(t);

        x += w + spacing;
    }
}

void setTimeLimit(GameMode mode) {
    switch (mode) {
        case EASY: timeLimit = 300000; break;
        case MEDIUM: timeLimit = 180000; break;
        case HARD: timeLimit = 60000; break;
        case FREE_PLAY: timeLimit = -1; break;
    }
}

TTF_Font* getFittingFont(string text, int maxWidth, int maxHeight) {
    int size = 48;

    while (size > 10) {
        TTF_Font* testFont = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", size);
        int w, h;
        TTF_SizeText(testFont, text.c_str(), &w, &h);
        if (w <= maxWidth && h <= maxHeight) return testFont;
        TTF_CloseFont(testFont);
        size--;
    }
    return TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 10);
}

SDL_Rect getCenteredImageRect(SDL_Texture* tex, int maxW, int maxH, int centerX, int centerY) {
    int imgW, imgH;
    SDL_QueryTexture(tex, nullptr, nullptr, &imgW, &imgH);

    double scaleX = (double)maxW / imgW;
    double scaleY = (double)maxH / imgH;
    double scale = min(scaleX, scaleY);

    int drawW = (int)(imgW * scale);
    int drawH = (int)(imgH * scale);

    SDL_Rect rect = {
        centerX - drawW / 2,
        centerY - drawH / 2,
        drawW,
        drawH
    };

    return rect;
}

// ------------------ Main ------------------
int main() {
    vector<Question> questions = loadQuestions("questions.csv");
    if (questions.empty()) {
        cout << "No questions found\n";
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    SDL_Window* window = SDL_CreateWindow("Quiz Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1400, 800, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 28);
    SDL_Color orange = { 255, 140, 0 };

    bool running = true;
    SDL_Event event;
    int currentQuestion = 0;

    if (gameState == LEADERBOARD_SCREEN)
        canAdvanceLeaderboard = true;
    else
        canAdvanceLeaderboard = false;

    if (gameState == SHOW_ANSWER_SCREEN)
        canAdvanceAnswerScreen = true;
    else
        canAdvanceAnswerScreen = false;

    while (running) {
        stateChanged = false;  // reset each frame
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        if (gameState == LEADERBOARD_SCREEN)
            canAdvanceLeaderboard = true;
        else
            canAdvanceLeaderboard = false;

        if (gameState == SHOW_ANSWER_SCREEN) {
            canAdvanceAnswerScreen = true;
        } else {
            canAdvanceAnswerScreen = false;
        }

        // -------- Event Handling --------
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;

            if ((gameState == WELCOME_SCREEN || gameState == INSTRUCTIONS_SCREEN) &&
                event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_RETURN &&
                event.key.repeat == 0 &&
                !stateChanged)
            {
                if (gameState == WELCOME_SCREEN) {
                    gameState = INSTRUCTIONS_SCREEN;
                }
                else if (gameState == INSTRUCTIONS_SCREEN) {
                    gameState = MODE_SELECT_SCREEN;
                }

                stateChanged = true;
                continue;
            }

            if (gameState == GAME_RUNNING &&
                event.type == SDL_KEYDOWN &&
                event.key.repeat == 0)
            {
                cout << "GAME key pressed: " << event.key.keysym.sym << endl;
                cout << "buzzedTeam before key handling: " << buzzedTeam << endl;

                if (buzzedTeam == -1) {
                    if (event.key.keysym.sym == SDLK_1) {
                        buzzedTeam = 0;
                        cout << "Team 1 buzzed\n";
                    }
                    else if (event.key.keysym.sym == SDLK_2) {
                        buzzedTeam = 1;
                        cout << "Team 2 buzzed\n";
                    }
                    else if (event.key.keysym.sym == SDLK_3) {
                        buzzedTeam = 2;
                        cout << "Team 3 buzzed\n";
                    }
                    else if (event.key.keysym.sym == SDLK_4) {
                        buzzedTeam = 3;
                        cout << "Team 4 buzzed\n";
                    }
                }
                else if ((event.key.keysym.sym == SDLK_BACKSPACE ||
                        event.key.keysym.sym == SDLK_DELETE) &&
                        buzzedTeam != -1)
                {
                    cout << "Incorrect answer triggered\n";
                    lastCorrectTeam = -1;
                    gameState = SHOW_ANSWER_SCREEN;
                    canAdvanceLeaderboard = false;
                    continue;
                }
                else if (event.key.keysym.sym == SDLK_RETURN && buzzedTeam != -1)
                {
                    cout << "Correct answer approved\n";
                    addPoints(teams, buzzedTeam, 10);
                    lastCorrectTeam = buzzedTeam;
                    buzzedTeam = -1;
                    gameState = LEADERBOARD_SCREEN;
                    canAdvanceLeaderboard = false;
                    continue;
                }

                cout << "buzzedTeam after key handling: " << buzzedTeam << endl;
            }

            if (gameState == MODE_SELECT_SCREEN &&
                event.type == SDL_KEYDOWN &&
                event.key.repeat == 0)
            {
                bool validSelection = true;

                if (event.key.keysym.sym == SDLK_1) currentMode = EASY;
                else if (event.key.keysym.sym == SDLK_2) currentMode = MEDIUM;
                else if (event.key.keysym.sym == SDLK_3) currentMode = HARD;
                else if (event.key.keysym.sym == SDLK_4) currentMode = FREE_PLAY;
                else validSelection = false;

                if (validSelection) {
                    setTimeLimit(currentMode);
                    questionStartTime = SDL_GetTicks();
                    timerRunning = true;
                    buzzedTeam = -1;
                    gameState = GAME_RUNNING;
                }
                continue;
            }

            if (gameState == GAME_RUNNING &&
                buzzedTeam == -1 &&
                event.type == SDL_MOUSEBUTTONDOWN)
            {
                gameState = LEADERBOARD_SCREEN;
                canAdvanceLeaderboard = false;
                continue;
            }

            if (gameState == GAME_RUNNING &&
                currentMode == FREE_PLAY &&
                event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_s) {
                    timerRunning = true;
                    questionStartTime = SDL_GetTicks();
                }
                if (event.key.keysym.sym == SDLK_p) {
                    timerRunning = false;
                }
            }

            if (gameState == LEADERBOARD_SCREEN && canAdvanceLeaderboard) {
                if ((event.type == SDL_KEYDOWN && event.key.repeat == 0) ||
                    event.type == SDL_MOUSEBUTTONDOWN)
                {
                    currentQuestion++;

                    if (currentQuestion >= questions.size()) {
                        gameState = FINAL_SCREEN;
                    } 
                    else {
                        questionStartTime = SDL_GetTicks();
                        timerRunning = true;
                        cachedQuestionIndex = -1;
                        lastCorrectTeam = -1;
                        buzzedTeam = -1;
                        gameState = GAME_RUNNING;
                    }
                    continue;
                }
            }

            if (gameState == SHOW_ANSWER_SCREEN &&
                ((event.type == SDL_KEYDOWN && event.key.repeat == 0) ||
                event.type == SDL_MOUSEBUTTONDOWN))
            {
                gameState = LEADERBOARD_SCREEN;
                canAdvanceLeaderboard = false;
                continue;
            }

            if (gameState == FINAL_SCREEN &&
                event.type == SDL_KEYDOWN &&
                event.key.repeat == 0)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }

                if (event.key.keysym.sym == SDLK_SPACE) {
                    for (auto& team : teams) {
                        team.score = 0;
                    }

                    currentQuestion = 0;
                    lastCorrectTeam = -1;
                    buzzedTeam = -1;
                    cachedQuestionIndex = -1;
                    gameState = WELCOME_SCREEN;
                }
            }
        }

        int margin = winW * 0.03;
        int topBarHeight = winH * 0.1;

        if (gameState == GAME_RUNNING || gameState == SHOW_ANSWER_SCREEN)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // white
        else
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // black
        SDL_RenderClear(renderer);      

        // -------- Welcome Screen --------
        if (gameState == WELCOME_SCREEN) {
            SDL_Texture* title = renderText("Northern Knowledge Trivia", font, orange, renderer);
            int tw, th;
            SDL_QueryTexture(title, nullptr, nullptr, &tw, &th);

            SDL_Rect titleRect = { (winW - tw)/2, (int)(winH * 0.3), tw, th };
            SDL_RenderCopy(renderer, title, nullptr, &titleRect);
            SDL_DestroyTexture(title);

            SDL_Texture* subtitle = renderText("Press Enter to Start", font, orange, renderer);
            SDL_QueryTexture(subtitle, nullptr, nullptr, &tw, &th);

            SDL_Rect subRect = { (winW - tw)/2, (int)(winH * 0.5), tw, th };
            SDL_RenderCopy(renderer, subtitle, nullptr, &subRect);
            SDL_DestroyTexture(subtitle);

            SDL_RenderPresent(renderer);
            continue;
        }

        // -------- Instructions Screen --------
        if (gameState == INSTRUCTIONS_SCREEN) {
            SDL_Texture* title = renderText("Game Instructions", font, orange, renderer);
            int tw, th;
            SDL_QueryTexture(title, nullptr, nullptr, &tw, &th);

            SDL_Rect titleRect = { (winW - tw)/2, (int)(winH * 0.1), tw, th };
            SDL_RenderCopy(renderer, title, nullptr, &titleRect);
            SDL_DestroyTexture(title);

            vector<string> instructions = {
                "1. Press your team button to buzz in.",
                "2. Only the first team to buzz can answer.",
                "3. Correct answers earn 10 points.",
                "4. No points awarded if the team is wrong.",
                "5. Highest score wins!"
            };

            for (int i = 0; i < instructions.size(); i++) {
                SDL_Texture* line = renderText(instructions[i], font, orange, renderer);
                SDL_QueryTexture(line, nullptr, nullptr, &tw, &th);

                SDL_Rect rect = { (winW - tw)/2, (int)(winH * 0.25) + i * (th + 20), tw, th };
                SDL_RenderCopy(renderer, line, nullptr, &rect);
                SDL_DestroyTexture(line);
            }

            SDL_Texture* cont = renderText("Press Enter to Continue", font, orange, renderer);
            SDL_QueryTexture(cont, nullptr, nullptr, &tw, &th);

            SDL_Rect contRect = { (winW - tw)/2, (int)(winH * 0.8), tw, th };
            SDL_RenderCopy(renderer, cont, nullptr, &contRect);
            SDL_DestroyTexture(cont);

            SDL_RenderPresent(renderer);
            continue;
        }

        // -------- Start Screen --------
        if (gameState == MODE_SELECT_SCREEN) {

            SDL_Texture* title = renderText("Select Game Mode", font, orange, renderer);
            int tw, th;
            SDL_QueryTexture(title, nullptr, nullptr, &tw, &th);

            SDL_Rect titleRect = { (winW - tw)/2, (int)(winH * 0.2), tw, th };
            SDL_RenderCopy(renderer, title, nullptr, &titleRect);
            SDL_DestroyTexture(title);

            vector<string> modes = {
                "1 - Easy (5 min)",
                "2 - Medium (3 min)",
                "3 - Hard (1 min)",
                "4 - Free Play"
            };

            for (int i = 0; i < modes.size(); i++) {
                SDL_Texture* txt = renderText(modes[i], font, orange, renderer);
                SDL_QueryTexture(txt, nullptr, nullptr, &tw, &th);

                SDL_Rect rect = { 
                    (winW - tw)/2, 
                    (int)(winH * 0.4) + i * (th + 20), 
                    tw, 
                    th 
                };

                SDL_RenderCopy(renderer, txt, nullptr, &rect);
                SDL_DestroyTexture(txt);
            }

            SDL_RenderPresent(renderer);
            continue;
        }

        // -------- Show Answer Screen --------
        if (gameState == SHOW_ANSWER_SCREEN) {

            Question& q = questions[currentQuestion];

            int tw, th;

            SDL_Color black = {0, 0, 0, 255};

            // Title
            SDL_Texture* title = renderText("Correct Answer", font, black, renderer);
            SDL_QueryTexture(title, nullptr, nullptr, &tw, &th);

            SDL_Rect titleRect = { (winW - tw)/2, (int)(winH * 0.2), tw, th };
            SDL_RenderCopy(renderer, title, nullptr, &titleRect);
            SDL_DestroyTexture(title);

            // Answer text
            SDL_Texture* answerTex = renderTextWrapped(q.a, font, black, renderer, (int)(winW * 0.6));
            SDL_QueryTexture(answerTex, nullptr, nullptr, &tw, &th);

            SDL_Rect answerRect = { (winW - tw)/2, (int)(winH * 0.35), tw, th };
            SDL_RenderCopy(renderer, answerTex, nullptr, &answerRect);
            SDL_DestroyTexture(answerTex);

            // Answer image, if present
            if (!q.answerImage.empty()) {
                SDL_Texture* ansImg = IMG_LoadTexture(renderer, q.answerImage.c_str());
                if (ansImg) {
                    SDL_Rect imgRect = getCenteredImageRect(
                        ansImg,
                        (int)(winW * 0.5),    // max width
                        (int)(winH * 0.3),    // max height
                        winW / 2,             // centered horizontally
                        (int)(winH * 0.63)    // below answer text
                    );

                    SDL_RenderCopy(renderer, ansImg, nullptr, &imgRect);
                    SDL_DestroyTexture(ansImg);
                } else {
                    cout << "Failed to load answer image: " << q.answerImage << endl;
                    cout << "IMG_LoadTexture Error: " << IMG_GetError() << endl;
                }
            }

            // Continue instruction
            SDL_Texture* cont = renderText("Press any key to continue", font, black, renderer);
            SDL_QueryTexture(cont, nullptr, nullptr, &tw, &th);

            SDL_Rect contRect = { (winW - tw)/2, (int)(winH * 0.8), tw, th };
            SDL_RenderCopy(renderer, cont, nullptr, &contRect);
            SDL_DestroyTexture(cont);

            SDL_RenderPresent(renderer);
            continue;
        }

        // -------- Leaderboard Screen --------
        if (gameState == LEADERBOARD_SCREEN) {
            cout << "Rendering leaderboard. lastCorrectTeam = " << lastCorrectTeam << endl;
            string titleText;
            SDL_Color titleColor = orange;  // default
            if (lastCorrectTeam != -1) {
                titleText = teams[lastCorrectTeam].name + " answered correctly!";
                titleColor = teamColors[lastCorrectTeam];
            }
            else
                titleText = "Leaderboard";

            SDL_Texture* titleTex = renderText(titleText, font, titleColor, renderer);
            int tw, th;
            SDL_QueryTexture(titleTex, nullptr, nullptr, &tw, &th);
            SDL_Rect titleRect = { (winW - tw) / 2, 50, tw, th };
            SDL_RenderCopy(renderer, titleTex, nullptr, &titleRect);
            SDL_DestroyTexture(titleTex);

            vector<Team> sortedTeams = teams;
            sort(sortedTeams.begin(), sortedTeams.end(), [](Team &a, Team &b) { return a.score > b.score; });
            SDL_Texture* cont = renderText("Click or press any key for next question", font, orange, renderer);

            int cw, ch;
            SDL_QueryTexture(cont, nullptr, nullptr, &cw, &ch);

            SDL_Rect contRect = { (winW - cw)/2, (int)(winH * 0.85), cw, ch };

            SDL_RenderCopy(renderer, cont, nullptr, &contRect);
            SDL_DestroyTexture(cont);
            for (int i = 0; i < sortedTeams.size(); i++) {
                SDL_Color color;
                if (i == 0)
                    color = orange;
                else
                    color = {255, 255, 255, 255};

                string text = to_string(i + 1) + ". " +
                            sortedTeams[i].name + ": " +
                            to_string(sortedTeams[i].score);

                SDL_Texture* t = renderText(text, font, color, renderer);

                int tw, th;
                SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);

                SDL_Rect rect = { (winW - tw) / 2, 150 + i * (th + 20), tw, th };

                SDL_RenderCopy(renderer, t, nullptr, &rect);
                SDL_DestroyTexture(t);
            }

            SDL_RenderPresent(renderer);
            continue;
        }

        if (gameState == FINAL_SCREEN) {

            // Title
            SDL_Texture* title = renderText("Final Results", font, orange, renderer);
            int tw, th;
            SDL_QueryTexture(title, nullptr, nullptr, &tw, &th);

            SDL_Rect titleRect = { (winW - tw)/2, (int)(winH * 0.1), tw, th };
            SDL_RenderCopy(renderer, title, nullptr, &titleRect);
            SDL_DestroyTexture(title);

            // Sort teams
            vector<Team> sortedTeams = teams;
            sort(sortedTeams.begin(), sortedTeams.end(),
                [](Team &a, Team &b) { return a.score > b.score; });

            // Winner / tie text
            int topScore = sortedTeams[0].score;
            vector<Team> tiedTeams;

            for (const auto& team : sortedTeams) {
                if (team.score == topScore) {
                    tiedTeams.push_back(team);
                } else {
                    break; // sorted descending, so stop when score drops
                }
            }

            string winnerText;
            SDL_Color winnerColor = orange;

            if (tiedTeams.size() == 1) {
                winnerText = tiedTeams[0].name + " wins!";

                // Use that team's matching color
                for (int i = 0; i < teams.size(); i++) {
                    if (teams[i].name == tiedTeams[0].name) {
                        winnerColor = teamColors[i];
                        break;
                    }
                }
            }
            else {
                winnerText = "It's a tie!";

                // keep tie text orange, or change to white if you prefer
                winnerColor = orange;
            }

            SDL_Texture* winnerTex = renderText(winnerText, font, winnerColor, renderer);
            
            if (tiedTeams.size() > 1) {
            string tieDetails = tiedTeams[0].name;

            for (int i = 1; i < tiedTeams.size(); i++) {
                tieDetails += " and " + tiedTeams[i].name;
            }

            tieDetails += " tied with " + to_string(topScore) + " points";

            SDL_Texture* tieTex = renderTextWrapped(tieDetails, font, {255, 255, 255, 255}, renderer, (int)(winW * 0.7));
            SDL_QueryTexture(tieTex, nullptr, nullptr, &tw, &th);

            SDL_Rect tieRect = { (winW - tw)/2, (int)(winH * 0.32), tw, th };
            SDL_RenderCopy(renderer, tieTex, nullptr, &tieRect);
            SDL_DestroyTexture(tieTex);
        }
            SDL_QueryTexture(winnerTex, nullptr, nullptr, &tw, &th);
            SDL_Rect winRect = { (winW - tw)/2, (int)(winH * 0.25), tw, th };
            SDL_RenderCopy(renderer, winnerTex, nullptr, &winRect);
            SDL_DestroyTexture(winnerTex);

            // Leaderboard
            for (int i = 0; i < sortedTeams.size(); i++) {
                string text = to_string(i + 1) + ". " +
                            sortedTeams[i].name + ": " +
                            to_string(sortedTeams[i].score);

                SDL_Color finalColor = {255, 255, 255, 255};

                for (int j = 0; j < teams.size(); j++) {
                    if (teams[j].name == sortedTeams[i].name) {
                        finalColor = teamColors[j];
                        break;
                    }
                }

                SDL_Texture* t = renderText(text, font, finalColor, renderer);
                SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                
                SDL_Rect rect = {
                    (winW - tw)/2,
                    (int)(winH * 0.4) + i * (th + 15),
                    tw,
                    th
                };

                SDL_RenderCopy(renderer, t, nullptr, &rect);
                SDL_DestroyTexture(t);
            }

            // Controls
            SDL_Texture* controls = renderText(
                "Press SPACE to Play Again or ESC to Quit",
                font, orange, renderer);

            SDL_QueryTexture(controls, nullptr, nullptr, &tw, &th);
            SDL_Rect cRect = { (winW - tw)/2, (int)(winH * 0.85), tw, th };

            SDL_RenderCopy(renderer, controls, nullptr, &cRect);
            SDL_DestroyTexture(controls);

            SDL_RenderPresent(renderer);
            continue;
        }
        // -------- Game Screen --------
        drawTeamRibbon(renderer, font, orange, winW);

        Question& q = questions[currentQuestion];

        // Cached question texture
        if (cachedQuestionIndex != currentQuestion) {
            if (cachedQuestionTex) {
                SDL_DestroyTexture(cachedQuestionTex);
                cachedQuestionTex = nullptr;
            }

            TTF_Font* qFont = getFittingFont(q.q, winW * 0.7, winH * 0.2);
            SDL_Color questionColor = {0, 0, 0, 255}; // ✅ BLACK TEXT
            cachedQuestionTex = renderTextWrapped(q.q, qFont, questionColor, renderer, winW * 0.7);
            TTF_CloseFont(qFont);
            cachedQuestionIndex = currentQuestion;
        }

        int w, h;
        SDL_QueryTexture(cachedQuestionTex, nullptr, nullptr, &w, &h);
        SDL_Rect qRect = { (winW - w) / 2, topBarHeight + margin, w, h };
        SDL_RenderCopy(renderer, cachedQuestionTex, nullptr, &qRect);

        // Options
        if (q.type == "MC") {
            for (int i = 0; i < q.options.size(); i++) {
                SDL_Color optionColor = {0, 0, 0, 255};
                int optionSpacing = winH * 0.08;
                SDL_Texture* opt = renderTextWrapped(q.options[i], font, optionColor, renderer, winW * 0.5);
                int startY = qRect.y + qRect.h + margin;
                int ow, oh;
                SDL_QueryTexture(opt, nullptr, nullptr, &ow, &oh);

                SDL_Rect r = { 
                    (int)(winW * 0.1), 
                    startY + i * optionSpacing, 
                    ow, 
                    oh 
                };
                SDL_RenderCopy(renderer, opt, nullptr, &r);
                SDL_DestroyTexture(opt);
            }
        }
        cout << "Loaded question: " << q.q << endl;
        cout << "Options count: " << q.options.size() << endl;

        // Question image
        if (!q.image.empty()) {
            SDL_Texture* img = IMG_LoadTexture(renderer, q.image.c_str());
            if (img) {
                SDL_Rect r = getCenteredImageRect(
                    img,
                    (int)(winW * 0.45),   // max width
                    (int)(winH * 0.35),   // max height
                    winW / 2,             // centered horizontally
                    (int)(winH * 0.72)    // lower part of question screen
                );

                SDL_RenderCopy(renderer, img, nullptr, &r);
                SDL_DestroyTexture(img);
            } else {
                cout << "Failed to load question image: " << q.image << endl;
                cout << "IMG_LoadTexture Error: " << IMG_GetError() << endl;
            }
        }

        // Timer
        int timeLeft = 0;
        if (timeLimit > 0 && timerRunning) {
            Uint32 elapsed = SDL_GetTicks() - questionStartTime;
            timeLeft = max(0, timeLimit - (int)elapsed);
        }
        int seconds = (timeLimit > 0) ? timeLeft / 1000 : 0;
        string timerText = (currentMode == FREE_PLAY) ? (timerRunning ? "Timer: Running" : "Timer: Paused") : "Time: " + to_string(seconds) + "s";

        SDL_Color timerColor = {0, 0, 0, 255}; // black
        SDL_Texture* timerTex = renderText(timerText, font, timerColor, renderer);
        int tw, th;
        SDL_QueryTexture(timerTex, nullptr, nullptr, &tw, &th);
        SDL_Rect timerRect = { winW - tw - margin, margin, tw, th };
        SDL_RenderCopy(renderer, timerTex, nullptr, &timerRect);
        SDL_DestroyTexture(timerTex);

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    if (cachedQuestionTex) SDL_DestroyTexture(cachedQuestionTex);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}

