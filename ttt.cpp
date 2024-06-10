#include <iostream>
#include <string>
#include <vector>

using namespace std;

bool valid(int sequence);
void printBoard(char gameboard[3][3]);
int playerTurn(int &location, int &row, int &col, char gameboard[3][3], int &retFlag);
int reverse(int sequence);
bool gamefinished(char gameboard[3][3]);
void alienTurn(int &newSequence, char gameboard[3][3]);
void winning(char alien);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <sequence>" << endl;
        return 1;
    }
    int sequence = atoi(argv[1]);

    if (!valid(sequence))
    {
        cerr << "Invalid sequence, Error" << endl;
        return 1;
    }
    int newSequence = reverse(sequence);
    char gameboard[3][3] = {{'0', '0', '0'}, {'0', '0', '0'}, {'0', '0', '0'}};

    int row, col, location;
    for (int i = 0; i < 5; i++)
    {
        alienTurn(newSequence, gameboard);
        if (gamefinished(gameboard))
            return 0;

        if (i == 4)
            break;

        int retFlag;
        int retVal = playerTurn(location, row, col, gameboard, retFlag);
        if (retFlag == 3)
            continue;
        if (retFlag == 1)
            return retVal;

        if (gamefinished(gameboard))
            return 0;
    }

    cout << "It's a draw!" << endl;
    return 0;
}

void alienTurn(int &newSequence, char gameboard[3][3])
{
    int location = newSequence % 10;
    newSequence /= 10;
    int row = (location - 1) / 3;
    int col = (location - 1) % 3;
    if (gameboard[row][col] != '0')
    {
        // Location is occupied, choose the next location from the sequence
        alienTurn(newSequence, gameboard);
        return;
    }
    gameboard[row][col] = 'X';
    cout << "Alien chose location " << location << endl;
    printBoard(gameboard);
}

int playerTurn(int &location, int &row, int &col, char gameboard[3][3], int &retFlag)
{
    retFlag = 1;
    while (true)
    {
        cout << "Choose a location (number between 1 to 9): ";
        cin >> location;
        if (location < 1 || location > 9)
        {
            cout << "Invalid location, try again" << endl;
            retFlag = 3;
            continue;
        }
        row = (location - 1) / 3;
        col = (location - 1) % 3;
        if (gameboard[row][col] != '0')
        {
            cout << "Cell already occupied, try again" << endl;
            retFlag = 3;
            continue;
        }
        break;
    }
    gameboard[row][col] = 'O';
    printBoard(gameboard);
    retFlag = 0;
    return 0;
}

void printBoard(char gameboard[3][3])
{
    for (int i = 0; i < 3; i++)
    {
        cout << " ";
        for (int j = 0; j < 3; j++)
        {
            if (gameboard[i][j] == '0')
            {
                cout << " ";
            }
            else
            {
                cout << gameboard[i][j];
            }
            if (j < 2)
            {
                cout << " | ";
            }
        }
        cout << endl;
        if (i < 2)
        {
            cout << "---|---|---" << endl;
        }
    }
    cout << endl
         << endl;
}

bool valid(int sequence)
{
    if (sequence < 123456789 || sequence > 987654321)
    {
        return false;
    }

    bool digits[9] = {false};
    for (int i = 0; i < 9; i++)
    {
        int digit = sequence % 10;
        if (digit == 0)
        {
            return false;
        }
        if (digits[digit - 1])
        {
            return false;
        }
        digits[digit - 1] = true;
        sequence /= 10;
    }

    return true;
}

int reverse(int sequence)
{
    int newSequence = 0;
    while (sequence > 0)
    {
        newSequence = newSequence * 10 + sequence % 10;
        sequence /= 10;
    }
    return newSequence;
}

bool gamefinished(char gameboard[3][3])
{
    for (int i = 0; i < 3; i++)
    {
        if (gameboard[i][0] == gameboard[i][1] && gameboard[i][1] == gameboard[i][2] && gameboard[i][0] != '0')
        {
            winning(gameboard[i][0]);
            return true;
        }
        if (gameboard[0][i] == gameboard[1][i] && gameboard[1][i] == gameboard[2][i] && gameboard[0][i] != '0')
        {
            winning(gameboard[0][i]);
            return true;
        }
    }

    if (gameboard[0][0] == gameboard[1][1] && gameboard[1][1] == gameboard[2][2] && gameboard[0][0] != '0')
    {
        winning(gameboard[0][0]);
        return true;
    }

    if (gameboard[0][2] == gameboard[1][1] && gameboard[1][1] == gameboard[2][0] && gameboard[0][2] != '0')
    {
        winning(gameboard[0][2]);
        return true;
    }

    return false;
}

void winning(char alien)
{
    if (alien == 'X')
    {
        cout << "I Win" << endl;
    }
    else
    {
        cout << "I lost" << endl;
    }
}