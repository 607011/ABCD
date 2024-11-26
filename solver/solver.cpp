#include <algorithm>
#include <iostream>
#include <limits>
#include <locale>
#include <string>
#include <vector>

class thsds_numpunct : public std::numpunct<char>
{
  protected:
    virtual char do_thousands_sep() const
    {
        return ',';
    }
    virtual std::string do_grouping() const
    {
        return "\03";
    }
};

typedef struct
{
    int row;
    int col;
} coord_t;

typedef struct
{
    int row;
    int col;
    char c;
} cell_t;

using board_t = std::vector<std::vector<char>>;
using moves_t = std::vector<coord_t>;

struct ABCD
{
    board_t board;
    moves_t moves;
    static int min_move_count;
    static moves_t min_moves;
    static unsigned long long num_tries;

    static constexpr char EMPTY = '.';

    ABCD(std::string const& board_data)
    {
        std::vector<char> row_data;
        for (char cell : board_data)
        {
            if (cell != '\n')
            {
                row_data.push_back(cell);
            }
            else
            {
                board.push_back(row_data);
                row_data.clear();
            }
        }
    }

    ABCD(ABCD const& other) : board(other.board), moves(other.moves)
    {
    }

    ABCD& operator=(ABCD const& other)
    {
        board = other.board;
        moves = other.moves;
        return *this;
    }

    void print() const
    {
        print_board(board);
    }

    void solve_dfs()
    {
        moves.clear();
        min_move_count = std::numeric_limits<int>::max();
        num_tries = 0;
        solve_dfs(*this);
    }

    static void print_board(board_t const& board)
    {
        for (const auto& row : board)
        {
            for (char cell : row)
            {
                std::cout << cell;
            }
            std::cout << '\n';
        }
        std::cout << std::endl;
    }

    bool is_clear() const
    {
        return std::all_of(board.begin(), board.end(), [](const std::vector<char>& row) {
            return std::all_of(row.begin(), row.end(), [](char cell) { return cell == EMPTY; });
        });
    }

    inline void add_move(int row, int col)
    {
        moves.emplace_back(coord_t{row, col});
    }

    inline bool board_empty_at(int row, int col) const
    {
        return board.at(row).at(col) == EMPTY;
    }

    void apply_gravity()
    {
        for (int col = 0; col < (int)board.front().size(); ++col)
        {
            int index = (int)board.size() - 1;
            for (int row = (int)board.size() - 1; row >= 0; --row)
            {
                if (board[row][col] != EMPTY)
                {
                    board[index][col] = board[row][col];
                    --index;
                }
            }
            while (index >= 0)
            {
                board[index][col] = EMPTY;
                --index;
            }
        }
    }

    void remove_letter(int row, int col)
    {
        char letter = board.at(row).at(col);
        std::function<void(int, int)> remove = [&](int r, int c) {
            if (board.at(r).at(c) == letter)
            {
                board[r][c] = EMPTY;
                if (r > 0)
                    remove(r - 1, c);
                if (r < board.size() - 1)
                    remove(r + 1, c);
                if (c > 0)
                    remove(r, c - 1);
                if (c < board[0].size() - 1)
                    remove(r, c + 1);
            }
        };
        remove(row, col);
        apply_gravity();
    }

    static void solve_dfs(ABCD const& abcd, int move_count = 0)
    {
        auto display_stats = [&]() {
            std::cout << "\r" << num_tries << " tries; ";
            for (auto const& move : abcd.moves)
            {
                std::cout << move.row << ',' << move.col << ' ';
            }
            std::cout << "\x1b[K" << std::flush;
        };
        if (move_count >= min_move_count)
        {
            if (++num_tries % 125'000 == 0)
                display_stats();
            return;
        }
        if (abcd.is_clear())
        {
            ++num_tries;
            if (move_count < min_move_count)
            {
                min_moves = abcd.moves;
                min_move_count = move_count;
                std::cout << "\rboard is clear after " << move_count << " moves: ";
                for (auto const& move : min_moves)
                {
                    std::cout << move.row << ',' << move.col << ' ';
                }
                std::cout << std::endl;
            }
            else if (num_tries % 125'000 == 0)
            {
                display_stats();
            }
            return;
        }
        for (int row = abcd.board.size() - 1; row >= 0; --row)
        {
            for (int col = 0; col < abcd.board.at(row).size(); ++col)
            {
                if (abcd.board_empty_at(row, col))
                    continue;
                ABCD new_game = abcd;
                new_game.remove_letter(row, col);
                new_game.add_move(row, col);
                solve_dfs(new_game, move_count + 1);
            }
        }
    }

    void solve_bfs()
    {
        moves.clear();
        min_move_count = std::numeric_limits<int>::max();
        num_tries = 0;

        std::queue<std::pair<ABCD, int>> states_queue;
        states_queue.push({*this, 0});

        while (!states_queue.empty())
        {
            auto [current_state, move_count] = states_queue.front();
            states_queue.pop();
            ++num_tries;
            if (current_state.is_clear())
            {
                std::cout << "\rboard is clear after " << move_count << " moves: ";
                for (auto const& move : min_moves)
                {
                    std::cout << move.row << ',' << move.col << ' ';
                }
                return;
            }
            else if (num_tries % 1'000 == 0)
            {
                std::cout << "\r" << num_tries << " tries; ";
                for (auto const& move : current_state.moves)
                {
                    std::cout << move.row << ',' << move.col << ' ';
                }

                std::cout << "\x1b[K" << std::flush;
            }
            for (int row = current_state.board.size() - 1; row >= 0; --row)
            {
                for (int col = 0; col < current_state.board[row].size(); ++col)
                {
                    if (!current_state.board_empty_at(row, col))
                    {
                        ABCD new_state = current_state;
                        new_state.remove_letter(row, col);
                        new_state.add_move(row, col);
                        states_queue.push({new_state, move_count + 1});
                    }
                }
            }
        }

        std::cout << "No solution found.\n";
    }
};

moves_t ABCD::min_moves = {};
int ABCD::min_move_count = std::numeric_limits<int>::max();
unsigned long long ABCD::num_tries = 0;

int main()
{
    std::string const board_data = "AAACDCB\n"
                                   "DCABAAB\n"
                                   "DCCBCCB\n"
                                   "BAACBCB\n"
                                   "BABDDBC\n"
                                   "BDBBBDC\n"
                                   "BCCBAAB\n"
                                   "ACACDAC\n"
                                   "ABAAAAA\n";
    std::cout.imbue(std::locale(std::locale::classic(), new thsds_numpunct));
    ABCD abcd(board_data);
    abcd.print();
    abcd.solve_dfs();
    return 0;
}
