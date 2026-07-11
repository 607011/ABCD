#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <queue>
#include <string>
#include <unordered_set>
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

struct ABCD;

struct board_hash
{
    size_t operator()(board_t const& board) const
    {
        size_t seed = board.size();
        for (const auto& row : board)
        {
            for (char cell : row)
            {
                seed ^= cell + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
        }
        return seed;
    }
};

struct board_equal
{
    bool operator()(board_t const& a, board_t const& b) const
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a.at(i) != b.at(i))
                return false;
        }
        return true;
    }
};

struct moves_hash
{
    size_t operator()(moves_t const& moves) const
    {
        size_t seed = moves.size();
        for (const auto& move : moves)
        {
            seed ^= move.col + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= move.row + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

struct moves_equal
{
    bool operator()(moves_t const& a, moves_t const& b) const
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a.at(i).col != b.at(i).col)
                return false;
            if (a.at(i).row != b.at(i).row)
                return false;
        }
        return true;
    }
};

struct ABCD
{
    board_t board;
    moves_t moves;

    static constexpr int max_moves{50};
    static constexpr char EMPTY = '.';

    static std::function<void(moves_t const&, bool&)> stats_callback;
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

    std::vector<coord_t> cluster_points() const
    {
        std::vector<coord_t> points;
        board_t b = board;
        char letter;
        std::function<void(int, int)> remove = [&](int r, int c) {
            if (b.at(r).at(c) == letter)
            {
                b[r][c] = EMPTY;
                if (r > 0)
                    remove(r - 1, c);
                if (r < (int)b.size() - 1)
                    remove(r + 1, c);
                if (c > 0)
                    remove(r, c - 1);
                if (c < (int)b[0].size() - 1)
                    remove(r, c + 1);
            }
        };
        for (int row = (int)b.size() - 1; row >= 0; --row)
        {
            for (int col = 0; col < (int)b.at(0).size(); ++col)
            {
                letter = b.at(row).at(col);
                if (letter == EMPTY)
                    continue;
                remove(row, col);
                points.emplace_back(coord_t{row, col});
            }
        }
        return points;
    }

    struct state_t
    {
        board_t board;
        moves_t moves;
    };

    void solve_bfs_clustered()
    {
        moves.clear();
        std::queue<ABCD> queue;
        queue.push(*this);
        std::unordered_set<board_t, board_hash, board_equal> visited;
        visited.insert(board);
        
        while (!queue.empty())
        {
            ABCD current_state = queue.front();
            queue.pop();
            
            if (current_state.is_clear())
            {
                if (result_callback)
                {
                    result_callback(current_state.moves);
                }
                break;
            }
            
            if (current_state.moves.size() >= max_moves)
            {
                continue;
            }
            
            for (auto [row, col] : current_state.cluster_points())
            {
                if (current_state.board_empty_at(row, col))
                    continue;
                ABCD new_game = current_state;
                new_game.remove_letter(row, col);
                new_game.apply_gravity();
                
                if (visited.insert(new_game.board).second)
                {
                    new_game.add_move(row, col);
                    queue.push(new_game);
                }
            }
        }
    }

    void solve_dfs_clustered()
    {
        moves.clear();
        std::function<void(ABCD const&)> solve_dfs = [&](ABCD const& abcd) {
            if (stats_callback)
            {
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
            for (auto [row, col] : abcd.cluster_points())
            {
                if (abcd.board_empty_at(row, col))
                    continue;
                ABCD new_game = abcd;
                new_game.remove_letter(row, col);
                new_game.apply_gravity();
                new_game.add_move(row, col);
                solve_dfs(new_game);
            }
        };
        solve_dfs(*this);
    }

    void solve_dfs()
    {
        moves.clear();
        std::function<void(ABCD const&)> solve_dfs = [&](ABCD const& abcd) {
            if (stats_callback)
            {
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
                    new_game.apply_gravity();
                    new_game.add_move(row, col);
                    solve_dfs(new_game);
                }
            }
        };
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

    static bool is_clear(board_t const& board)
    {
        return std::all_of(board.begin(), board.end(), [](const std::vector<char>& row) {
            return std::all_of(row.begin(), row.end(), [](char cell) { return cell == EMPTY; });
        });
    }

    bool is_clear() const
    {
        return is_clear(board);
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
            int empty_count = 0;
            for (int row = (int)board.size() - 1; row >= 0; --row)
            {
                if (board[row][col] == EMPTY)
                {
                    ++empty_count;
                }
                else if (empty_count > 0)
                {
                    board[row + empty_count][col] = board[row][col];
                    board[row][col] = EMPTY;
                }
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
    }
};

std::function<void(moves_t const&, bool&)> ABCD::stats_callback;
std::function<void(moves_t const&)> ABCD::result_callback;

int min_move_count = std::numeric_limits<int>::max();
moves_t min_moves = {};
unsigned long long num_tries = 0;

void display_stats(moves_t const& moves, bool& has_improved)
{
    has_improved = (int)moves.size() < min_move_count;
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
    if ((int)moves.size() < min_move_count)
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

int main(int argc, char* argv[])
{
    bool use_dfs = false;
    if (argc > 1)
    {
        std::string arg(argv[1]);
        if (arg == "--dfs")
        {
            use_dfs = true;
        }
    }
    std::string board_data;
    for (std::string line; std::getline(std::cin, line);)
    {
        board_data.append(line);
        board_data.append("\n");
    }
    std::cout.imbue(std::locale(std::locale::classic(), new thsds_numpunct));
    ABCD abcd(board_data);
    abcd.result_callback = display_result;
    abcd.stats_callback = display_stats;
    abcd.print();
    if (use_dfs)
    {
        abcd.solve_dfs_clustered();
    }
    else
    {
        abcd.solve_bfs_clustered();
    }
    return 0;
}
