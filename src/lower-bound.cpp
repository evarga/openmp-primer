#include <cmath>

// Performs a DFS traversal of the grid and counts the number of walls around visited cells.
int traverse(int row, int col, GridConfiguration &grid) {
    // We cannot use recursion here because the grid can be very large.
    vector<Cell> cells;
    cells.emplace_back(row, col);
    grid[row][col] = visited;
    int total = 0;

    while (!cells.empty()) {
        const Cell cell = cells.back();
        cells.pop_back();
        row = cell.first;
        col = cell.second;

        for (int i = 0; i < 4; i++) {
            const int nextRow = row + dy[i];
            const int nextCol = col + dx[i];
            if (grid[nextRow][nextCol] == available) {
                grid[nextRow][nextCol] = visited;
                cells.emplace_back(nextRow, nextCol);
            } else if (grid[nextRow][nextCol] == wall)
                total++;
        }
    }
    return total;
}

/*
 * Finds the theoretical lower bound for the required number of seeded cells.
 * Each connected component of cells is handled separately and their lower
 * bounds are summed up for the whole grid.
 *
 * A free cell can receive a contribution from max. 4 neighbors to become flooded.
 * A wall reduces such contribution. For example, a cell surrounded by 3 walls can
 * emit 25% of its contribution to its sole neighbor. Looking from the opposite
 * direction, the walls will absorb 75% of its contribution efficacy.
 *
 * The lower bound is calculated under an assumption that all contributions
 * from flooded cells would be leveraged in a solution. Of course, this is hardly
 * attainable in complex grids.
 */
int findLowerBound(const int n, const int m, GridConfiguration grid) {
    int total = 0;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            if (grid[i][j] == available)
                total += ceil(traverse(i, j, grid) / 4.0f);
    return total;
}
