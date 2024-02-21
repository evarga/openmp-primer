#include <algorithm>
#include <climits>
#include <forward_list>
#include <random>

#ifdef _OPENMP
    #include <omp.h>
#else
    #define omp_get_max_threads() 1
#endif

// We seed the generator with some fixed value to ensure reproducibility.
mt19937 rnd(15);

// Solves the problem using a greedy approach with randomization. The key for this
// approach is a flexible and customizable heuristic function (see 'candidate' below).
vector<Cell> solveChunk(const int n, const int m, GridConfiguration &grid, const EngineMode mode) {
    // Stores the number of walls surrounding each available (free) cell.
    GridStatistics walls(n + 2, vector<int>(m + 2));
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            for (int k = 0; k < 4; k++)
                walls[i][j] += grid[i + dy[k]][j + dx[k]] == wall;

    vector<Cell> result;
    // Stores the number of flooded neighbors for each free cell.
    GridStatistics floodedNeighbors(n + 2, vector<int>(m + 2));
    // Stores cells whose neighbors may flood them, so they are good starting points for expanding
    // the search space.
    forward_list<Cell> candidates;

    /*
     * This function simulates flooding of cells using the Flood Fill algorithm.
     * See https://www.geeksforgeeks.org/flood-fill-algorithm/
     *
     * The dryRun parameter enables simulations to happen on the grid without
     * changing it.
     */
    auto flood = [&] (const int row, const int col, const bool dryRun = true) {
        vector<Cell> backlog;
        int front = 0;
        int total = 0;

        if (dryRun and mode == fast) {
            // Just count how many direct neighbors will be flooded if we flood the given cell.
            for (int i = 0; i < 4; i++)
                if (grid[row + dy[i]][col + dx[i]] == available and floodedNeighbors[row + dy[i]][col + dx[i]] == 1)
                    total++;
            return total;
        }

        // Registers cells that becomes flooded if we flood the specified cell.
        auto addNeighbors = [&] (const Cell &cell) {
            for (int i = 0; i < 4; i++) {
                const int nextRow = cell.first + dy[i];
                const int nextCol = cell.second + dx[i];
                if ((nextRow != row or nextCol != col) and grid[nextRow][nextCol] == available) {
                    floodedNeighbors[nextRow][nextCol]++;
                    if (floodedNeighbors[nextRow][nextCol] == 2)
                        backlog.emplace_back(nextRow, nextCol);
                    else if (!dryRun and floodedNeighbors[nextRow][nextCol] == 1)
                        candidates.emplace_front(nextRow, nextCol);
                }
            }
        };

        backlog.emplace_back(row, col);
        if (!dryRun)
            result.emplace_back(row, col);

        while (front < backlog.size()) {
            const auto cell = backlog[front++];
            total++;
            addNeighbors(cell);
            if (!dryRun)
                grid[cell.first][cell.second] = flooded;
        }

        // In dry run mode we need to revert the changes made to the floodedNeighbors data structure.
        if (dryRun)
            for (const auto &cell: backlog)
                for (int i = 0; i < 4; i++) {
                    const int nextRow = cell.first + dy[i];
                    const int nextCol = cell.second + dx[i];
                    if ((nextRow != row or nextCol != col) and grid[nextRow][nextCol] == available)
                        --floodedNeighbors[nextRow][nextCol];
                }
        return total;
    };

    // Create a list of available cells. We know that we have done our job if all of them become flooded.
    vector<Cell> cells;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            if (grid[i][j] == available)
                cells.emplace_back(i, j);

    // Randomize the order of cells to simulate various isometric transformations of the grid.
    shuffle(cells.begin(), cells.end(), rnd);

    /* 
     * Sort the cells based upon a simplified heuristic (an advanced version is used in the
     * 'candidate' function below), since at this moment we have little information about
     * the cells in the grid. Whenever we need to pick a cell to process a new connected component
     * we will select the one with the highest score. This is achieved by taking elements
     * from the back of this list.
     *
     * We must use a stable sorting algorithm to ensure that we don't diminish the effect
     * of the previous shuffling.
     */
    stable_sort(cells.begin(), cells.end(), [&] (const Cell &a, const Cell &b) {
        return walls[a.first][a.second] < walls[b.first][b.second];
    });

    /*
     * Each candidate is prioritized based upon the following weighted features:
     *
     * 1. Number of cells that can be additionally flooded if we flood the candidate.
     * 2. Number of flooded neighbors (less is better).
     * 3. Number of walls surrounding the candidate. Border cells are overall better
     *    candidates.
     * 4. Random jitter that breaks ties and introduces additional variability.
     *
     * The heuristic depends on the engine mode. In fast mode we put more emphasis on
     * avoiding cells that have many flooded neighbors, while in regular mode we put
     * more emphasis on the number of cells that can be additionally flooded.
     */
    typedef pair<int,Cell> ScoredCandidate;

    auto candidate = [&] (const int row, const int col) {
        if (mode == regular)
            return ScoredCandidate(1000 * flood(row, col)
                                   - 100 * floodedNeighbors[row][col]
                                   + 10 * walls[row][col]
                                   + rnd() % 10,
                    Cell(row, col));
        return ScoredCandidate(-100000 * floodedNeighbors[row][col]
                               + 1000 * flood(row, col)
                               + 100 * walls[row][col]
                               + rnd() % 100,
                    Cell(row, col));
    };

    while (!cells.empty()) {
        const auto cell = cells.back();

        if (candidates.empty() or walls[cell.first][cell.second] >= 3) {
            // It is crucial to pick a good starting cell for a new connected component.
            // We do this by selecting a cell surrounded by a largest number of walls.
            // This is achieved by taking elements from the back of the previously sorted list.
            cells.pop_back();
            if (grid[cell.first][cell.second] == available)
                flood(cell.first, cell.second, false);
        } else {
            ScoredCandidate bestCandidate = ScoredCandidate(INT_MIN, Cell(0, 0));
            for (auto prev = candidates.before_begin(), curr = candidates.begin(); curr != candidates.end();) {
                const int row = curr->first;
                const int col = curr->second;

                if (grid[row][col] == flooded) {
                    curr++;
                    candidates.erase_after(prev);
                } else {
                    for (int i = 0; i < 4; i++) {
                        const int nextRow = row + dy[i];
                        const int nextCol = col + dx[i];
                        if (grid[nextRow][nextCol] == available)
                            bestCandidate = max(bestCandidate, candidate(nextRow, nextCol));
                    }
                    prev = curr++;
                }
            }

            if (bestCandidate.first > INT_MIN) {
                const auto cell = bestCandidate.second;
                flood(cell.first, cell.second, false);
            }
        }
    }    
    return result;
}

// Performs chunking as necessary and aggregates partial results.
vector<Cell> solve(const int n, const int m, const GridConfiguration &grid, const EngineMode mode) {
    // We are handling blocks with dimensions of chunkSize x chunkSize.
    const int chunkSize = (n <= 1000 and m <= 1000) ? 1000 : 300;
    vector<Cell> result;

    #pragma omp parallel if (chunkSize < 1000)
    #pragma omp for schedule(static) collapse(2)
    for (int i = 1; i <= n; i += chunkSize)
        for (int j = 1; j <= m; j += chunkSize) {
            const int bottom = min(i + chunkSize - 1, n);
            const int right = min(j + chunkSize - 1, m);

            GridConfiguration gridChunk(chunkSize + 2, vector<CellState>(chunkSize + 2));
            for (int k = i; k <= bottom; k++)
                for (int l = j; l <= right; l++)
                    gridChunk[k - i + 1][l - j + 1] = grid[k][l];

            auto partialResult = solveChunk(chunkSize, chunkSize, gridChunk, mode);
            for (auto &cell: partialResult) {
                cell.first += i - 1;
                cell.second += j - 1;
            }
            #pragma omp critical(update_result)
            result.insert(result.end(), partialResult.cbegin(), partialResult.cend());
        }
    return result;
}
