import sys
from typing import List, Tuple, Iterator
from collections import deque

class ABCDGameSolver:
    def __init__(self, board_str: str):
        """
        Initialize the solver with a board string
        
        :param board_str: Multi-line string representing the game board
        """
        # Split the string into rows and convert to list of lists
        self.board = [list(row) for row in board_str.strip().split('\n')]
        self.height = len(self.board)
        self.width = len(self.board[0])
    
    def _deep_copy_board(self, board):
        """
        Create a deep copy of the board
        
        :param board: Original board
        :return: Deep copy of the board
        """
        return [row.copy() for row in board]
    
    def _remove_connected_letters(self, board: List[List[str]], row: int, col: int) -> List[List[str]]:
        """
        Remove connected letters of the same type
        
        :param board: Current game board
        :param row: Row of the clicked cell
        :param col: Column of the clicked cell
        :return: Updated board after letter removal
        """
        letter = board[row][col]
        board[row][col] = '.'
        
        # Check and remove adjacent cells with same letter
        directions = [(0,1), (0,-1), (1,0), (-1,0)]
        for dx, dy in directions:
            new_row, new_col = row + dx, col + dy
            
            # Check bounds and matching letter
            if (0 <= new_row < self.height and 
                0 <= new_col < self.width and 
                board[new_row][new_col] == letter):
                board = self._remove_connected_letters(board, new_row, new_col)
        
        return board
    
    def _apply_gravity(self, board: List[List[str]]) -> List[List[str]]:
        """
        Apply gravity to the board, moving letters down
        
        :param board: Current game board
        :return: Updated board after gravity
        """
        # Iterate through each column
        for col in range(self.width):
            # Collect non-empty cells
            column = [board[row][col] for row in range(self.height) if board[row][col] != '.']
            
            # Pad with empty cells at the top
            column = ['.'] * (self.height - len(column)) + column
            
            # Update the column
            for row in range(self.height):
                board[row][col] = column[row]
        
        return board
    
    def _is_board_clear(self, board: List[List[str]]) -> bool:
        """
        Check if the board is completely cleared
        
        :param board: Current game board
        :return: True if board is clear, False otherwise
        """
        return all(cell == '.' for row in board for cell in row)
    
    def _get_cluster_points(self, board: List[List[str]]) -> List[Tuple[int, int]]:
        """
        Get one representative coordinate for each connected component
        of matching letters on the board to dramatically optimize search.
        """
        points = []
        b = [row.copy() for row in board]
        
        def remove(r: int, c: int, letter: str):
            b[r][c] = '.'
            directions = [(0,1), (0,-1), (1,0), (-1,0)]
            for dx, dy in directions:
                nr, nc = r + dx, c + dy
                if 0 <= nr < self.height and 0 <= nc < self.width and b[nr][nc] == letter:
                    remove(nr, nc, letter)
                    
        for r in range(self.height - 1, -1, -1):
            for c in range(self.width):
                letter = b[r][c]
                if letter == '.':
                    continue
                remove(r, c, letter)
                points.append((r, c))
                
        return points
    
    def solve(self, max_moves: int = 50) -> Iterator[List[Tuple[int, int]]]:
        """
        Find all solutions using breadth-first search
        
        :param max_moves: Maximum number of moves to explore
        :yield: List of moves to clear the board
        """
        # State representation: (board, moves, history)
        start_state = (self._deep_copy_board(self.board), [], set())
        queue = deque([start_state])
        visited_states = set()
        solutions = set()
        
        # Add initial state to visited
        initial_state = tuple(tuple(row) for row in self.board)
        visited_states.add(initial_state)
        
        while queue:
            current_board, current_moves, state_hash = queue.popleft()
            
            # Check if board is cleared
            if self._is_board_clear(current_board):
                # Convert moves to a hashable format for duplicate detection
                solution_key = tuple(current_moves)
                if solution_key not in solutions:
                    solutions.add(solution_key)
                    yield current_moves
                continue
            
            # Limit move depth
            if len(current_moves) >= max_moves:
                continue
            
            # Try clicking each cluster point
            for row, col in self._get_cluster_points(current_board):
                # Create a copy of the board and make a move
                new_board = self._deep_copy_board(current_board)
                new_board = self._remove_connected_letters(new_board, row, col)
                new_board = self._apply_gravity(new_board)
                
                # Create a hashable state representation
                board_state = tuple(tuple(row) for row in new_board)
                
                # Avoid revisiting states
                if board_state in visited_states:
                    continue
                
                new_moves = current_moves + [(row, col)]
                visited_states.add(board_state)
                queue.append((new_board, new_moves, board_state))

def main():
    # Attempt to read board from file argument or standard input
    if len(sys.argv) > 1:
        with open(sys.argv[1], 'r') as f:
            board_str = f.read()
    elif not sys.stdin.isatty():
        board_str = sys.stdin.read()
    else:
        # Example board from the provided string if no input given
        board_str = """CAACBBA
CCCBBCB
CAADDAD
CADDDDD
DDDBCCC
ADDDCDD
BDCBBDC
ACBBCDD
BCAACAD"""
    
    solver = ABCDGameSolver(board_str)
    
    print("Initial Board:")
    for row in solver.board:
        print(' '.join(row))
    print()
    
    for solution in solver.solve():
        print(f"\nSolution with {len(solution)} moves:")
        print(" ".join([f"{row},{col}" for row, col in solution]))
        # Since BFS guarantees finding the shortest solution first, we can exit early.
        break

if __name__ == "__main__":
    main()