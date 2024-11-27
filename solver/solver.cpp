#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <queue>
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

    static constexpr char EMPTY = '.';

    static std::function<void(moves_t const&, bool &)> stats_callback;
    static std::function<void(moves_t const&)> result_callback;

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
                if (r < (int)board.size() - 1)
                    remove(r + 1, c);
                if (c > 0)
                    remove(r, c - 1);
                if (c < (int)board[0].size() - 1)
                    remove(r, c + 1);
            }
        };
        remove(row, col);
        apply_gravity();
    }

    static void solve_dfs(ABCD const& abcd)
    {
        if (stats_callback) {
            bool has_improved;
            stats_callback(abcd.moves, has_improved);
            if (!has_improved)
                return;
        }
        if (abcd.is_clear())
        {
            if (result_callback)
                result_callback(abcd.moves);
            return;
        }
        for (int row = abcd.board.size() - 1; row >= 0; --row)
        {
            for (int col = 0; col < (int)abcd.board.at(row).size(); ++col)
            {
                if (abcd.board_empty_at(row, col))
                    continue;
                ABCD new_game = abcd;
                new_game.remove_letter(row, col);
                new_game.add_move(row, col);
                solve_dfs(new_game);
            }
        }
    }
};

std::function<void(moves_t const&, bool&)> ABCD::stats_callback;
std::function<void(moves_t const&)> ABCD::result_callback;

int min_move_count = std::numeric_limits<int>::max();
moves_t min_moves = {};
unsigned long long num_tries = 0;

void display_stats(moves_t const& moves, bool &has_improved)
{
    has_improved = moves.size() < min_move_count;
    if (!has_improved && ++num_tries % 125'000 == 0)
    {
        std::cout << "\r" << num_tries << " tries; ";
        for (auto const& move : moves)
        {
            std::cout << move.row << ',' << move.col << ' ';
        }
        std::cout << "\x1b[K" << std::flush;
    }
}
void display_result(moves_t const& moves)
{
    ++num_tries;
    if (moves.size() < min_move_count)
    {
        min_moves = moves;
        min_move_count = moves.size();
        std::cout << "\rboard is clear after " << min_move_count << " moves: ";
        for (auto const& move : min_moves)
        {
            std::cout << move.row << ',' << move.col << ' ';
        }
        std::cout << std::endl;
    }
}

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
    abcd.result_callback = display_result;
    abcd.stats_callback = display_stats;
    abcd.print();
    abcd.solve_dfs();
    return 0;
}
