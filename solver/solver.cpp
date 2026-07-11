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

    struct astar_node_t
    {
        board_t board;
        moves_t moves;
        int rating;
        
        bool operator>(astar_node_t const& other) const
        {
            return rating > other.rating;
        }
    };

    int count_letters(board_t const& b) const
    {
        int count = 0;
        for (const auto& row : b)
        {
            for (char cell : row)
            {
                if (cell != EMPTY)
                    ++count;
            }
        }
        return count;
    }

    void solve_astar_clustered()
    {
        moves.clear();
        std::priority_queue<astar_node_t, std::vector<astar_node_t>, std::greater<astar_node_t>> pq;
        std::unordered_map<board_t, int, board_hash, board_equal> visited;
        std::unordered_set<moves_t, moves_hash, moves_equal> unique_solutions;
        
        int initial_count = count_letters(board);
        pq.push(astar_node_t{board, {}, initial_count});
        visited[board] = 0;
        
        int optimal_length = -1;
        
        while (!pq.empty())
        {
            astar_node_t current = pq.top();
            pq.pop();
            
            int current_g = current.moves.size();
            
            // If we already found an optimal solution and are now exploring deeper levels, we can stop
            if (optimal_length != -1 && current_g > optimal_length)
            {
                break;
            }
            
            if (is_clear(current.board))
            {
                if (optimal_length == -1)
                {
                    optimal_length = current_g;
                }
                
                if (current_g == optimal_length)
                {
                    if (unique_solutions.insert(current.moves).second)
                    {
                        if (result_callback)
                        {
                            result_callback(current.moves);
                        }
                    }
                }
                continue;
            }
            
            // For finding ALL optimal solutions, A* must allow visiting the same board at the SAME optimal depth.
            // But if we found a strictly shorter path to this board already, we must skip.
            if (visited.count(current.board) && visited[current.board] < current_g)
            {
                continue;
            }
            
            // Temporary ABCD to get cluster points
            ABCD temp_game("");
            temp_game.board = current.board;
            
            for (auto [row, col] : temp_game.cluster_points())
            {
                if (temp_game.board_empty_at(row, col))
                    continue;
                
                ABCD new_game = temp_game;
                new_game.remove_letter(row, col);
                new_game.apply_gravity();
                
                int next_g = current_g + 1;
                // MUST allow equal 'next_g' to visit the same state through different paths since we want multiple solutions
                if (!visited.count(new_game.board) || visited[new_game.board] >= next_g)
                {
                    visited[new_game.board] = next_g;
                    
                    moves_t next_moves = current.moves;
                    next_moves.emplace_back(coord_t{row, col});
                    
                    // Standard admissibility score: 
                    // To find ALL solutions correctly we use rating = next_g * multiplier + remaining_letters
                    // With next_g * 10, remaining_letters acts as an tie-breaker which allows finding optimal solutions
                    // first, but doesn't delay visiting them so much that we run out of memory or time.
                    int rating = next_g * 4 + count_letters(new_game.board);
                    pq.push(astar_node_t{new_game.board, next_moves, rating});
                }
            }
        }
    }

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
        std::unordered_map<board_t, int, board_hash, board_equal> visited;
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
            
            auto it = visited.find(abcd.board);
            if (it != visited.end() && it->second <= (int)abcd.moves.size())
            {
                return;
            }
            visited[abcd.board] = abcd.moves.size();
            
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
    bool use_bfs = false;
    if (argc > 1)
    {
        std::string arg(argv[1]);
        if (arg == "--dfs")
        {
            use_dfs = true;
        }
        else if (arg == "--bfs")
        {
            use_bfs = true;
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
    else if (use_bfs)
    {
        abcd.solve_bfs_clustered();
    }
    else
    {
        abcd.solve_astar_clustered();
    }
    return 0;
}
