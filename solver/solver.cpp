#include <algorithm>
#include <chrono>
#include <deque>
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

    static std::function<void(moves_t const&, int, bool&)> stats_callback;
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

    struct cluster_t
    {
        coord_t point;
        int size;
    };

    // Like cluster_points(), but also reports each cluster's size -- needed by solve_ida_star()
    // to filter out single-cell clusters (not clickable under that ruleset) and to try the
    // largest clusters first.
    std::vector<cluster_t> clusters() const
    {
        std::vector<cluster_t> result;
        board_t b = board;
        char letter;
        int size;
        std::function<void(int, int)> remove = [&](int r, int c) {
            int idx = r * width + c;
            if (b.at(idx) == letter)
            {
                b[idx] = EMPTY;
                ++size;
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
                size = 0;
                remove(row, col);
                result.push_back(cluster_t{coord_t{row, col}, size});
            }
        }
        return result;
    }

    struct state_t
    {
        board_t board;
        moves_t moves;
    };

    // Parent points into path_pool (a std::deque, so pointers survive further push_back calls),
    // so a queued node's move history is a cheap pointer instead of a full moves_t copy per node.
    struct astar_path_node_t
    {
        astar_path_node_t const* parent;
        coord_t last_move;
        coord_t first_move;
        int depth;
    };

    struct astar_node_t
    {
        board_t board;
        astar_path_node_t const* path;
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

    // Admissible lower bound on the moves still needed to clear `b`: every move clears cells of
    // exactly one color, so at least one move per remaining distinct color is unavoidable (the
    // best case is that all cells of a color merge into a single cluster via gravity and go in
    // one move -- never fewer). Unlike count_letters, this never overestimates the true
    // remaining cost, so g + this bound is safe to prune on.
    int count_distinct_colors(board_t const& b) const
    {
        bool seen[256] = {false};
        int count = 0;
        for (char cell : b)
        {
            if (cell != EMPTY && !seen[(unsigned char)cell])
            {
                seen[(unsigned char)cell] = true;
                ++count;
            }
        }
        return count;
    }

    // IDA*: repeatedly do a depth-limited, branch-and-bound DFS with an increasing move-count
    // limit, starting at the admissible color-count lower bound. O(depth) memory -- no
    // transposition table at all, trading repeated work across iterations for the memory
    // blowup a full visited-state search would incur on large boards. Same clickability rule as
    // --dfs/--bfs/--astar (any non-empty cluster, including single cells); only the
    // largest-cluster-first move ordering needs cluster size.
    // The first depth_limit that admits any solution is, by construction, optimal (every
    // smaller depth_limit already searched exhaustively and failed) -- so that whole iteration
    // is run to completion instead of stopping at the first hit, to report every solution tied
    // at that length.
    void solve_ida_star()
    {
        moves.clear();
        int lower_bound = count_distinct_colors(board);
        unsigned long long nodes_visited = 0;
        bool found_any = false;

        std::function<void(ABCD const&, int, moves_t&)> search = [&](ABCD const& abcd, int depth_limit,
                                                                       moves_t& path) {
            ++nodes_visited;

            if (abcd.is_clear())
            {
                if ((int)path.size() == depth_limit)
                {
                    found_any = true;
                    if (result_callback)
                    {
                        result_callback(path);
                    }
                }
                return;
            }

            if ((int)path.size() + count_distinct_colors(abcd.board) > depth_limit)
            {
                return;
            }

            std::vector<cluster_t> candidates = abcd.clusters();
            std::sort(candidates.begin(), candidates.end(),
                      [](cluster_t const& a, cluster_t const& b) { return a.size > b.size; });

            for (auto const& cl : candidates)
            {
                ABCD new_game = abcd;
                new_game.remove_letter(cl.point.row, cl.point.col);
                new_game.apply_gravity();
                path.push_back(cl.point);
                search(new_game, depth_limit, path);
                path.pop_back();
            }
        };

        for (int depth_limit = lower_bound; depth_limit <= max_moves; ++depth_limit)
        {
            moves_t path;
            nodes_visited = 0;
            found_any = false;
            auto t0 = std::chrono::steady_clock::now();
            search(*this, depth_limit, path);
            double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            if (found_any)
            {
                return;
            }
            std::cout << "Tiefe " << depth_limit << ": keine Loesung (" << nodes_visited << " Knoten, " << elapsed
                       << "s)" << std::endl;
        }
    }

    void solve_astar_clustered()
    {
        moves.clear();
        std::priority_queue<astar_node_t, std::vector<astar_node_t>, std::greater<astar_node_t>> pq;
        std::unordered_map<board_t, std::unordered_map<int, std::unordered_set<int>>, board_hash, board_equal>
            visited_by_start;
        std::unordered_set<moves_t, moves_hash, moves_equal> unique_solutions;
        std::deque<astar_path_node_t> path_pool;

        int initial_count = count_letters(board);
        pq.push(astar_node_t{board, nullptr, initial_count});

        int optimal_length = -1;

        while (!pq.empty())
        {
            astar_node_t current = pq.top();
            pq.pop();

            int current_g = current.path ? current.path->depth : 0;

            if (optimal_length != -1 && current_g > optimal_length)
            {
                // Not `break`: rating = g*4 + remaining_letter_count is not an admissible bound
                // (one move can clear an arbitrarily large cluster), so a node that's still one
                // move away from a tied solution can rate worse than an unrelated dead end that's
                // already past optimal_length. Skip this dead end but keep draining the queue so
                // every node with g <= optimal_length still gets its turn.
                continue;
            }

            if (is_clear(current.board))
            {
                if (optimal_length == -1)
                {
                    optimal_length = current_g;
                }

                if (current_g == optimal_length)
                {
                    moves_t solution_moves;
                    for (astar_path_node_t const* p = current.path; p != nullptr; p = p->parent)
                    {
                        solution_moves.push_back(p->last_move);
                    }
                    std::reverse(solution_moves.begin(), solution_moves.end());

                    if (unique_solutions.insert(solution_moves).second)
                    {
                        if (result_callback)
                        {
                            result_callback(solution_moves);
                        }
                    }
                }
                continue;
            }

            if (current.path != nullptr)
            {
                coord_t first_move = current.path->first_move;

                auto it = visited_by_start.find(current.board);
                if (it != visited_by_start.end())
                {
                    auto& start_map = it->second;
                    if (start_map.count(first_move.row) && start_map[first_move.row].count(first_move.col))
                    {
                        continue;
                    }
                    start_map[first_move.row].insert(first_move.col);
                }
                else if (visited_by_start.size() < max_visited_entries)
                {
                    visited_by_start[current.board][first_move.row].insert(first_move.col);
                }
                // else: cap reached and this board isn't tracked yet -- just skip the dedup
                // optimization for it (may re-expand a (board, first move) pair redundantly,
                // but never produces a wrong or missed solution).
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
                coord_t move{row, col};
                coord_t first_move = current.path ? current.path->first_move : move;

                path_pool.push_back(astar_path_node_t{current.path, move, first_move, next_g});
                astar_path_node_t const* new_path = &path_pool.back();

                int rating = next_g * 4 + count_letters(new_game.board);
                pq.push(astar_node_t{new_game.board, new_path, rating});
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
        // Keyed by (board, first move), not board alone: two different opening moves that
        // happen to reach the same intermediate board must each keep exploring independently,
        // otherwise whichever one DFS visits first silently swallows every tied solution that
        // starts with the other opening move (same completeness issue solved for A* earlier).
        std::unordered_map<board_t, std::unordered_map<int, int>, board_hash, board_equal> visited;
        std::function<void(ABCD const&)> solve_dfs = [&](ABCD const& abcd) {
            if (stats_callback)
            {
                bool has_improved;
                stats_callback(abcd.moves, count_distinct_colors(abcd.board), has_improved);
                if (!has_improved)
                    return;
            }
            if (abcd.is_clear())
            {
                if (result_callback)
                    result_callback(abcd.moves);
                return;
            }

            int first_move_key =
                abcd.moves.empty() ? -1 : (abcd.moves.front().row * width + abcd.moves.front().col);

            auto it = visited.find(abcd.board);
            if (it != visited.end())
            {
                auto& by_first_move = it->second;
                auto it2 = by_first_move.find(first_move_key);
                if (it2 != by_first_move.end() && it2->second <= (int)abcd.moves.size())
                {
                    return;
                }
                by_first_move[first_move_key] = abcd.moves.size();
            }
            else if (visited.size() < max_visited_entries)
            {
                visited[abcd.board][first_move_key] = abcd.moves.size();
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

std::function<void(moves_t const&, int, bool&)> ABCD::stats_callback;
std::function<void(moves_t const&)> ABCD::result_callback;

int min_move_count = std::numeric_limits<int>::max();
std::vector<moves_t> optimal_solutions;
unsigned long long num_tries = 0;

void display_stats(moves_t const& moves, int lower_bound, bool& has_improved)
{
    // Branch-and-bound: prune as soon as the best possible completion from here (current depth
    // plus the admissible lower bound on remaining moves) can no longer match or beat the known
    // best. Uses <= (not <) so ties at exactly min_move_count still get explored far enough to
    // reach is_clear() and get recorded -- with strict <, DFS would silently stop reporting after
    // the very first optimal solution found.
    has_improved = (int)moves.size() + lower_bound <= min_move_count;
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
        min_move_count = moves.size();
        optimal_solutions.clear();
    }
    if ((int)moves.size() == min_move_count)
    {
        optimal_solutions.push_back(moves);
        std::cout << "\rboard is clear after " << min_move_count << " moves (solution #" << optimal_solutions.size()
                   << "): ";
        for (auto const& move : moves)
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
    bool use_astar = false;
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
        else if (arg == "--astar")
        {
            use_astar = true;
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
    else if (use_astar)
    {
        abcd.solve_astar_clustered();
    }
    else
    {
        abcd.solve_ida_star();
    }
    if (!optimal_solutions.empty())
    {
        std::cout << optimal_solutions.size() << " optimal solution(s) of length " << min_move_count << " found."
                   << std::endl;
    }
    return 0;
}
