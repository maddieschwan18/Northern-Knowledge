#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>

using namespace std;

void read_record()
{
    // File pointer
    fstream fin;

    // Open an existing file
    fin.open("questions.csv", ios::in);
    if (!fin.is_open()) {
    cout << "Error opening file\n";
    return;
}


    // Get the roll number
    // of which the data is required
    int count = 0;


    // Read the Data from the file
    // as String Vector
    vector<string> row;
    string line, word;
    bool haslines = true;

    while (haslines)
    {
        count = 1;

        row.clear();

        // read an entire row and
        // store it in a string variable 'line'
        //fin.ignore('\n');

        getline(fin, line);

        if (line.empty())
        {
            haslines = false;
        }

        // used for breaking words
        stringstream s(line);

        // read every column data of a row and
        // store it in a string variable, 'word'
        while (getline(s, word, ','))
        {
            // add all the column data
            // of a row to a vector
            row.push_back(word);
        }

        // Compare the roll number
        // Print the found data

        cout << row[0] << ", " << row [1] << "\n";
    }
    //if (count == 0)
        //cout << "questions not found\n";
    fin.close();
}

int main () {
    read_record();
    return 0;
}
