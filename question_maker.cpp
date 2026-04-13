#include <iostream>
#include <string>
#include <fstream>
#include <limits>

using namespace std;

void makeQuestion()
{
    fstream fout;
    fout.open("questions.csv", ios::out | ios::app);

    if (!fout.is_open()) {
        cout << "Error opening file!\n";
        return;
    }

    int typeChoice;
    cout << "\nSelect question type:";
    cout << "\n1 - Multiple Choice";
    cout << "\n2 - Free Answer";
    cout << "\n3 - Image Question";
    cout << "\nChoice: ";
    cin >> typeChoice;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string question, answer;
    cout << "\nProvide question: ";
    getline(cin, question);

    cout << "\nProvide answer: ";
    getline(cin, answer);

    cout << "Saving question...\n";

    if (typeChoice == 1)
    {
        string o1, o2, o3, o4;

        cout << "Option 1: "; getline(cin, o1);
        cout << "Option 2: "; getline(cin, o2);
        cout << "Option 3: "; getline(cin, o3);
        cout << "Option 4: "; getline(cin, o4);

        fout << "MC," << question << "," << answer << ","
             << o1 << "," << o2 << "," << o3 << "," << o4 << "\n";
    }
    else if (typeChoice == 2)
    {
        fout << "FA," << question << "," << answer << "\n";
    }
    else if (typeChoice == 3)
    {
        string imagePath;
        cout << "Enter image file path: ";
        getline(cin, imagePath);

        fout << "IMG," << question << "," << answer << ",,,,,"
             << imagePath << "\n";
    }

    fout.flush();
    fout.close();
}

void ui () {
    bool makingQuestions = true;
    int playerChoice;

    while (makingQuestions == true)
    {
        cout << "\ndo you want to make a question? 1 for yes, 0 for no";
        cin >> playerChoice;

        if (playerChoice == 0)
        makingQuestions = false;
        else if (playerChoice == 1)
        {
            makeQuestion();
        }
        else
        cout << "\ninvalid input";
    }
}

int main () {
    ui();
    return 0;
}
