#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <locale>
#include <queue>
#include <string>
#include <unordered_set>
#include <unordered_map>
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

using board_t = std::vector<char>;
using moves_t = std::vector<coord_t>;

struct ABCD;

struct board_hash
{
    size_t operator()(board_t const& board) const
    {
        size_t seed = board.size();
        for (char cell : board)
        {
            seed ^= cell + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

struct board_equal
{
    bool operator()(board_t const& a, board_t const& b) const
    {
        return a == b;
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
    int height{0};
    int width{0};

    static constexpr int max_moves{50};
    static constexpr char EMPTY = '.';
    // Bounds the DFS transposition table so memory stays capped instead of growing with the
    // (potentially huge) number of distinct reachable boards; once full, new states just lose
    // the dedup optimization rather than being pruned incorrectly.
    static constexpr size_t max_visited_entries{3'000'000};

    static std::function<void(moves_t const&, bool&)> stats_callback;
    static std::function<void(moves_t const&)> result_callback;

    ABCD(std::string const& board_data)
    {
        int cols = 0;
        int current_cols = 0;
        for (char cell : board_data)
        {
            if (cell != '\r' && cell != '\n')
            {
                board.push_back(cell);
                current_cols++;
            }
            else if (current_cols > 0)
            {
                cols = current_cols;
                current_cols = 0;
            }
        }
        if (current_cols > 0)
        {
            cols = current_cols;
        }
        width = cols;
        if (width > 0)
        {
            height = board.size() / width;
        }
    }

    ABCD(ABCD const& other) : board(other.board), moves(other.moves), height(other.height), width(other.width)
    {
    }

    ABCD& operator=(ABCD const& other)
    {
        board = other.board;
        moves = other.moves;
        height = other.height;
        width = other.width;
        return *this;
    }

    void print() const
    {
        print_board(board, width);
    }

    std::vector<coord_t> cluster_points() const
    {
        std::vector<coord_t> points;
        board_t b = board;
        char letter;
        std::function<void(int, int)> remove = [&](int r, int c) {
            int idx = r * width + c;
            if (b.at(idx) == letter)
            {
                b[idx] = EMPTY;
                if (r > 0)
                    remove(r - 1, c);
                if (r < height - 1)
                    remove(r + 1, c);
                if (c > 0)
                    remove(r, c - 1);
                if (c < width - 1)
                    remove(r, c + 1);
            }
        };
        for (int row = height - 1; row >= 0; --row)
        {
            for (int col = 0; col < width; ++col)
            {
                letter = b.at(row * width + col);
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
        for (char cell : b)
        {
            if (cell != EMPTY)
                ++count;
        }
        return count;
    }

    void solve_astar_clustered()
    {
        moves.clear();
        std::priority_queue<astar_node_t, std::vector<astar_node_t>, std::greater<astar_node_t>> pq;
        std::unordered_map<board_t, std::unordered_map<int, std::unordered_set<int>>, board_hash, board_equal>
            visited_by_start;
        std::unordered_set<moves_t, moves_hash, moves_equal> unique_solutions;

        int initial_count = count_letters(board);
        pq.push(astar_node_t{board, {}, initial_count});

        int optimal_length = -1;

        while (!pq.empty())
        {
            astar_node_t current = pq.top();
            pq.pop();

            int current_g = current.moves.size();

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

            if (!current.moves.empty())
            {
                coord_t first_move = current.moves.front();

                auto& start_map = visited_by_start[current.board];
                if (start_map.count(first_move.row) && start_map[first_move.row].count(first_move.col))
                {
                    continue;
                }
                start_map[first_move.row].insert(first_move.col);
            }

            ABCD temp_game("");
            temp_game.board = current.board;
            temp_game.width = width;
            temp_game.height = height;

            for (auto [row, col] : temp_game.cluster_points())
            {
                if (temp_game.board_empty_at(row, col))
                    continue;

                ABCD new_game = temp_game;
                new_game.remove_letter(row, col);
                new_game.apply_gravity();

                int next_g = current_g + 1;

                moves_t next_moves = current.moves;
                next_moves.emplace_back(coord_t{row, col});

                int rating = next_g * 4 + count_letters(new_game.board);
                pq.push(astar_node_t{new_game.board, next_moves, rating});
            }
        }
    }

    // Parent points into the key storage of `visited` below, which is stable across inserts
    // (unordered_map only invalidates pointers/references to an element by erasing it, never
    // by inserting more elements) -- so each board is stored exactly once, as a map key,
    // instead of once in a "pool" vector and again in a separate "visited" set.
    struct bfs_node_t
    {
        board_t const* parent;
        coord_t last_move;
        int depth;
    };

    void solve_bfs_clustered()
    {
        moves.clear();
        std::unordered_map<board_t, bfs_node_t, board_hash, board_equal> visited;
        std::queue<board_t const*> q;

        auto [root_it, _] = visited.emplace(board, bfs_node_t{nullptr, coord_t{0, 0}, 0});
        q.push(&root_it->first);

        while (!q.empty())
        {
            board_t const* curr_board = q.front();
            q.pop();

            bfs_node_t const& current_node = visited.at(*curr_board);

            if (is_clear(*curr_board))
            {
                moves_t path;
                board_t const* p = curr_board;
                while (true)
                {
                    bfs_node_t const& n = visited.at(*p);
                    if (n.parent == nullptr)
                        break;
                    path.push_back(n.last_move);
                    p = n.parent;
                }
                std::reverse(path.begin(), path.end());

                if (result_callback)
                {
                    result_callback(path);
                }
                break;
            }

            if (current_node.depth >= max_moves)
            {
                continue;
            }

            ABCD temp_game("");
            temp_game.board = *curr_board;
            temp_game.width = width;
            temp_game.height = height;

            for (auto [row, col] : temp_game.cluster_points())
            {
                if (temp_game.board_empty_at(row, col))
                    continue;

                ABCD new_game = temp_game;
                new_game.remove_letter(row, col);
                new_game.apply_gravity();

                auto [it, inserted] =
                    visited.emplace(new_game.board, bfs_node_t{curr_board, coord_t{row, col}, current_node.depth + 1});
                if (inserted)
                {
                    q.push(&it->first);
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
            if (it != visited.end())
            {
                if (it->second <= (int)abcd.moves.size())
                {
                    return;
                }
                it->second = abcd.moves.size();
            }
            else if (visited.size() < max_visited_entries)
            {
                visited.emplace(abcd.board, (int)abcd.moves.size());
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

    static void print_board(board_t const& board, int w)
    {
        for (size_t i = 0; i < board.size(); ++i)
        {
            std::cout << board[i];
            if ((i + 1) % w == 0)
            {
                std::cout << '\n';
            }
        }
        std::cout << std::endl;
    }

    static bool is_clear(board_t const& board)
    {
        return std::all_of(board.begin(), board.end(), [](char cell) { return cell == EMPTY; });
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
        return board.at(row * width + col) == EMPTY;
    }

    void apply_gravity()
    {
        for (int col = 0; col < width; ++col)
        {
            int empty_count = 0;
            for (int row = height - 1; row >= 0; --row)
            {
                int idx = row * width + col;
                if (board[idx] == EMPTY)
                {
                    ++empty_count;
                }
                else if (empty_count > 0)
                {
                    board[(row + empty_count) * width + col] = board[idx];
                    board[idx] = EMPTY;
                }
            }
        }
    }

    void remove_letter(int row, int col)
    {
        char letter = board.at(row * width + col);
        std::function<void(int, int)> remove = [&](int r, int c) {
            int idx = r * width + c;
            if (board.at(idx) == letter)
            {
                board[idx] = EMPTY;
                if (r > 0)
                    remove(r - 1, c);
                if (r < height - 1)
                    remove(r + 1, c);
                if (c > 0)
                    remove(r, c - 1);
                if (c < width - 1)
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
